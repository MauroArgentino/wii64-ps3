/**
 * ZSort.cpp - ZSort microcode for Mario Kart 64
 * Full implementation based on GLideN64's ZSort.cpp, adapted for wii64-ps3
 */

#include <string.h>
#include <math.h>
#include "ZSort.h"
#include "RSP.h"
#include "gDP.h"
#include "gSP.h"
#include "N64.h"
#include "RDP.h"
#include "GBI.h"
#include "F3D.h"
#include "convert.h"
#include "3DMath.h"

#ifdef PS3
#include <rsx/rsx.h>
#include <rsx/gcm_sys.h>
#include "../../main/rsxutil.h"
extern gcmContextData *context;
#endif

#define FIXED2FLOATRECIPCOLOR5	3.22580635547637939453125e-02f
#define FIXED2FLOATRECIPCOLOR7	7.8740157186985015869140625e-03f
#define FIXED2FLOATRECIPCOLOR8	3.9215688593685626983642578125e-03f

#define _FIXED2FLOATCOLOR( v, b ) \
	((f32)(v) * FIXED2FLOATRECIPCOLOR##b)

#define GZF_LOAD		0
#define GZF_SAVE		1

#define ZH_NULL		0
#define ZH_SHTRI	1
#define ZH_TXTRI	2
#define ZH_SHQUAD	3
#define ZH_TXQUAD	4

#define GZM_USER0		0
#define GZM_USER1		2
#define GZM_MMTX		4
#define GZM_PMTX		6
#define GZM_MPMTX		8
#define GZM_OTHERMODE	10
#define GZM_VIEWPORT	12

struct ZSortRDP zSortRdp = {{0, 0}, {0, 0}};

void ZSort_RDPCMD( u32 w0, u32 _w1 )
{
	(void)w0;
	u32 addr = RSP_SegmentToPhysical(_w1) >> 2;
	if (addr) {
		while(true)
		{
			u32 w0_cmd = ((u32*)RDRAM)[addr++];
			RSP.cmd = _SHIFTR( w0_cmd, 24, 8 );
			if (RSP.cmd == 0xDF)
				break;
			u32 w1 = ((u32*)RDRAM)[addr++];
			if (RSP.cmd == G_TEXRECT || RSP.cmd == G_TEXRECTFLIP) {
				addr++;
				addr++;
			}
			GBI.cmd[RSP.cmd]( w0_cmd, w1 );
		};
	}
}

int ZSort_Calc_invw( int _w )
{
	if (_w == 0)
		return 0x7FFFFFFF;
	return 0x7FFFFFFF / _w;
}

static
void ZSort_DrawObject( u8 * _addr, u32 _type )
{
	u32 textured = 0, vnum = 0, vsize = 0;
	switch (_type) {
	case ZH_NULL:
		textured = vnum = vsize = 0;
	break;
	case ZH_SHTRI:
		textured = 0;
		vnum = 3;
		vsize = 8;
	break;
	case ZH_TXTRI:
		textured = 1;
		vnum = 3;
		vsize = 16;
	break;
	case ZH_SHQUAD:
		textured = 0;
		vnum = 4;
		vsize = 8;
	break;
	case ZH_TXQUAD:
		textured = 1;
		vnum = 4;
		vsize = 16;
	break;
	}

	if (vnum == 0)
		return;

	u32 base = 72;
	u32 i;
	for (i = 0; i < vnum; i++) {
		SPVertex *vtx = &gSP.vertices[base + i];
		memset( vtx, 0, sizeof(SPVertex) );

		vtx->x = _FIXED2FLOAT( ((s16*)_addr)[0 ^ 1], 2 );
		vtx->y = _FIXED2FLOAT( ((s16*)_addr)[1 ^ 1], 2 );
		vtx->z = 0.0f;
		vtx->r = _addr[4^3] * 0.0039215689f;
		vtx->g = _addr[5^3] * 0.0039215689f;
		vtx->b = _addr[6^3] * 0.0039215689f;
		vtx->a = _addr[7^3] * 0.0039215689f;
		vtx->flag = 0;
		vtx->xClip = 0.0f;
		vtx->yClip = 0.0f;
		vtx->zClip = 0.0f;

		if (textured != 0) {
			if (gDP.otherMode.texturePersp != 0) {
				vtx->s = _FIXED2FLOAT( ((s16*)_addr)[4 ^ 1], 5 );
				vtx->t = _FIXED2FLOAT( ((s16*)_addr)[5 ^ 1], 5 );
			} else {
				vtx->s = _FIXED2FLOAT( ((s16*)_addr)[4 ^ 1], 6 );
				vtx->t = _FIXED2FLOAT( ((s16*)_addr)[5 ^ 1], 6 );
			}
			vtx->w = (f32)ZSort_Calc_invw( ((int*)_addr)[3] ) / 31.0f;
		} else {
			vtx->w = 1.0f;
		}

		_addr += vsize;
	}

	if (vnum == 3) {
		gSP1Triangle( base, base + 1, base + 2, 0 );
	} else if (vnum == 4) {
		gSP2Triangles( base, base + 1, base + 2, 0,
		                base, base + 2, base + 3, 0 );
	}
}

static
u32 ZSort_LoadObject( u32 _zHeader, u32 * _pRdpCmds )
{
	const u32 type = _zHeader & 7;
	u8 * addr = RDRAM + (_zHeader & 0xFFFFFFF8);
	u32 w1;
	switch (type) {
	case ZH_SHTRI:
	case ZH_SHQUAD:
	{
		w1 = ((u32*)addr)[1];
		if (w1 != _pRdpCmds[0]) {
			_pRdpCmds[0] = w1;
			ZSort_RDPCMD( 0, w1 );
		}
		ZSort_DrawObject( addr + 8, type );
	}
	break;
	case ZH_NULL:
	case ZH_TXTRI:
	case ZH_TXQUAD:
	{
		w1 = ((u32*)addr)[1];
		if (w1 != _pRdpCmds[0]) {
			_pRdpCmds[0] = w1;
			ZSort_RDPCMD( 0, w1 );
		}
		w1 = ((u32*)addr)[2];
		if (w1 != _pRdpCmds[1]) {
			ZSort_RDPCMD( 0, w1 );
			_pRdpCmds[1] = w1;
		}
		w1 = ((u32*)addr)[3];
		if (w1 != _pRdpCmds[2]) {
			ZSort_RDPCMD( 0, w1 );
			_pRdpCmds[2] = w1;
		}
		if (type != 0) {
			ZSort_DrawObject( addr + 16, type );
		}
	}
	break;
	}
	return RSP_SegmentToPhysical( ((u32*)addr)[0] );
}

void ZSort_Obj( u32 _w0, u32 _w1 )
{
	u32 rdpcmds[3] = {0, 0, 0};
	u32 cmd1 = _w1;
	u32 zHeader = RSP_SegmentToPhysical( _w0 );
	while (zHeader)
		zHeader = ZSort_LoadObject( zHeader, rdpcmds );
	zHeader = RSP_SegmentToPhysical( cmd1 );
	while (zHeader)
		zHeader = ZSort_LoadObject( zHeader, rdpcmds );
}

void ZSort_Interpolate( u32 w0, u32 w1 )
{
	(void)w0; (void)w1;
}

void ZSort_XFMLight( u32 _w0, u32 _w1 )
{
	int mid = _SHIFTR(_w0, 0, 8);
	gSPNumLights( 1 + _SHIFTR(_w1, 12, 8) );
	u32 addr = (u32)(-1024 + _SHIFTR(_w1, 0, 12));

	(void)mid;

	gSP.lights[gSP.numLights].r = _FIXED2FLOATCOLOR( DMEM[(addr+0)^3], 8 );
	gSP.lights[gSP.numLights].g = _FIXED2FLOATCOLOR( DMEM[(addr+1)^3], 8 );
	gSP.lights[gSP.numLights].b = _FIXED2FLOATCOLOR( DMEM[(addr+2)^3], 8 );
	addr += 8;

	u32 i;
	for (i = 0; i < (u32)gSP.numLights; i++) {
		gSP.lights[i].r = _FIXED2FLOATCOLOR( DMEM[(addr+0)^3], 8 );
		gSP.lights[i].g = _FIXED2FLOATCOLOR( DMEM[(addr+1)^3], 8 );
		gSP.lights[i].b = _FIXED2FLOATCOLOR( DMEM[(addr+2)^3], 8 );
		gSP.lights[i].x = (f32)(((s8*)DMEM)[(addr+8)^3]);
		gSP.lights[i].y = (f32)(((s8*)DMEM)[(addr+9)^3]);
		gSP.lights[i].z = (f32)(((s8*)DMEM)[(addr+10)^3]);
		addr += 24;
	}
}

void ZSort_LightingL( u32 w0, u32 w1 )
{
	(void)w0; (void)w1;
}

void ZSort_Lighting( u32 _w0, u32 _w1 )
{
	u32 csrs = (u32)(-1024 + _SHIFTR(_w0, 12, 12));
	u32 nsrs = (u32)(-1024 + _SHIFTR(_w0, 0, 12));
	u32 num = 1 + _SHIFTR(_w1, 24, 8);
	u32 cdest = (u32)(-1024 + _SHIFTR(_w1, 12, 12));
	int use_material = (csrs != 0x0ff0);
	u32 i;

	for (i = 0; i < num; i++) {
		f32 r = 0.5f, g = 0.5f, b = 0.5f, a = 1.0f;

		if (use_material) {
			r = _FIXED2FLOATCOLOR( DMEM[(csrs)^3], 8 );
			g = _FIXED2FLOATCOLOR( DMEM[(csrs+1)^3], 8 );
			b = _FIXED2FLOATCOLOR( DMEM[(csrs+2)^3], 8 );
			a = _FIXED2FLOATCOLOR( DMEM[(csrs+3)^3], 8 );
			csrs += 4;
		}

		DMEM[(cdest)^3] = (u8)(r * 255.0f);
		DMEM[(cdest+1)^3] = (u8)(g * 255.0f);
		DMEM[(cdest+2)^3] = (u8)(b * 255.0f);
		DMEM[(cdest+3)^3] = (u8)(a * 255.0f);
		cdest += 4;
		nsrs += 3;
	}
}

void ZSort_MTXRNSP( u32 w0, u32 w1 )
{
	(void)w0; (void)w1;
}

void ZSort_MTXCAT(u32 _w0, u32 _w1)
{
	u32 S = _SHIFTR(_w0, 0, 4);
	u32 T = _SHIFTR(_w1, 16, 4);
	u32 D = _SHIFTR(_w1, 0, 4);

	f32 s[4][4], t[4][4];

	switch (S) {
	case GZM_MMTX:
		memcpy( s, gSP.matrix.modelView[gSP.matrix.modelViewi], 64 );
	break;
	case GZM_PMTX:
		memcpy( s, gSP.matrix.projection, 64 );
	break;
	case GZM_MPMTX:
		memcpy( s, gSP.matrix.combined, 64 );
	break;
	default:
		return;
	}

	switch (T) {
	case GZM_MMTX:
		memcpy( t, gSP.matrix.modelView[gSP.matrix.modelViewi], 64 );
	break;
	case GZM_PMTX:
		memcpy( t, gSP.matrix.projection, 64 );
	break;
	case GZM_MPMTX:
		memcpy( t, gSP.matrix.combined, 64 );
	break;
	default:
		return;
	}

	MultMatrix( s, t );

	switch (D) {
	case GZM_MMTX:
		memcpy( gSP.matrix.modelView[gSP.matrix.modelViewi], s, 64 );
	break;
	case GZM_PMTX:
		memcpy( gSP.matrix.projection, s, 64 );
	break;
	case GZM_MPMTX:
		memcpy( gSP.matrix.combined, s, 64 );
	break;
	}
}

void ZSort_MultMPMTX( u32 _w0, u32 _w1 )
{
	int num = 1 + _SHIFTR(_w1, 24, 8);
	int src = (int)(-1024 + _SHIFTR(_w1, 12, 12));
	int dst = (int)(-1024 + _SHIFTR(_w1, 0, 12));
	s16 * saddr = (s16*)(DMEM + src);
	struct zSortVDest * daddr = (struct zSortVDest*)(DMEM + dst);
	int idx = 0;
	struct zSortVDest v;
	int i;

	memset( &v, 0, sizeof(v) );

	for (i = 0; i < num; i++) {
		s16 sx = saddr[(idx)^1];
		s16 sy = saddr[(idx+1)^1];
		s16 sz = saddr[(idx+2)^1];
		idx += 3;

		f32 x = (f32)sx*gSP.matrix.combined[0][0] + (f32)sy*gSP.matrix.combined[1][0] + (f32)sz*gSP.matrix.combined[2][0] + gSP.matrix.combined[3][0];
		f32 y = (f32)sx*gSP.matrix.combined[0][1] + (f32)sy*gSP.matrix.combined[1][1] + (f32)sz*gSP.matrix.combined[2][1] + gSP.matrix.combined[3][1];
		f32 z = (f32)sx*gSP.matrix.combined[0][2] + (f32)sy*gSP.matrix.combined[1][2] + (f32)sz*gSP.matrix.combined[2][2] + gSP.matrix.combined[3][2];
		f32 w = (f32)sx*gSP.matrix.combined[0][3] + (f32)sy*gSP.matrix.combined[1][3] + (f32)sz*gSP.matrix.combined[2][3] + gSP.matrix.combined[3][3];

		v.sx = (s16)(zSortRdp.view_trans[0] + x / w * zSortRdp.view_scale[0]);
		v.sy = (s16)(zSortRdp.view_trans[1] + y / w * zSortRdp.view_scale[1]);

		v.xi = (s16)x;
		v.yi = (s16)y;
		v.wi = (s16)w;
		v.invw = ZSort_Calc_invw( (int)(w * 31.0) );

		if (w < 0.0f)
			v.fog = 0;
		else {
			int fog = (int)(z / w * gSP.fog.multiplier + gSP.fog.offset);
			if (fog > 255)
				fog = 255;
			v.fog = (fog >= 0) ? (u8)fog : 0;
		}

		v.cc = 0;
		if (x < -w) v.cc |= 0x10;
		if (x > w) v.cc |= 0x01;
		if (y < -w) v.cc |= 0x20;
		if (y > w) v.cc |= 0x02;
		if (w < 0.1f) v.cc |= 0x04;

		daddr[i] = v;
	}
}

void ZSort_LinkSubDL( u32 w0, u32 w1 )
{
	(void)w0; (void)w1;
}

void ZSort_SetSubDL( u32 w0, u32 w1 )
{
	(void)w0; (void)w1;
}

void ZSort_WaitSignal( u32 w0, u32 w1 )
{
	(void)w0; (void)w1;
}

void ZSort_SendSignal( u32 w0, u32 w1 )
{
	(void)w0; (void)w1;
}

static
void ZSort_SetTexture()
{
	gSP.texture.scales = 1.0f;
	gSP.texture.scalet = 1.0f;
	gSP.texture.level = 0;
	gSP.texture.on = 1;
	gSP.texture.tile = 0;

	gSPSetGeometryMode( G_SHADING_SMOOTH | G_SHADE );
}

void ZSort_MoveMem( u32 _w0, u32 _w1 )
{
	int idx = _w0 & 0x0E;
	int ofs = _SHIFTR(_w0, 6, 9) << 3;
	int len = 1 + (_SHIFTR(_w0, 15, 9) << 3);
	int flag = _w0 & 0x01;
	u32 addr = RSP_SegmentToPhysical( _w1 );

	switch (idx) {
	case GZF_LOAD:
		if (flag == 0) {
			int dmem_addr = (idx << 3) + ofs;
			memcpy( DMEM + dmem_addr, RDRAM + addr, len );
		} else {
			int dmem_addr = (idx << 3) + ofs;
			memcpy( RDRAM + addr, DMEM + dmem_addr, len );
		}
	break;

	case GZM_MMTX:
		RSP_LoadMatrix( gSP.matrix.modelView[gSP.matrix.modelViewi], addr );
		gSP.changed |= CHANGED_MATRIX;
	break;

	case GZM_PMTX:
		RSP_LoadMatrix( gSP.matrix.projection, addr );
		gSP.changed |= CHANGED_MATRIX;
	break;

	case GZM_MPMTX:
		RSP_LoadMatrix( gSP.matrix.combined, addr );
		gSP.changed &= ~CHANGED_MATRIX;
	break;

	case GZM_OTHERMODE:
	break;

	case GZM_VIEWPORT:
	{
		u32 a = addr >> 1;
		const f32 scale_x = _FIXED2FLOAT( *(s16*)&RDRAM[(a+0)^1], 2 );
		const f32 scale_y = _FIXED2FLOAT( *(s16*)&RDRAM[(a+1)^1], 2 );
		const f32 scale_z = _FIXED2FLOAT( *(s16*)&RDRAM[(a+2)^1], 10 );
		const s16 fm = ((s16*)RDRAM)[(a+3)^1];
		const f32 trans_x = _FIXED2FLOAT( *(s16*)&RDRAM[(a+4)^1], 2 );
		const f32 trans_y = _FIXED2FLOAT( *(s16*)&RDRAM[(a+5)^1], 2 );
		const f32 trans_z = _FIXED2FLOAT( *(s16*)&RDRAM[(a+6)^1], 10 );
		const s16 fo = ((s16*)RDRAM)[(a+7)^1];
		gSPFogFactor( fm, fo );

		gSP.viewport.vscale[0] = scale_x;
		gSP.viewport.vscale[1] = scale_y;
		gSP.viewport.vscale[2] = scale_z;
		gSP.viewport.vtrans[0] = trans_x;
		gSP.viewport.vtrans[1] = trans_y;
		gSP.viewport.vtrans[2] = trans_z;

		gSP.viewport.x      = gSP.viewport.vtrans[0] - gSP.viewport.vscale[0];
		gSP.viewport.y      = gSP.viewport.vtrans[1] - gSP.viewport.vscale[1];
		gSP.viewport.width  = gSP.viewport.vscale[0] * 2;
		gSP.viewport.height = gSP.viewport.vscale[1] * 2;
		gSP.viewport.nearz  = gSP.viewport.vtrans[2] - gSP.viewport.vscale[2];
		gSP.viewport.farz   = (gSP.viewport.vtrans[2] + gSP.viewport.vscale[2]);

		zSortRdp.view_scale[0] = scale_x * 4.0f;
		zSortRdp.view_scale[1] = scale_y * 4.0f;
		zSortRdp.view_trans[0] = trans_x * 4.0f;
		zSortRdp.view_trans[1] = trans_y * 4.0f;

		gSP.changed |= CHANGED_VIEWPORT;

		ZSort_SetTexture();
	}
	break;
	}
}

void SZort_SetScissor( u32 _w0, u32 _w1 )
{
	gDPSetScissor( _SHIFTR(_w1, 24, 2),
		_FIXED2FLOAT( _SHIFTR(_w1, 12, 12 ), 2 ),
		_FIXED2FLOAT( _SHIFTR(_w1,  0, 12 ), 2 ),
		_FIXED2FLOAT( _SHIFTR(_w0, 12, 12 ), 2 ),
		_FIXED2FLOAT( _SHIFTR(_w0,  0, 12 ), 2 ) );

	if ((gDP.scissor.lrx - gDP.scissor.ulx) > (zSortRdp.view_scale[0] - zSortRdp.view_trans[0]))
	{
		f32 w = (gDP.scissor.lrx - gDP.scissor.ulx) / 2.0f;
		f32 h = (gDP.scissor.lry - gDP.scissor.uly) / 2.0f;

		gSP.viewport.vscale[0] = w;
		gSP.viewport.vscale[1] = h;
		gSP.viewport.vtrans[0] = w;
		gSP.viewport.vtrans[1] = h;

		gSP.viewport.x      = gSP.viewport.vtrans[0] - gSP.viewport.vscale[0];
		gSP.viewport.y      = gSP.viewport.vtrans[1] - gSP.viewport.vscale[1];
		gSP.viewport.width  = gSP.viewport.vscale[0] * 2;
		gSP.viewport.height = gSP.viewport.vscale[1] * 2;

		zSortRdp.view_scale[0] = w * 4.0f;
		zSortRdp.view_scale[1] = h * 4.0f;
		zSortRdp.view_trans[0] = w * 4.0f;
		zSortRdp.view_trans[1] = h * 4.0f;

		gSP.changed |= CHANGED_VIEWPORT;

		ZSort_SetTexture();
	}
}

void ZSort_Init()
{
	GBI_InitFlags( F3D );

	GBI.PCStackSize = 10;

	GBI_SetGBI( G_SPNOOP,				F3D_SPNOOP,				F3D_SPNoOp );
	GBI_SetGBI( G_RESERVED0,			F3D_RESERVED0,			F3D_Reserved0 );
	GBI_SetGBI( G_RESERVED1,			F3D_RESERVED1,			F3D_Reserved1 );
	GBI_SetGBI( G_DL,					0xDE,					F3D_DList );
	GBI_SetGBI( G_RESERVED2,			F3D_RESERVED2,			F3D_Reserved2 );
	GBI_SetGBI( G_RESERVED3,			F3D_RESERVED3,			F3D_Reserved3 );

	GBI_SetGBI( G_CULLDL,				F3D_CULLDL,				F3D_CullDL );
	GBI_SetGBI( G_MOVEWORD,				0xDB,					F3D_MoveWord );
	GBI_SetGBI( G_TEXTURE,				F3D_TEXTURE,			F3D_Texture );
	GBI_SetGBI( G_ZSETSCISSOR,			G_SETSCISSOR,			SZort_SetScissor );
	GBI_SetGBI( G_SETOTHERMODE_H,		0xE3,					F3D_SetOtherMode_H );
	GBI_SetGBI( G_SETOTHERMODE_L,		0xE2,					F3D_SetOtherMode_L );
	GBI_SetGBI( G_ENDDL,				0xDF,					F3D_EndDL );
	GBI_SetGBI( G_SETGEOMETRYMODE,		F3D_SETGEOMETRYMODE,	F3D_SetGeometryMode );
	GBI_SetGBI( G_CLEARGEOMETRYMODE,	F3D_CLEARGEOMETRYMODE,	F3D_ClearGeometryMode );
	GBI_SetGBI( G_RDPHALF_1,			F3D_RDPHALF_1,			F3D_RDPHalf_1 );
	GBI_SetGBI( G_RDPHALF_2,			F3D_RDPHALF_2,			F3D_RDPHalf_2 );
	GBI_SetGBI( G_RDPHALF_CONT,			F3D_RDPHALF_CONT,		F3D_RDPHalf_Cont );

	GBI_SetGBI( G_ZOBJ,				0x80,					ZSort_Obj );
	GBI_SetGBI( G_ZRDPCMD,				0x81,					ZSort_RDPCMD );
	GBI_SetGBI( G_MOVEMEM,				0xDC,					ZSort_MoveMem );
	GBI_SetGBI( G_ZSENDSIGNAL,			0xDA,					ZSort_SendSignal );
	GBI_SetGBI( G_ZWAITSIGNAL,			0xD9,					ZSort_WaitSignal );
	GBI_SetGBI( G_ZSETSUBDL,			0xD8,					ZSort_SetSubDL );
	GBI_SetGBI( G_ZLINKSUBDL,			0xD7,					ZSort_LinkSubDL );
	GBI_SetGBI( G_ZMULT_MPMTX,			0xD6,					ZSort_MultMPMTX );
	GBI_SetGBI( G_ZMTXCAT,				0xD5,					ZSort_MTXCAT );
	GBI_SetGBI( G_ZMTXTRNSP,			0xD4,					ZSort_MTXRNSP );
	GBI_SetGBI( G_ZLIGHTING_L,			0xD3,					ZSort_LightingL );
	GBI_SetGBI( G_ZLIGHTING,			0xD2,					ZSort_Lighting );
	GBI_SetGBI( G_ZXFMLIGHT,			0xD1,					ZSort_XFMLight );
	GBI_SetGBI( G_ZINTERPOLATE,		0xD0,					ZSort_Interpolate );
}
