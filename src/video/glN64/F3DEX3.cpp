/**
 * glN64_GX - F3DEX3.cpp
 * Copyright (C) 2003 Orkin
 *
 * F3DEX3: Conker's Bad Fur Day, Perfect Dark PAL, Mickey's Speedway USA
 * Extended F3DEX2 with tristrip/trifan/trisnake, ambient occlusion, Fresnel, attr offsets, RELSEGMENT
**/

#ifdef __GX__
#include <gccore.h>
#endif // __GX__

#include "glN64.h"
#include "Debug.h"
#include "F3D.h"
#include "F3DEX.h"
#include "F3DEX2.h"
#include "F3DEX3.h"
#include "N64.h"
#include "RSP.h"
#include "RDP.h"
#include "gSP.h"
#include "gDP.h"
#include "GBI.h"

static int f3dex3_version = 0;

int GBI_f3dex3Version()
{
	return f3dex3_version;
}

#define _LIGHT_TO_OFFSET(n) (((n) - 1) * 0x10 + 0x10)

static void writeLight( u32 off, u32 w )
{
	if (0 == off)
	{
		gSPCameraWorld( w );
	}
	if (0x8 == off)
	{
		gSPLookAt( w, 0 );
		gSPLookAt( w + 8, 1 );
	}

	for (int i = 1; i <= 10; i++)
	{
		if (_LIGHT_TO_OFFSET( i ) == off)
		{
			gSPLight( w, i );
		}
	}

	if ((F3DEX3_G_MAX_LIGHTS * 0x10) + 0x18 == off)
	{
		// OcclusionPlane not supported
	}
}

void F3DEX3_MoveMem( u32 w0, u32 w1 )
{
	switch (_SHIFTR( w0, 0, 8 ))
	{
		case F3DEX2_MV_VIEWPORT:
			gSPViewport( w1 );
			break;
		case G_MV_LIGHT:
		{
			u32 ofs = _SHIFTR( w0, 8, 8 ) * 8;
			u32 len = (1 + _SHIFTR( w0, 19, 5 )) * 8;
			for (u32 i = 0; i < len; i += 4)
			{
				writeLight( ofs + i, w1 + i );
			}
		}
		break;
	}
}

static void handleFX( u32 mwo, u16 what )
{
	switch (mwo)
	{
		case F3DEX3_G_MWO_AO_AMBIENT:
			// gsSPAOAmbient(what);
			break;
		case F3DEX3_G_MWO_AO_DIRECTIONAL:
			// gsSPAODirectional(what);
			break;
		case F3DEX3_G_MWO_AO_POINT:
			// gsSPAOPoint(what);
			break;
		case F3DEX3_G_MWO_PERSPNORM:
			gSPPerspNormalize( what );
			break;
		case F3DEX3_G_MWO_FRESNEL_SCALE:
			// gsSPFresnelScale(what);
			break;
		case F3DEX3_G_MWO_FRESNEL_OFFSET:
			// gsSPFresnelOffset(what);
			break;
		case F3DEX3_G_MWO_ATTR_OFFSET_S:
			// gsSPAttrOffsetS(what);
			break;
		case F3DEX3_G_MWO_ATTR_OFFSET_T:
			// gsSPAttrOffsetT(what);
			break;
		case F3DEX3_A_G_MWO_ATTR_OFFSET_Z:
			break;
		case F3DEX3_A_G_MWO_ALPHA_COMPARE_CULL:
			// gsSPAlphaCompareCull(what);
			break;
		case F3DEX3_A_G_MWO_NORMALS_MODE:
			break;
		case F3DEX3_A_G_MWO_LAST_MAT_DL_ADDR:
			break;
	}
}

void F3DEX3_MoveWord( u32 w0, u32 w1 )
{
	switch (_SHIFTR( w0, 16, 8 ))
	{
		case F3DEX3_G_MW_FX:
		{
			u32 value = _SHIFTR( w0, 0, 16 );
			u32 what = w1;
			bool half = value & F3DEX3_G_MW_HALFWORD_FLAG;
			u32 mwo = value & ~F3DEX3_G_MW_HALFWORD_FLAG;
			if (f3dex3_version > 0)
			{
				switch (mwo)
				{
					case F3DEX3_B_G_MWO_ALPHA_COMPARE_CULL:
						mwo = F3DEX3_A_G_MWO_ALPHA_COMPARE_CULL;
						break;
					case F3DEX3_B_G_MWO_LAST_MAT_DL_ADDR:
						mwo = F3DEX3_A_G_MWO_LAST_MAT_DL_ADDR;
						break;
				}
			}

			if (half)
			{
				handleFX( mwo, what & 0xffff );
			}
			else
			{
				handleFX( mwo + 0, (what >> 16) & 0xffff );
				handleFX( mwo + 2, what & 0xffff );
			}
		}
		break;
		case G_MW_NUMLIGHT:
			gSPNumLights( w1 / 0x10 );
			break;
		case G_MW_SEGMENT:
			gSPSegment( _SHIFTR( w0, 2, 4 ), w1 & 0x00FFFFFF );
			break;
		case G_MW_FOG:
			gSPFogFactor( (s16)_SHIFTR( w1, 16, 16 ), (s16)_SHIFTR( w1, 0, 16 ) );
			break;
		case G_MW_LIGHTCOL:
		{
			int off = _SHIFTR( w0, 0, 16 );
			gSPLightColor( (off / 0x10) + 1, w1 );
			break;
		}
	}
}

