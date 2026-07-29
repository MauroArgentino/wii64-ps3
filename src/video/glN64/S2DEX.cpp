/**
 * glN64_GX - S2DEX.cpp
 * Copyright (C) 2003 Orkin
 * Copyright (C) 2008, 2009 sepp256 (Port to Wii/Gamecube/PS3)
 *
 * glN64 homepage: http://gln64.emulation64.com
 * Wii64 homepage: http://www.emulatemii.com
 * email address: sepp256@gmail.com
 *
 * S2DEX implementation ported from GLideN64
 *
 * Ported: ObjCoordinates, _loadBGImage, one-piece BG rendering,
 *          _drawYUVImageToFrameBuffer, gSPSetupFunctions
**/

#ifdef __GX__
#include <gccore.h>
#endif // __GX__

#include "OpenGL.h"
#include "S2DEX.h"
#include "F3D.h"
#include "F3DEX.h"
#include "GBI.h"
#include "gSP.h"
#include "gDP.h"
#include "RSP.h"
#include "RDP.h"
#include "Types.h"
#include "convert.h"
#include "Debug.h"

#include "DepthBuffer.h"

#define S2DEX_MV_MATRIX			0
#define S2DEX_MV_SUBMUTRIX		2
#define S2DEX_MV_VIEWPORT		8

// S2DEX version
S2DEXVersion gs_s2dexversion = eVer1_7;

static uObjMtx objMtx;

static f32 _calcX(s16 x, s16 y, s16 origin)
{
	s16 X = origin + ((x * objMtx.A) >> 16) + ((y * objMtx.B) >> 16);
	return _FIXED2FLOAT(X, 2);
}

static f32 _calcY(s16 x, s16 y, s16 origin)
{
	s16 Y = origin + ((x * objMtx.C) >> 16) + ((y * objMtx.D) >> 16);
	return _FIXED2FLOAT(Y, 2);
}

void resetObjMtx()
{
	objMtx.A = 1 << 16;
	objMtx.B = 0;
	objMtx.C = 0;
	objMtx.D = 1 << 16;
	objMtx.X = 0;
	objMtx.Y = 0;
	objMtx.BaseScaleX = 1 << 10;
	objMtx.BaseScaleY = 1 << 10;
}

static inline u32 _YUVtoRGBA(u8 y, u8 u, u8 v)
{
	float r = y + (1.370705f * (v - 128));
	float g = y - (0.698001f * (v - 128)) - (0.337633f * (u - 128));
	float b = y + (1.732446f * (u - 128));
	r *= 0.125f;
	g *= 0.125f;
	b *= 0.125f;
	if (r > 31) r = 31;
	if (g > 31) g = 31;
	if (b > 31) b = 31;
	if (r < 0) r = 0;
	if (g < 0) g = 0;
	if (b < 0) b = 0;
	return ((u32)(r) << 11) | ((u32)(g) << 6) | ((u32)(b) << 1) | 1;
}

// ---- Ported from GLideN64 ----

void gSPSetupFunctions()
{
}

struct ObjCoordinates
{
	f32 ulx, uly, lrx, lry;
	f32 uls, ult, lrs, lrt;
	f32 z, w;

