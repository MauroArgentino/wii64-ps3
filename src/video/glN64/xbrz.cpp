/**
 * xBRZ 2x filter — faithful PS3 port of the xBRZ algorithm by Zenju.
 * Based on xBRZ (GPL-3.0 with linking exception), structure follows the
 * reference xbrz.cpp: 4x4 preprocessing kernel, blend-info byte packing,
 * four rotations of the 3x3 blend kernel and the Scaler2x blend operations.
 *
 * PS3 adaptations (self-contained, no libstdc++/STL):
 *  - Pixel format 0xRRGGBBAA (big-endian): R=bits31-24 G=23-16 B=15-8 A=7-0.
 *  - Color distance: inline YCbCr (BT.2020), SQUARED (no sqrt, no LUT).
 *    Thresholds are squared accordingly so comparisons stay equivalent:
 *    equalColorTolerance^2, dominantDirectionThreshold^2, steepDirectionThreshold^2.
 *  - Alpha channel participates in the M/N gradient lerp like any other channel
 *    (distance itself ignores alpha, matching xBRZ's RGB color-distance mode).
 *  - Out-of-bounds reads duplicate the nearest edge pixel (OobReaderDuplicate).
 *  - Preprocessing buffer obtained with malloc() and checked (PS3 crash policy).
 **/

#include "xbrz.h"
#include <string.h>

