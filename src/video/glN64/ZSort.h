/**
 * ZSort.h - ZSort microcode for Mario Kart 64
 * Full implementation based on GLideN64's ZSort.cpp, adapted for wii64-ps3
 */

#ifndef ZSORT_H
#define ZSORT_H

#include "Types.h"

struct zSortVDest{
	s16 sy;
	s16 sx;
	s32 invw;
	s16 yi;
	s16 xi;
	s16 wi;
	u8 fog;
	u8 cc;
};

struct ZSortRDP {
	f32 view_scale[2];
	f32 view_trans[2];
};

extern struct ZSortRDP zSortRdp;

void ZSort_Init();
void ZSort_RDPCMD( u32 w0, u32 _w1 );
int ZSort_Calc_invw( int _w );

#endif // ZSORT_H