	ObjCoordinates(const uObjSprite *_pObjSprite, bool _useMatrix)
	{
		/* Fixed point coordinates calculation. Decoded by olivieryuyu */
		S2DEXCoordCorrector CC;
		s16 xh, xl, yh, yl;
		s16 sh, sl, th, tl;

		const u16 objSpriteScaleW = S2DEX_MAX(_pObjSprite->scaleW, (u16)1);
		const u16 objSpriteScaleH = S2DEX_MAX(_pObjSprite->scaleH, (u16)1);
		if (_useMatrix) {
			const u32 scaleW = (u32(objMtx.BaseScaleX) * 0x40 * objSpriteScaleW) >> 16;
			const u32 scaleH = (u32(objMtx.BaseScaleY) * 0x40 * objSpriteScaleH) >> 16;
			if (gs_s2dexversion == eVer1_3) {
				xh = (s16)(((((s64(_pObjSprite->objX) << 27) * (0x80007FFFU / u32(objMtx.BaseScaleX))) >> 0x30) + objMtx.X + CC.A2) & CC.B0);
				xl = ((s16)(((((s64(_pObjSprite->imageW) - CC.A1) << 8) * (0x80007FFFU / scaleW)) >> 0x20) + CC.B2) & CC.B0) + xh;
				yh = (s16)(((((s64(_pObjSprite->objY) << 27) * (0x80007FFFU / u32(objMtx.BaseScaleY))) >> 0x30) + objMtx.Y + CC.A2) & CC.B0);
				yl = ((s16)(((((s64(_pObjSprite->imageH) - CC.A1) << 8) * (0x80007FFFU / scaleH)) >> 0x20) + CC.B2) & CC.B0) + yh;
				sh = CC.A0 + CC.B3;
				sl = sh + _pObjSprite->imageW + CC.A0 - CC.A1 - 1;
				th = sh - (((yh & 3) * 0x0200 * scaleH) >> 16);
				tl = th + _pObjSprite->imageH + CC.A0 - CC.A1 - 1;
			} else {
				const s32 xhp = ((((s64(_pObjSprite->objX) << 16) * 0x0800) * (0x80007FFFU / u32(objMtx.BaseScaleX))) >> 32) + (((objMtx.X + CC.A2) & CC.B0) << 16);
				xh = (s16)(xhp >> 16);
				const s32 xlp = xhp + ((((u64(_pObjSprite->imageW) - CC.A1) << 24) * (0x80007FFFU / scaleW)) >> 32);
				xl = (s16)(xlp >> 16);
				const s32 yhp = ((((s64(_pObjSprite->objY) << 16) * 0x0800) * (0x80007FFFU / u32(objMtx.BaseScaleY))) >> 32) + (((objMtx.Y + CC.A2) & CC.B0) << 16);
				yh = (s16)(yhp >> 16);
				const s32 ylp = yhp + ((((u64(_pObjSprite->imageH) - CC.A1) << 24) * (0x80007FFFU / scaleH)) >> 32);
				yl = (s16)(ylp >> 16);
				sh = CC.A0 + CC.B2;
				sl = sh + _pObjSprite->imageW + CC.A0 - CC.A1 - 1;
				th = sh - (((yh & 3) * 0x0200 * scaleH) >> 16);
				tl = th + _pObjSprite->imageH + CC.A0 - CC.A1 - 1;
			}
		} else {
			xh = (_pObjSprite->objX + CC.A2) & CC.B0;
			xl = ((s16)((((u64(_pObjSprite->imageW) - CC.A1) << 24) * (0x80007FFFU / u32(objSpriteScaleW))) >> 48)) + xh;
			yh = (_pObjSprite->objY + CC.A2) & CC.B0;
			yl = ((s16)((((u64(_pObjSprite->imageH) - CC.A1) << 24) * (0x80007FFFU / u32(objSpriteScaleH))) >> 48)) + yh;
			sh = CC.A0 + CC.B2;
			sl = sh + _pObjSprite->imageW + CC.A0 - CC.A1 - 1;
			th = sh - (((yh & 3) * 0x0200 * objSpriteScaleH) >> 16);
			tl = th + _pObjSprite->imageH + CC.A0 - CC.A1 - 1;
		}

		ulx = _FIXED2FLOAT(xh, 2);
		lrx = _FIXED2FLOAT(xl, 2);
		uly = _FIXED2FLOAT(yh, 2);
		lry = _FIXED2FLOAT(yl, 2);

		uls = _FIXED2FLOAT(sh, 5);
		lrs = _FIXED2FLOAT(sl, 5);
		ult = _FIXED2FLOAT(th, 5);
		lrt = _FIXED2FLOAT(tl, 5);

		if ((_pObjSprite->imageFlags & G_BG_FLAG_FLIPS) != 0) {
			f32 _tmp = uls; uls = lrs; lrs = _tmp;
		}
		if ((_pObjSprite->imageFlags & G_BG_FLAG_FLIPT) != 0) {
			f32 _tmp = ult; ult = lrt; lrt = _tmp;
		}

		z = (gDP.otherMode.depthSource == G_ZS_PRIM) ? gDP.primDepth.z : gSP.viewport.nearz;
		w = 1.0f;
	}