namespace {

/* ------------------------------------------------------------------ */
/* Channel accessors for 0xRRGGBBAA                                    */
/* ------------------------------------------------------------------ */

inline u32 getRed(u32 c)   { return (c >> 24) & 0xFF; }
inline u32 getGreen(u32 c) { return (c >> 16) & 0xFF; }
inline u32 getBlue(u32 c)  { return (c >>  8) & 0xFF; }
inline u32 getAlpha(u32 c) { return  c        & 0xFF; }

/* ------------------------------------------------------------------ */
/* ScalerCfg defaults                                                  */
/* ------------------------------------------------------------------ */

const double LUMINANCE_WEIGHT           = 1.0;
const double EQUAL_COLOR_TOLERANCE      = 12.0;
const double DOMINANT_DIRECTION_THRESH  = 3.6;
const double STEEP_DIRECTION_THRESHOLD  = 2.2;
const double CENTER_DIRECTION_BIAS      = 4.0;

/* distYCbCr() below returns the SQUARED distance, so compare against
 * squared thresholds to preserve the original metric relationships. */
const double EQ_TOL_SQ   = EQUAL_COLOR_TOLERANCE     * EQUAL_COLOR_TOLERANCE;
const double DOMINANT_SQ = DOMINANT_DIRECTION_THRESH * DOMINANT_DIRECTION_THRESH;
const double STEEP_SQ    = STEEP_DIRECTION_THRESHOLD * STEEP_DIRECTION_THRESHOLD;

inline double square(double v) { return v * v; }

/* Squared YCbCr color distance (ITU-R BT.2020), computed inline.
 * Alpha does not take part (same as xBRZ ColorDistanceRGB). */
inline double distYCbCr(u32 pix1, u32 pix2)
{
	if (pix1 == pix2) /* fast path, ~8% overall boost */
		return 0.0;

	const int rd = (int)getRed(pix1)   - (int)getRed(pix2);
	const int gd = (int)getGreen(pix1) - (int)getGreen(pix2);
	const int bd = (int)getBlue(pix1)  - (int)getBlue(pix2);

	const double k_r = 0.2627;
	const double k_b = 0.0593;
	const double k_g = 1 - k_r - k_b;

	const double scale_b = 0.5 / (1 - k_b);
	const double scale_r = 0.5 / (1 - k_r);

	const double y  = k_r * rd + k_g * gd + k_b * bd;
	const double cb = scale_b * (bd - y);
	const double cr = scale_r * (rd - y);

	return square(LUMINANCE_WEIGHT * y) + square(cb) + square(cr);
}

inline bool eqColor(u32 c1, u32 c2)
{
	return distYCbCr(c1, c2) < EQ_TOL_SQ;
}

/* ------------------------------------------------------------------ */
/* Blend classification                                                */
/* ------------------------------------------------------------------ */

enum BlendType
{
	BLEND_NONE = 0,
	BLEND_NORMAL,   /* normal indication to blend                    */
	BLEND_DOMINANT, /* strong indication to blend                    */
	/* must fit into 2 bits! */
};

struct BlendResult
{
	BlendType
	/**/blend_f, blend_g,
	/**/blend_j, blend_k;
};

struct Kernel4x4
{
	u32
	/**/a, b, c, d,
	/**/e, f, g, h,
	/**/i, j, k, l,
	/**/m, n, o, p;
};

/*
input kernel area naming convention:
-----------------
| A | B | C | D |
|---|---|---|---|
| E | F | G | H |   evaluate the four corners between F, G, J, K
|---|---|---|---|   input pixel is at position F
| I | J | K | L |
|---|---|---|---|
| M | N | O | P |
-----------------
*/
static void preProcessCorners(const Kernel4x4& ker, BlendResult& result)
{
	result.blend_f = BLEND_NONE;
	result.blend_g = BLEND_NONE;
	result.blend_j = BLEND_NONE;
	result.blend_k = BLEND_NONE;

	if ((ker.f == ker.g &&
	     ker.j == ker.k) ||
	    (ker.f == ker.j &&
	     ker.g == ker.k))
		return;

	const double bias = CENTER_DIRECTION_BIAS;

	const double jg = distYCbCr(ker.i, ker.f) + distYCbCr(ker.f, ker.c) +
	                  distYCbCr(ker.n, ker.k) + distYCbCr(ker.k, ker.h) +
	                  bias * distYCbCr(ker.j, ker.g);
	const double fk = distYCbCr(ker.e, ker.j) + distYCbCr(ker.j, ker.o) +
	                  distYCbCr(ker.b, ker.g) + distYCbCr(ker.g, ker.l) +
	                  bias * distYCbCr(ker.f, ker.k);

	if (jg < fk) /* test sample: median max/min ratio is 1.8 */
	{
		const bool dominantGradient = DOMINANT_SQ * jg < fk;
		if (ker.f != ker.g && ker.f != ker.j)
			result.blend_f = dominantGradient ? BLEND_DOMINANT : BLEND_NORMAL;

		if (ker.k != ker.j && ker.k != ker.g)
			result.blend_k = dominantGradient ? BLEND_DOMINANT : BLEND_NORMAL;
	}
	else if (fk < jg)
	{
		const bool dominantGradient = DOMINANT_SQ * fk < jg;
		if (ker.j != ker.f && ker.j != ker.k)
			result.blend_j = dominantGradient ? BLEND_DOMINANT : BLEND_NORMAL;

		if (ker.g != ker.f && ker.g != ker.k)
			result.blend_g = dominantGradient ? BLEND_DOMINANT : BLEND_NORMAL;
	}
}

/* ------------------------------------------------------------------ */
/* blendInfo byte packing: 4 x 2 bits                                  */
/*   TopL = bits 0-1, TopR = bits 2-3, BottomR = bits 4-5,             */
/*   BottomL = bits 6-7                                                */
/* ------------------------------------------------------------------ */

inline BlendType getTopR(unsigned char b)         { return (BlendType)(0x3 & (b >> 2)); }
inline BlendType getBottomL(unsigned char b)      { return (BlendType)(0x3 & (b >> 6)); }
inline BlendType getBottomR(unsigned char b)      { return (BlendType)(0x3 & (b >> 4)); }

inline void addTopR(unsigned char& b, BlendType bt)      { b |= (unsigned char)(bt << 2); }
inline void addBottomR(unsigned char& b, BlendType bt)   { b |= (unsigned char)(bt << 4); }
inline void addBottomL(unsigned char& b, BlendType bt)   { b |= (unsigned char)(bt << 6); }
inline void clearAddTopL(unsigned char& b, BlendType bt) { b = (unsigned char)bt; }

static inline unsigned char rotateBlendInfo(int rot, unsigned char b)
{
	switch (rot)
	{
		case 1:  return (unsigned char)((b << 2) | (b >> 6));
		case 2:  return (unsigned char)((b << 4) | (b >> 4));
		case 3:  return (unsigned char)((b << 6) | (b >> 2));
		default: return b;
	}
}

/* ------------------------------------------------------------------ */
/* 3x3 kernel rotation.                                                */
/* Layout: a b c / d e f / g h i  (e = center pixel F)                 */
/* ROT_IDX[r][n] = source index of logical cell n after r*90 CW turns  */
/* ------------------------------------------------------------------ */

static const int ROT_IDX[4][9] =
{
	{ 0, 1, 2,  3, 4, 5,  6, 7, 8 }, /* ROT_0                          */
	{ 6, 3, 0,  7, 4, 1,  8, 5, 2 }, /* ROT_90:  a<-g,d,a / h,e,b / i,f,c */
	{ 8, 7, 6,  5, 4, 3,  2, 1, 0 }, /* ROT_180                        */
	{ 2, 5, 8,  1, 4, 7,  0, 3, 6 }, /* ROT_270: a<-c,f,i / b,e,h / a,d,g */
};

/* ------------------------------------------------------------------ */
/* Gradient blending (ScalerCfg alphaGrad<M,N>)                        */
/* Blends pixFront with opacity M/N over pixBack, all four channels,   */
/* truncating integer division like xBRZ gradientRGB().                */
/* ------------------------------------------------------------------ */

static inline u32 mixColors(u32 front, u32 back, int M, int N)
{
	const u32 r = (getRed(front)   * M + getRed(back)   * (N - M)) / N;
	const u32 g = (getGreen(front) * M + getGreen(back) * (N - M)) / N;
	const u32 b = (getBlue(front)  * M + getBlue(back)  * (N - M)) / N;
	const u32 a = (getAlpha(front) * M + getAlpha(back) * (N - M)) / N;
	return (r << 24) | (g << 16) | (b << 8) | a;
}

static inline void alphaGrad(u32* pixBack, u32 pixFront, int M, int N)
{
	*pixBack = mixColors(pixFront, *pixBack, M, N);
}

/* ------------------------------------------------------------------ */
/* Output matrix access: 2x2 block at "base", row I col J (0/1 each).  */
/* Rotation mapping equals xBRZ MatrixRotation<N=2, rot>:              */
/*   ROT_0: J + I*W   ROT_90: I + (1-J)*W                              */
/*   ROT_180: (1-J)+(1-I)*W   ROT_270: (1-I) + J*W                     */
/* ------------------------------------------------------------------ */

static inline u32* oref(u32* base, int W, int rot, int I, int J)
{
	int io = I, jo = J;
	switch (rot)
	{
		case 1:  io = 1 - J; jo = I;     break;
		case 2:  io = 1 - I; jo = 1 - J; break;
		case 3:  io = J;     jo = 1 - I; break;
		default: break;
	}
	return base + jo + io * W;
}

/* ------------------------------------------------------------------ */
/* Scaler2x blend operations                                           */
/* ------------------------------------------------------------------ */

static inline void blendLineShallow(u32 col, u32* out, int W, int rot)
{
	alphaGrad(oref(out, W, rot, 1, 0), col, 1, 4);
	alphaGrad(oref(out, W, rot, 1, 1), col, 3, 4);
}

static inline void blendLineSteep(u32 col, u32* out, int W, int rot)
{
	alphaGrad(oref(out, W, rot, 0, 1), col, 1, 4);
	alphaGrad(oref(out, W, rot, 1, 1), col, 3, 4);
}

static inline void blendLineSteepAndShallow(u32 col, u32* out, int W, int rot)
{
	alphaGrad(oref(out, W, rot, 1, 0), col, 1, 4);
	alphaGrad(oref(out, W, rot, 0, 1), col, 1, 4);
	alphaGrad(oref(out, W, rot, 1, 1), col, 5, 6); /* fixes 7/8 used in xBR */
}

static inline void blendLineDiagonal(u32 col, u32* out, int W, int rot)
{
	alphaGrad(oref(out, W, rot, 1, 1), col, 1, 2);
}

static inline void blendCorner(u32 col, u32* out, int W, int rot)
{
	/* model a round corner: 1 - pi/4 = 0.2146018366 */
	alphaGrad(oref(out, W, rot, 1, 1), col, 21, 100);
}

/* ------------------------------------------------------------------ */
/* blendPixel for one rotation                                         */
/* ------------------------------------------------------------------ */

static void blendPixelRot(const u32 ker[9], u32* target, int trgWidth,
                          unsigned char blendInfo, int rot)
{
	const unsigned char bi = rotateBlendInfo(rot, blendInfo);

	if (getBottomR(bi) < BLEND_NORMAL)
		return;

	const u32 kb = ker[ROT_IDX[rot][1]];
	const u32 kc = ker[ROT_IDX[rot][2]];
	const u32 kd = ker[ROT_IDX[rot][3]];
	const u32 ke = ker[ROT_IDX[rot][4]];
	const u32 kf = ker[ROT_IDX[rot][5]];
	const u32 kg = ker[ROT_IDX[rot][6]];
	const u32 kh = ker[ROT_IDX[rot][7]];
	const u32 ki = ker[ROT_IDX[rot][8]];

	bool doLineBlend;
	if (getBottomR(bi) >= BLEND_DOMINANT)
		doLineBlend = true;
	/* make sure there is no second blending in an adjacent rotation for this
	 * pixel: handles insular pixels, mario eyes (but support double-blending
	 * for 90 degree corners) */
	else if (getTopR(bi) != BLEND_NONE && !eqColor(ke, kg))
		doLineBlend = false;
	else if (getBottomL(bi) != BLEND_NONE && !eqColor(ke, kc))
		doLineBlend = false;
	/* no full blending for L-shapes; blend corner only ("mario mushroom eyes") */
	else if (!eqColor(ke, ki) &&
	         eqColor(kg, kh) && eqColor(kh, ki) &&
	         eqColor(ki, kf) && eqColor(kf, kc))
		doLineBlend = false;
	else
		doLineBlend = true;

	const u32 px = distYCbCr(ke, kf) <= distYCbCr(ke, kh) ? kf : kh; /* most similar color */

	if (doLineBlend)
	{
		const double fg = distYCbCr(kf, kg);
		const double hc = distYCbCr(kh, kc);

		const bool haveShallowLine = STEEP_SQ * fg <= hc && ke != kg && kd != kg;
		const bool haveSteepLine   = STEEP_SQ * hc <= fg && ke != kc && kb != kc;

		if (haveShallowLine)
		{
			if (haveSteepLine)
				blendLineSteepAndShallow(px, target, trgWidth, rot);
			else
				blendLineShallow(px, target, trgWidth, rot);
		}
		else
		{
			if (haveSteepLine)
				blendLineSteep(px, target, trgWidth, rot);
			else
				blendLineDiagonal(px, target, trgWidth, rot);
		}
	}
	else
		blendCorner(px, target, trgWidth, rot);
}

} /* namespace */

