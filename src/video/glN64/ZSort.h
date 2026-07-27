/**
 * ZSort.h - ZSort microcode for Mario Kart 64
 * Minimal Z-sort implementation for transparent particles (smoke, sparks, etc.)
 */

#ifndef ZSORT_H
#define ZSORT_H

#include "Types.h"

// ZSort command opcodes
#define G_ZSORT_TRI1      0x80
#define G_ZSORT_TRI2      0x81
#define G_ZSORT_QUAD      0x82
#define G_ZSORT_LOAD_UCODE 0x83
#define G_ZSORT_SET_Z      0x84
#define G_ZSORT_SET_ALPHA  0x85
#define G_ZSORT_SET_DEPTH  0x86

// ZSort state
extern bool zsort_enabled;
extern bool zsort_depth_test_enabled;

struct ZSortState {
    bool depth_test;
    bool alpha_blend;
    u32 z_value;
    u8 alpha_threshold;
};

extern struct ZSortState zsort_state;

// ZSort functions
void ZSORT_Init();
void ZSORT_SwitchToZSort();
void ZSORT_SwitchFromZSort();

// ZSort command implementations
void ZSORT_SPNoOp( u32 w0, u32 w1 );
void ZSORT_DList( u32 w0, u32 w1 );
void ZSORT_Tri1( u32 w0, u32 w1 );
void ZSORT_Tri2( u32 w0, u32 w1 );
void ZSORT_Quad( u32 w0, u32 w1 );
void ZSORT_Load_uCode( u32 w0, u32 w1 );
void ZSORT_SetOtherMode_H( u32 w0, u32 w1 );
void ZSORT_SetOtherMode_L( u32 w0, u32 w1 );
void ZSORT_SetGeometryMode( u32 w0, u32 w1 );
void ZSORT_ClearGeometryMode( u32 w0, u32 w1 );

#endif // ZSORT_H