	ObjCoordinates(const uObjScaleBg * _pObjScaleBg)
	{
		const f32 frameX = _FIXED2FLOAT(_pObjScaleBg->frameX, 2);
		const f32 frameY = _FIXED2FLOAT(_pObjScaleBg->frameY, 2);
		const f32 imageX = gSP.bgImage.imageX;
		const f32 imageY = gSP.bgImage.imageY;
		f32 scaleW = gSP.bgImage.scaleW;
		f32 scaleH = gSP.bgImage.scaleH;

		if (gDP.otherMode.cycleType == G_CYC_COPY) {
			scaleW = 1.0f;
			scaleH = 1.0f;
		}

		f32 frameW = _FIXED2FLOAT(_pObjScaleBg->frameW, 2);
		f32 frameH = _FIXED2FLOAT(_pObjScaleBg->frameH, 2);
		f32 imageW = (f32)((_pObjScaleBg->imageW >> 2) & 0xFFFFFFFE);
		f32 imageH = (f32)((_pObjScaleBg->imageH >> 2) & 0xFFFFFFFE);

		ulx = frameX;
		uly = frameY;
		lrx = ulx + S2DEX_MIN(frameW, imageW / scaleW);
		lry = uly + S2DEX_MIN(frameH, imageH / scaleH);

		uls = imageX;
		ult = imageY;
		lrs = uls + (lrx - ulx) * scaleW;
		lrt = ult + (lry - uly) * scaleH;

		if (gDP.otherMode.cycleType != G_CYC_COPY) {
			if ((gSP.objRendermode & G_OBJRM_SHRINKSIZE_1) != 0u) {
				uls += 0.5f;
				ult += 0.5f;
				lrs -= 0.5f;
				lrt -= 0.5f;
			}
			else if ((gSP.objRendermode & G_OBJRM_SHRINKSIZE_2) != 0u) {
				uls += 1.0f;
				ult += 1.0f;
				lrs -= 1.0f;
				lrt -= 1.0f;
			}
		}

#ifdef _BIG_ENDIAN
		if ((_pObjScaleBg->imageLoad & G_BG_FLAG_FLIPS) != 0u) {
#else
		if ((_pObjScaleBg->imageFlip & G_BG_FLAG_FLIPS) != 0u) {
#endif
			f32 _tmp = ulx; ulx = lrx; lrx = _tmp;
		}

		z = (gDP.otherMode.depthSource == G_ZS_PRIM) ? gDP.primDepth.z : gSP.viewport.nearz;
		w = 1.0f;
	}
};

static void gSPDrawObjRect(const ObjCoordinates & _coords)
{
	gDP.texRect.width = (unsigned long)(_coords.lrs + 1.0f);
	gDP.texRect.height = (unsigned long)(_coords.lrt + 1.0f);
	OGL_DrawTexturedRect(_coords.ulx, _coords.uly, _coords.lrx, _coords.lry,
		_coords.uls, _coords.ult, _coords.lrs, _coords.lrt, false);

	if (depthBuffer.current) depthBuffer.current->cleared = FALSE;
	gDP.colorImage.changed = TRUE;
	gDP.colorImage.height = (unsigned long)(S2DEX_MAX(gDP.colorImage.height, (u32)gDP.scissor.lry));
}

void _drawYUVImageToFrameBuffer(const ObjCoordinates & _objCoords)
{
	const u32 ulx = (u32)_objCoords.ulx;
	const u32 uly = (u32)_objCoords.uly;
	const u32 lrx = (u32)_objCoords.lrx;
	const u32 lry = (u32)_objCoords.lry;
	const u32 ci_width = gDP.colorImage.width;
	const u32 ci_height = (u32)gDP.scissor.lry;
	if (ulx >= ci_width) return;
	if (uly >= ci_height) return;
	u32 width = 16, height = 16;
	if (lrx > ci_width) width = ci_width - ulx;
	if (lry > ci_height) height = ci_height - uly;
	u32 * mb = (u32*)(RDRAM + gDP.textureImage.address);
	u16 * dst = (u16*)(RDRAM + gDP.colorImage.address);
	dst += ulx + uly * ci_width;

	for (u16 h = 0; h < 16; h++) {
		for (u16 w = 0; w < 16; w += 2) {
			u32 t = *(mb++);
			if ((h < height) && (w < width)) {
				u8 y0 = (u8)t & 0xFF;
				u8 v = (u8)(t >> 8) & 0xFF;
				u8 y1 = (u8)(t >> 16) & 0xFF;
				u8 u = (u8)(t >> 24) & 0xFF;
				*(dst++) = (u16)_YUVtoRGBA(y0, u, v);
				*(dst++) = (u16)_YUVtoRGBA(y1, u, v);
			}
		}
		dst += ci_width - 16;
	}
}

static void _loadBGImage(const uObjScaleBg * _pBgInfo, bool _loadScale)
{
	gSP.bgImage.address = RSP_SegmentToPhysical(_pBgInfo->imagePtr);

	const u32 imageW = _pBgInfo->imageW >> 2;
	const u32 imageH = _pBgInfo->imageH >> 2;
	gSP.bgImage.width = imageW - imageW % 2;
	gSP.bgImage.height = imageH - imageH % 2;
	gSP.bgImage.format = _pBgInfo->imageFmt;
	gSP.bgImage.size = _pBgInfo->imageSiz;
	gSP.bgImage.palette = _pBgInfo->imagePal;
	gSP.bgImage.imageX = _FIXED2FLOAT(_pBgInfo->imageX, 5);
	gSP.bgImage.imageY = _FIXED2FLOAT(_pBgInfo->imageY, 5);

	if (_loadScale) {
		gSP.bgImage.scaleW = _FIXED2FLOAT(_pBgInfo->scaleW, 10);
		gSP.bgImage.scaleH = _FIXED2FLOAT(_pBgInfo->scaleH, 10);
	} else {
		gSP.bgImage.scaleW = 1.0f;
		gSP.bgImage.scaleH = 1.0f;
	}

	gDP.textureMode = TEXTUREMODE_BGIMAGE;
}

static void BgRect1CycOnePiece(u32 _bg)
{
	uObjScaleBg *pObjScaleBg = (uObjScaleBg*)&RDRAM[_bg];
	_loadBGImage(pObjScaleBg, true);

	gDP.otherMode.cycleType = G_CYC_1CYCLE;
	gDP.changed |= CHANGED_CYCLETYPE;
	gSPTexture(1.0f, 1.0f, 0, 0, TRUE);

	ObjCoordinates objCoords(pObjScaleBg);
	gSPDrawObjRect(objCoords);
}

static void BgRectCopyOnePiece(u32 _bg)
{
	uObjScaleBg *pObjBg = (uObjScaleBg*)&RDRAM[_bg];
	_loadBGImage(pObjBg, false);

	gDP.otherMode.cycleType = G_CYC_COPY;
	gDP.changed |= CHANGED_CYCLETYPE;
	gSPTexture(1.0f, 1.0f, 0, 0, TRUE);

	ObjCoordinates objCoords(pObjBg);
	gSPDrawObjRect(objCoords);
}

// ---- End of ported code ----

static void gSPSetSpriteTile(const uObjSprite *objSprite)
{
	u32 w = S2DEX_MAX(objSprite->imageW >> 5, 1);
	u32 h = S2DEX_MAX(objSprite->imageH >> 5, 1);

	gDPSetTile(objSprite->imageFmt, objSprite->imageSiz, objSprite->imageStride, objSprite->imageAdrs, G_TX_RENDERTILE, objSprite->imagePal, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, 0, 0, 0, 0);
	gDPSetTileSize(G_TX_RENDERTILE, 0, 0, (w - 1) << 2, (h - 1) << 2);
	gSPTexture(1.0f, 1.0f, 0, 0, TRUE);
}

void gSPObjLoadTxtr(u32 tx)
{
	u32 address = RSP_SegmentToPhysical(tx);
	uObjTxtr *objTxtr = (uObjTxtr*)&RDRAM[address];

	if ((gSP.status[objTxtr->block.sid >> 2] & objTxtr->block.mask) != objTxtr->block.flag)
	{
		switch (objTxtr->block.type)
		{
			case G_OBJLT_TXTRBLOCK:
				gDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, objTxtr->block.tsize + 1, objTxtr->block.image);
				gDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, objTxtr->block.tmem, G_TX_LOADTILE, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 0, 0, 0, 0);
				gDPLoadBlock(G_TX_LOADTILE, 0, 0, objTxtr->block.tsize << 2, objTxtr->block.tline);
				break;
			case G_OBJLT_TXTRTILE:
				gDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, objTxtr->tile.twidth + 1, objTxtr->tile.image);
				gDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, (objTxtr->tile.twidth + 1) >> 2, objTxtr->tile.tmem, G_TX_LOADTILE, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 0, 0, 0, 0);
				gDPLoadTile(G_TX_LOADTILE, 0, 0, objTxtr->tile.twidth << 2, objTxtr->tile.theight);
				break;
			case G_OBJLT_TLUT:
				gDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, objTxtr->tlut.image);
				gDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_4b, 0, objTxtr->tlut.phead, G_TX_LOADTILE, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 0, 0, 0, 0);
				gDPLoadTLUT(G_TX_LOADTILE, 0, 0, objTxtr->tlut.pnum << 2, 0);
				break;
		}
		gSP.status[objTxtr->block.sid >> 2] = (gSP.status[objTxtr->block.sid >> 2] & ~objTxtr->block.mask) | (objTxtr->block.flag & objTxtr->block.mask);
	}
}