/* ------------------------------------------------------------------ */
/* Public entry point                                                  */
/* ------------------------------------------------------------------ */

void xbrz_scale2x(const u32* src, u32* dst, int w, int h)
{
	if (!src || !dst || w <= 0 || h <= 0)
		return;

	const int dw = w * 2;

	unsigned char* preProcBuf = (unsigned char*)malloc(w);
	if (!preProcBuf)
	{
		/* OOM fallback: nearest-neighbor 2x, still writes every output pixel */
		for (int y = 0; y < h; ++y)
		{
			const u32* srow = src + y * w;
			u32* d0 = dst + 2 * y * dw;
			u32* d1 = d0 + dw;
			for (int x = 0; x < w; ++x)
			{
				const u32 c = srow[x];
				d0[2 * x]     = c;
				d0[2 * x + 1] = c;
				d1[2 * x]     = c;
				d1[2 * x + 1] = c;
			}
		}
		return;
	}
	memset(preProcBuf, 0, w); /* BLEND_NONE == 0 */

	for (int y = 0; y < h; ++y)
	{
		/* line pointers clamped at top/bottom -> duplicate edge pixels */
		const u32* sm1 = src + w * (y > 0       ? y - 1 : 0);
		const u32* s0  = src + w * y;
		const u32* sp1 = src + w * (y + 1 < h   ? y + 1 : h - 1);
		const u32* sp2 = src + w * (y + 2 < h   ? y + 2 : h - 1);

		u32* out = dst + 2 * y * dw;

		unsigned char blend_xy1 = 0; /* corner blending for (x, y + 1) */

		for (int x = 0; x < w; ++x, out += 2)
		{
			/* columns clamped left/right */
			const int xm1 = x > 0       ? x - 1 : 0;
			const int xp1 = x + 1 < w   ? x + 1 : w - 1;
			const int xp2 = x + 2 < w   ? x + 2 : w - 1;

			Kernel4x4 ker;
			ker.a = sm1[xm1]; ker.b = sm1[x]; ker.c = sm1[xp1]; ker.d = sm1[xp2];
			ker.e = s0 [xm1]; ker.f = s0 [x]; ker.g = s0 [xp1]; ker.h = s0 [xp2];
			ker.i = sp1[xm1]; ker.j = sp1[x]; ker.k = sp1[xp1]; ker.l = sp1[xp2];
			ker.m = sp2[xm1]; ker.n = sp2[x]; ker.o = sp2[xp1]; ker.p = sp2[xp2];

			/* evaluate bottom-right corner of current pixel; the other three
			 * corners were stored earlier by neighboring evaluations */
			unsigned char blend_xy = preProcBuf[x];
			{
				BlendResult res;
				preProcessCorners(ker, res);

				addBottomR(blend_xy, res.blend_f); /* all four corners known now */

				addTopR(blend_xy1, res.blend_j);   /* 2nd known corner for (x, y + 1)  */
				preProcBuf[x] = blend_xy1;         /* store for next row               */

				if (x + 1 < w)
				{
					clearAddTopL(blend_xy1, res.blend_k);    /* 1st corner for (x + 1, y + 1) */
					addBottomL(preProcBuf[x + 1], res.blend_g); /* 3rd corner for (x + 1, y)  */
				}
			}

			/* fill the 2x2 block with the center color first */
			out[0]      = ker.f;
			out[1]      = ker.f;
			out[dw]     = ker.f;
			out[dw + 1] = ker.f;

			/* blend the four corners of current pixel */
			if (blend_xy)
			{
				const u32 ker3[9] =
				{
					sm1[xm1], sm1[x], sm1[xp1],
					s0 [xm1], s0 [x], s0 [xp1],
					sp1[xm1], sp1[x], sp1[xp1],
				};

				blendPixelRot(ker3, out, dw, blend_xy, 0);
				blendPixelRot(ker3, out, dw, blend_xy, 1);
				blendPixelRot(ker3, out, dw, blend_xy, 2);
				blendPixelRot(ker3, out, dw, blend_xy, 3);
			}
		}
	}

	free(preProcBuf);
}
