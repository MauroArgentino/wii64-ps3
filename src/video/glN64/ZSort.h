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
#define G_ZSORT_SETOTHERMODE_H  0x87
#define G_ZSORT_SETOTHERMODE_L  0x88
#define G_ZSORT_SETGEOMETRYMODE  0x88
#define G_ZSORT_CLEARGEOMETRYMODE 0x89
#define G_ZSORT_SPNOOP    0x8A
#define G_ZSORT_DL        0x8B

// ZSort microcode constants (based on F3DEX)
#define ZSORT_MTX_STACKSIZE		18
#define ZSORT_MTX_MODELVIEW		0x00
#define ZSORT_MTX_PROJECTION		0x01
#define ZSORT_MTX_MUL			0x02
#define ZSORT_MTX_LOAD			0x03
#define ZSORT_MTX_NOPUSH		0x04
#define ZSORT_MTX_PUSH			0x05

#define ZSORT_TEXTURE_ENABLE	0x01
#define ZSORT_SHADING_SMOOTH	0x02
#define ZSORT_CULL_FRONT		0x04
#define ZSORT_CULL_BACK		0x08
#define ZSORT_CULL_BOTH		0x0C
#define ZSORT_CLIPPING		0x10

#define ZSORT_MV_VIEWPORT		0x01

#define ZSORT_MWO_aLIGHT_1	0x01
#define ZSORT_MWO_bLIGHT_1	0x02
#define ZSORT_MWO_aLIGHT_2	0x03
#define ZSORT_MWO_bLIGHT_2	0x04
#define ZSORT_MWO_aLIGHT_3	0x05
#define ZSORT_MWO_bLIGHT_3	0x06
#define ZSORT_MWO_aLIGHT_4	0x07
#define ZSORT_MWO_bLIGHT_4	0x08
#define ZSORT_MWO_aLIGHT_5	0x09
#define ZSORT_MWO_bLIGHT_5	0x0A
#define ZSORT_MWO_aLIGHT_6	0x0B
#define ZSORT_MWO_bLIGHT_6	0x0C
#define ZSORT_MWO_aLIGHT_7	0x0D
#define ZSORT_MWO_bLIGHT_7	0x0E
#define ZSORT_MWO_aLIGHT_8	0x0F
#define ZSORT_MWO_bLIGHT_8	0x10

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