void gSPObjRectangle(u32 sp)
{
	u32 address = RSP_SegmentToPhysical(sp);
	uObjSprite *objSprite = (uObjSprite*)&RDRAM[address];
	gSPSetSpriteTile(objSprite);

	ObjCoordinates objCoords(objSprite, false);
	gSPDrawObjRect(objCoords);
}

void gSPObjRectangleR(u32 sp)
{
	u32 address = RSP_SegmentToPhysical(sp);
	uObjSprite *objSprite = (uObjSprite*)&RDRAM[address];
	gSPSetSpriteTile(objSprite);

	ObjCoordinates objCoords(objSprite, true);
	gSPDrawObjRect(objCoords);
}

void gSPObjSprite(u32 sp)
{
	u32 address = RSP_SegmentToPhysical(sp);
	uObjSprite *objSprite = (uObjSprite*)&RDRAM[address];
	gSPSetSpriteTile(objSprite);

	S2DEXCoordCorrector CC;
	s16 x0 = (gs_s2dexversion == eVer1_3) ? ((objMtx.X + CC.B5) & CC.B0) + CC.B7 : ((objMtx.X + CC.B3) & CC.B0);
	s16 y0 = (gs_s2dexversion == eVer1_3) ? ((objMtx.Y + CC.B5) & CC.B0) + CC.B7 : ((objMtx.Y + CC.B3) & CC.B0);
	s16 ulx = objSprite->objX + CC.A3;
	s16 uly = objSprite->objY + CC.A3;
	u32 objSpriteScaleW = S2DEX_MAX(objSprite->scaleW, 1U);
	u32 objSpriteScaleH = S2DEX_MAX(objSprite->scaleH, 1U);
	s16 lrx = ((((u64)objSprite->imageW - CC.A1) << 8) * (0x80007FFFU / objSpriteScaleW) >> 32) + ulx;
	s16 lry = ((((u64)objSprite->imageH - CC.A1) << 8) * (0x80007FFFU / objSpriteScaleH) >> 32) + uly;

	f32 uls = 0.0f;
	f32 lrs = _FIXED2FLOAT(objSprite->imageW, 5) - 1.0f;
	f32 ult = 0.0f;
	f32 lrt = _FIXED2FLOAT(objSprite->imageH, 5) - 1.0f;

	if (objSprite->imageFlags & G_BG_FLAG_FLIPS) { f32 tmp = uls; uls = lrs; lrs = tmp; }
	if (objSprite->imageFlags & G_BG_FLAG_FLIPT) { f32 tmp = ult; ult = lrt; lrt = tmp; }

	f32 z = (gDP.otherMode.depthSource == G_ZS_PRIM) ? gDP.primDepth.z : gSP.viewport.nearz;

	gSP.vertices[0].x = _calcX(ulx, uly, x0); gSP.vertices[0].y = _calcY(ulx, uly, y0); gSP.vertices[0].z = z; gSP.vertices[0].w = 1.0f; gSP.vertices[0].s = uls; gSP.vertices[0].t = ult;
	gSP.vertices[1].x = _calcX(lrx, uly, x0); gSP.vertices[1].y = _calcY(lrx, uly, y0); gSP.vertices[1].z = z; gSP.vertices[1].w = 1.0f; gSP.vertices[1].s = lrs; gSP.vertices[1].t = ult;
	gSP.vertices[2].x = _calcX(ulx, lry, x0); gSP.vertices[2].y = _calcY(ulx, lry, y0); gSP.vertices[2].z = z; gSP.vertices[2].w = 1.0f; gSP.vertices[2].s = uls; gSP.vertices[2].t = lrt;
	gSP.vertices[3].x = _calcX(lrx, lry, x0); gSP.vertices[3].y = _calcY(lrx, lry, y0); gSP.vertices[3].z = z; gSP.vertices[3].w = 1.0f; gSP.vertices[3].s = lrs; gSP.vertices[3].t = lrt;

	gSP1Quadrangle(0, 1, 3, 2);
}