struct Vertices7
{
	u8 v[7];

	inline bool valid( u8 i ) const
	{
		return v[i] < 64;
	}
};

static inline Vertices7 unpackVertices7( u32 w0, u32 w1 )
{
	Vertices7 v;
	v.v[0] = _SHIFTR( w0, 17, 7 );
	v.v[1] = _SHIFTR( w0, 9, 7 );
	v.v[2] = _SHIFTR( w0, 1, 7 );
	v.v[3] = _SHIFTR( w1, 25, 7 );
	v.v[4] = _SHIFTR( w1, 17, 7 );
	v.v[5] = _SHIFTR( w1, 9, 7 );
	v.v[6] = _SHIFTR( w1, 1, 7 );
	return v;
}

void F3DEX3_TriStrip( u32 w0, u32 w1 )
{
	Vertices7 vertices = unpackVertices7( w0, w1 );
	if (!vertices.valid(0) || !vertices.valid(1) || !vertices.valid(2)) return;
	gSP1Triangle( vertices.v[0], vertices.v[1], vertices.v[2], 0 );

	if (!vertices.valid(3)) return;
	gSP1Triangle( vertices.v[2], vertices.v[1], vertices.v[3], 0 );

	if (!vertices.valid(4)) return;
	gSP1Triangle( vertices.v[2], vertices.v[3], vertices.v[4], 0 );

	if (!vertices.valid(5)) return;
	gSP1Triangle( vertices.v[4], vertices.v[3], vertices.v[5], 0 );

	if (!vertices.valid(6)) return;
	gSP1Triangle( vertices.v[4], vertices.v[5], vertices.v[6], 0 );
}

void F3DEX3_TriFan( u32 w0, u32 w1 )
{
	Vertices7 vertices = unpackVertices7( w0, w1 );
	if (!vertices.valid(0) || !vertices.valid(1) || !vertices.valid(2)) return;
	gSP1Triangle( vertices.v[0], vertices.v[1], vertices.v[2], 0 );

	if (!vertices.valid(3)) return;
	gSP1Triangle( vertices.v[0], vertices.v[2], vertices.v[3], 0 );

	if (!vertices.valid(4)) return;
	gSP1Triangle( vertices.v[0], vertices.v[3], vertices.v[4], 0 );

	if (!vertices.valid(5)) return;
	gSP1Triangle( vertices.v[0], vertices.v[4], vertices.v[5], 0 );

	if (!vertices.valid(6)) return;
	gSP1Triangle( vertices.v[0], vertices.v[5], vertices.v[6], 0 );
}

void F3DEX3_TriSnake( u32 w0, u32 w1 )
{
	(void)w1;
	u8 a = w0 & 0xFF;
	if (!(a & 1))
		return;

	u8 b = (w0 >> 8) & 0xFF;
	u8 c = (w0 >> 16) & 0xFF;

	u32 cursor = RSP.PC[RSP.PCi] - 4;

	while (true)
	{
		gSP1Triangle( (a >> 1) & 0x3f, b >> 1, c >> 1, 0 );
		if (a & 0x80)
			break;

		u32 cursorLoc = (cursor++) ^ 3;
		if (cursorLoc >= RDRAMSize)
			break;

		u8 v = RDRAM[cursorLoc];
		bool right = v & 1;
		if (!right)
			b = a;
		else
			c = a;
		a = v;
	}

	RSP.PC[RSP.PCi] = (cursor + 7) & (~7U);
	RSP.nextCmd = _SHIFTR( *(u32*)&RDRAM[RSP.PC[RSP.PCi]], 24, 8 );
	gSP1Triangle( 0, 0, 0, 0 ); // flush
}

void F3DEX3_LightToRDP( u32 w0, u32 w1 )
{
	(void)w0; (void)w1;
}

void F3DEX3_RelSegment( u32 w0, u32 w1 )
{
	gSPRelSegment( _SHIFTR( w0, 2, 4 ), w1 & 0x00FFFFFF );
}

