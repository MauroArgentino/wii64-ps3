/**
 * HQ4x filter for N64 texture upscaling (4x)
 * Based on the HQx family of filters by Hiroyuki Hori.
 * Simplified for PS3: RGBA8 input/output only.
 **/

#ifndef HQ4X_H
#define HQ4X_H

#include "../../main/winlnxdefs.h"

/* Scale a RGBA8 buffer by 4x using the HQ4x algorithm.
 * src/w/h: source dimensions (pixels)
 * dst must be at least (w*4)*(h*4)*4 bytes. */
void hq4x_scale4x(const u32* src, u32* dst, int w, int h);

#endif