void gSPObjMatrix(u32 mtx)
{
	objMtx = *reinterpret_cast<const uObjMtx*>(RDRAM + RSP_SegmentToPhysical(mtx));
}

void gSPObjSubMatrix(u32 mtx)
{
	const uObjSubMtx *pObjSubMtx = reinterpret_cast<const uObjSubMtx*>(RDRAM + RSP_SegmentToPhysical(mtx));
	objMtx.X = pObjSubMtx->X;
	objMtx.Y = pObjSubMtx->Y;
	objMtx.BaseScaleX = pObjSubMtx->BaseScaleX;
	objMtx.BaseScaleY = pObjSubMtx->BaseScaleY;
}

void gSPBgRect1Cyc(u32 bg)
{
	const u32 bgAddr = RSP_SegmentToPhysical(bg);
	BgRect1CycOnePiece(bgAddr);
}

void gSPBgRectCopy(u32 bg)
{
	const u32 bgAddr = RSP_SegmentToPhysical(bg);
	BgRectCopyOnePiece(bgAddr);
}

void gSPLoadUcodeEx(u32 uc_start, u32 uc_dstart, u16 uc_dsize)
{
	RSP.PCi = 0;
	gSP.matrix.modelViewi = 0;
	gSP.changed |= CHANGED_MATRIX;
	gSP.status[0] = gSP.status[1] = gSP.status[2] = gSP.status[3] = 0;
	resetObjMtx();

	if ((((uc_start & 0x1FFFFFFF) + 4096) > RDRAMSize) || (((uc_dstart & 0x1FFFFFFF) + uc_dsize) > RDRAMSize))
	{
#ifdef DEBUG
		DebugMsg( DEBUG_HIGH | DEBUG_ERROR, "// Attempting to load ucode out of invalid address\n" );
		DebugMsg( DEBUG_HIGH | DEBUG_HANDLED, "gSPLoadUcodeEx( 0x%08X, 0x%08X, %i );\n", uc_start, uc_dstart, uc_dsize );
#endif
		return;
	}

	MicrocodeInfo *ucode = GBI_DetectMicrocode(uc_start, uc_dstart, uc_dsize);

	if (ucode->type != NONE) {
		GBI_MakeCurrent(ucode);
# ifdef __GX__
#ifdef SHOW_DEBUG
		sprintf(txtbuffer,"UCODE Detected: %s", MicrocodeTypes[ucode->type]);
		DEBUG_print(txtbuffer,DBG_RSPINFO);
#endif
# endif // __GX__
	}
#ifdef SHOW_DEBUG
	else
# ifdef RSPTHREAD
		SetEvent( RSP.threadMsg[RSPMSG_CLOSE] );
# else
# if !(defined(__GX__)||defined(PS3))
		puts( "Warning: Unknown UCODE!!!" );
# else // !__GX__
		DEBUG_print((char*)"Warning: Unknown UCODE!!!",DBG_RSPINFO);
# endif // __GX__
# endif
#endif

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_ERROR, "// Unknown microcode" );
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED, "gSPLoadUcodeEx( 0x%08X, 0x%08X, %i );\n", uc_start, uc_dstart, uc_dsize );
#endif
}