void F3DEX3_Memset( u32 w0, u32 w1 )
{
	u32 value = (u16)gDP.half_1;
	u32 addr = w1;
	u32 length = w0 & 0x00FFFFFF;
	gDPMemset( value, addr, length );
}

void F3DEX3_Mtx( u32 w0, u32 w1 )
{
	gSPMatrix( w1, _SHIFTR( w0, 0, 8 ) ^ G_MTX_PUSH ^ G_MTX_LOAD );
}

void F3DZEX3_Branch_W( u32 w0, u32 w1 )
{
	gSPBranchLessW( gDP.half_1, _SHIFTR( w0, 1, 7 ), w1 );
}

void F3DEX3_Init()
{
	f3dex3_version = 0;

	GBI_InitFlags( F3DEX2 );

	G_ATTROFFSET_ST_ENABLE = F3DEX3_A_G_ATTROFFSET_ST_ENABLE;
	G_AMBOCCLUSION = F3DEX3_A_G_AMBOCCLUSION;

	GBI.PCStackSize = 18;

	GBI_SetGBI( G_RDPHALF_2,			F3DEX2_RDPHALF_2,			F3D_RDPHalf_2 );
	GBI_SetGBI( G_SETOTHERMODE_H,		F3DEX2_SETOTHERMODE_H,		F3DEX2_SetOtherMode_H );
	GBI_SetGBI( G_SETOTHERMODE_L,		F3DEX2_SETOTHERMODE_L,		F3DEX2_SetOtherMode_L );
	GBI_SetGBI( G_RDPHALF_1,			F3DEX2_RDPHALF_1,			F3D_RDPHalf_1 );
	GBI_SetGBI( G_SPNOOP,				F3DEX2_SPNOOP,				F3D_SPNoOp );
	GBI_SetGBI( G_ENDDL,				F3DEX2_ENDDL,				F3D_EndDL );
	GBI_SetGBI( G_DL,					F3DEX2_DL,					F3D_DList );
	GBI_SetGBI( G_LOAD_UCODE,			F3DEX2_LOAD_UCODE,			F3DEX_Load_uCode );
	GBI_SetGBI( G_MOVEMEM,				F3DEX2_MOVEMEM,				F3DEX3_MoveMem );
	GBI_SetGBI( G_MOVEWORD,				F3DEX2_MOVEWORD,			F3DEX3_MoveWord );
	GBI_SetGBI( G_MTX,					F3DEX2_MTX,					F3DEX2_Mtx );
	GBI_SetGBI( G_GEOMETRYMODE,			F3DEX2_GEOMETRYMODE,		F3DEX2_GeometryMode );
	GBI_SetGBI( G_POPMTX,				F3DEX2_POPMTX,				F3DEX2_PopMtx );
	GBI_SetGBI( G_TEXTURE,				F3DEX2_TEXTURE,				F3DEX2_Texture );
	GBI_SetGBI( G_DMA_IO,				F3DEX2_DMA_IO,				F3DEX2_DMAIO );
	GBI_SetGBI( G_SPECIAL_2,			F3DEX2_SPECIAL_2,			F3DEX2_Special_2 );
	GBI_SetGBI( G_SPECIAL_3,			F3DEX2_SPECIAL_3,			F3DEX2_Special_3 );

	GBI_SetGBI( G_VTX,					F3DEX2_VTX,					F3DEX2_Vtx );
	GBI_SetGBI( G_MODIFYVTX,			F3DEX2_MODIFYVTX,			F3DEX_ModifyVtx );
	GBI_SetGBI( G_CULLDL,				F3DEX2_CULLDL,				F3DEX_CullDL );
	GBI_SetGBI( G_BRANCH_W,				F3DEX3_BRANCH_WZ,			F3DZEX3_Branch_W );
	GBI_SetGBI( G_TRI1,					F3DEX2_TRI1,				F3DEX2_Tri1 );
	GBI_SetGBI( G_TRI2,					F3DEX2_TRI2,				F3DEX_Tri2 );
	GBI_SetGBI( G_QUAD,					F3DEX2_QUAD,				F3DEX2_Quad );
	GBI_SetGBI( G_TRISTRIP,				F3DEX3_TRISTRIP,			F3DEX3_TriStrip );
	GBI_SetGBI( G_TRIFAN,				F3DEX3_TRIFAN,				F3DEX3_TriFan );
	GBI_SetGBI( G_LIGHTTORDP,			F3DEX3_LIGHTTORDP,			F3DEX3_LightToRDP );
	GBI_SetGBI( G_RELSEGMENT,			F3DEX3_RELSEGMENT,			F3DEX3_RelSegment );
	GBI_SetGBI( G_SPECIAL_1,			F3DEX3_MEMSET,				F3DEX3_Memset );
}