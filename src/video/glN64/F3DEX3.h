/**
 * glN64_GX - F3DEX3.h
 * Copyright (C) 2003 Orkin
 *
 * F3DEX3: Conker's Bad Fur Day, Perfect Dark PAL, Mickey's Speedway USA microcode
 * Extended F3DEX2 with TRISTRIP, TRIFAN, TRISNAKE, LIGHTTORDP, RELSEGMENT,
 * MoveWord FX (ambient occlusion, Fresnel, alpha compare cull, texture offsets)
**/

#ifndef F3DEX3_H
#define F3DEX3_H

#define F3DEX3_BRANCH_WZ	0x04

#define F3DEX3_MEMSET            0xD5

#define F3DEX3_TRISTRIP          0x08
#define F3DEX3_TRIFAN            0x09
#define F3DEX3_LIGHTTORDP        0x0A
#define F3DEX3_RELSEGMENT        0x0B

#define F3DEX3_B_TRISNAKE        0x08
#define F3DEX3_B_TRI_NOOP        0x09

#define F3DEX3_G_MW_FX		     0x00
#define F3DEX3_G_MW_LIGHTCOL     0x0A

#define F3DEX3_G_MW_HALFWORD_FLAG 0x8000

#define F3DEX3_G_MWO_AO_AMBIENT         0x00
#define F3DEX3_G_MWO_AO_DIRECTIONAL     0x02
#define F3DEX3_G_MWO_AO_POINT           0x04
#define F3DEX3_G_MWO_PERSPNORM          0x06
#define F3DEX3_G_MWO_FRESNEL_SCALE      0x0C
#define F3DEX3_G_MWO_FRESNEL_OFFSET     0x0E
#define F3DEX3_G_MWO_ATTR_OFFSET_S      0x10
#define F3DEX3_G_MWO_ATTR_OFFSET_T      0x12

#define F3DEX3_A_G_MWO_ATTR_OFFSET_Z      0x14
#define F3DEX3_A_G_MWO_ALPHA_COMPARE_CULL 0x16
#define F3DEX3_A_G_MWO_NORMALS_MODE       0x18
#define F3DEX3_A_G_MWO_LAST_MAT_DL_ADDR   0x1A

#define F3DEX3_B_G_MWO_ALPHA_COMPARE_CULL 0x14
#define F3DEX3_B_G_MWO_LAST_MAT_DL_ADDR   0x16

#define F3DEX3_G_MAX_LIGHTS 9

#define F3DEX3_A_G_AMBOCCLUSION          0x00000040
#define F3DEX3_A_G_ATTROFFSET_Z_ENABLE   0x00000080
#define F3DEX3_A_G_ATTROFFSET_ST_ENABLE  0x00000100

#define F3DEX3_B_G_ATTROFFSET_ST_ENABLE  0x00000080
#define F3DEX3_B_G_AMBOCCLUSION          0x00000100

void F3DEX3_Init();
void F3DEX3_MoveMem( u32 w0, u32 w1 );
void F3DEX3_MoveWord( u32 w0, u32 w1 );
void F3DEX3_TriStrip( u32 w0, u32 w1 );
void F3DEX3_TriFan( u32 w0, u32 w1 );
void F3DEX3_TriSnake( u32 w0, u32 w1 );
void F3DEX3_LightToRDP( u32 w0, u32 w1 );

#endif