void S2DEX_BG_1Cyc(u32 w0, u32 w1)
{
	const u32 bgAddr = RSP_SegmentToPhysical(w1);
	gSPBgRect1Cyc(bgAddr);
}

void S2DEX_BG_Copy(u32 w0, u32 w1)
{
	const u32 bgAddr = RSP_SegmentToPhysical(w1);
	gSPBgRectCopy(bgAddr);
}

void S2DEX_Obj_Rectangle(u32 w0, u32 w1)
{
	gSPObjRectangle(w1);
}

void S2DEX_Obj_Sprite(u32 w0, u32 w1)
{
	gSPObjSprite(w1);
}

void S2DEX_Obj_MoveMem(u32 w0, u32 w1)
{
	switch (_SHIFTR(w0, 0, 16))
	{
		case S2DEX_MV_MATRIX:
			gSPObjMatrix(w1);
			break;
		case S2DEX_MV_SUBMUTRIX:
			gSPObjSubMatrix(w1);
			break;
		case S2DEX_MV_VIEWPORT:
			gSPViewport(w1);
			break;
	}
}

void S2DEX_Select_DL(u32 w0, u32 w1)
{
	gSP.selectDL.addr |= (_SHIFTR(w0, 0, 16)) << 16;
	const u8 sid = gSP.selectDL.sid;
	const u32 flag = gSP.selectDL.flag;
	const u32 mask = w1;
	if ((gSP.status[sid] & mask) == flag)
		return;

	gSP.status[sid] = (gSP.status[sid] & ~mask) | (flag & mask);

	switch (_SHIFTR(w0, 16, 8))
	{
		case 0: // G_DL_PUSH
			gSPDisplayList(gSP.selectDL.addr);
			break;
		case 1: // G_DL_NOPUSH
			gSPBranchList(gSP.selectDL.addr);
			break;
	}
}

