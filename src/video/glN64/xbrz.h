/**
 * xBRZ filter for N64 texture upscaling (2x)
 * Based on xBRZ by Zenju (public domain).
 * Simplified for PS3: RGBA8 input/output only, no rotation/mirror.
 **/

#ifndef XBRZ_H
#define XBRZ_H

#include "../../main/winlnxdefs.h"
#include <stdlib.h>

/* Scale a RGBA8 buffer by 2x using the xBRZ algorithm.
 * src/w/h: source dimensions (pixels)
 * dst must be at least (w*2)*(h*2)*4 bytes. */
void xbrz_scale2x(const u32* src, u32* dst, int w, int h);

#endif