void S2DEX_Obj_RenderMode(u32 w0, u32 w1)
{
	gSP.objRendermode = w1;
}

void S2DEX_Obj_Rectangle_R(u32 w0, u32 w1)
{
	gSPObjRectangleR(w1);
}

void S2DEX_Obj_LoadTxtr(u32 w0, u32 w1)
{
	gSPObjLoadTxtr(w1);
}

void S2DEX_Obj_LdTx_Sprite(u32 w0, u32 w1)
{
	gSPObjLoadTxtr(w1);
	gSPObjSprite(w1 + sizeof(uObjTxtr));
}

void S2DEX_Obj_LdTx_Rect(u32 w0, u32 w1)
{
	gSPObjLoadTxtr(w1);
	gSPObjRectangle(w1 + sizeof(uObjTxtr));
}

void S2DEX_Obj_LdTx_Rect_R(u32 w0, u32 w1)
{
	gSPObjLoadTxtr(w1);
	gSPObjRectangleR(w1 + sizeof(uObjTxtr));
}

void S2DEX_RDPHalf_0(u32 w0, u32 w1)
{
	if (RSP.nextCmd == G_SELECT_DL) {
		gSP.selectDL.addr = _SHIFTR(w0, 0, 16);
		gSP.selectDL.sid = _SHIFTR(w0, 18, 8);
		gSP.selectDL.flag = w1;
		return;
	}
	if (RSP.nextCmd == G_RDPHALF_1) {
		RDP_TexRect(w0, w1);
		return;
	}
}

void S2DEX_MoveWord(u32 w0, u32 w1)
{
	switch (_SHIFTR(w0, 0, 8))
	{
		case G_MW_GENSTAT:
			gSPSetStatus(_SHIFTR(w0, 0, 16), w1);
			break;
		default:
			F3D_MoveWord(w0, w1);
			break;
	}
}

void S2DEX_Init()
{
	gSPSetupFunctions();
	GBI_InitFlags(F3DEX);
	gSP.geometryMode = 0;
	resetObjMtx();

	GBI.PCStackSize = 18;

	GBI_SetGBI(G_SPNOOP, F3D_SPNOOP, F3D_SPNoOp);
	GBI_SetGBI(G_BG_1CYC, 0x01, S2DEX_BG_1Cyc);
	GBI_SetGBI(G_BG_COPY, 0x02, S2DEX_BG_Copy);
	GBI_SetGBI(G_OBJ_RECTANGLE, 0x03, S2DEX_Obj_Rectangle);
	GBI_SetGBI(G_OBJ_SPRITE, 0x04, S2DEX_Obj_Sprite);
	GBI_SetGBI(G_OBJ_MOVEMEM, 0x05, S2DEX_Obj_MoveMem);
	GBI_SetGBI(G_DL, F3D_DL, F3D_DList);
	GBI_SetGBI(G_SELECT_DL, 0xB0, S2DEX_Select_DL);
	GBI_SetGBI(G_OBJ_RENDERMODE, 0xB1, S2DEX_Obj_RenderMode);
	GBI_SetGBI(G_OBJ_RECTANGLE_R, 0xB2, S2DEX_Obj_Rectangle_R);
	GBI_SetGBI(G_OBJ_LOADTXTR, 0xC1, S2DEX_Obj_LoadTxtr);
	GBI_SetGBI(G_OBJ_LDTX_SPRITE, 0xC2, S2DEX_Obj_LdTx_Sprite);
	GBI_SetGBI(G_OBJ_LDTX_RECT, 0xC3, S2DEX_Obj_LdTx_Rect);
	GBI_SetGBI(G_OBJ_LDTX_RECT_R, 0xC4, S2DEX_Obj_LdTx_Rect_R);
	GBI_SetGBI(G_MOVEWORD, F3D_MOVEWORD, S2DEX_MoveWord);
	GBI_SetGBI(G_SETOTHERMODE_H, F3D_SETOTHERMODE_H, F3D_SetOtherMode_H);
	GBI_SetGBI(G_SETOTHERMODE_L, F3D_SETOTHERMODE_L, F3D_SetOtherMode_L);
	GBI_SetGBI(G_ENDDL, F3D_ENDDL, F3D_EndDL);
	GBI_SetGBI(G_RDPHALF_0, 0xE4, S2DEX_RDPHalf_0);
	GBI_SetGBI(G_RDPHALF_1, F3D_RDPHALF_1, F3D_RDPHalf_1);
	GBI_SetGBI(G_RDPHALF_2, F3D_RDPHALF_2, F3D_RDPHalf_2);
	GBI_SetGBI(G_LOAD_UCODE, 0xAF, F3DEX_Load_uCode);
}

void S2DEX_1_03_Init() { S2DEX_Init(); gs_s2dexversion = eVer1_3; }
void S2DEX_1_05_Init() { S2DEX_Init(); gs_s2dexversion = eVer1_5; }
void S2DEX_1_07_Init() { S2DEX_Init(); gs_s2dexversion = eVer1_7; }
