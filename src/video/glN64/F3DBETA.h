/**
 * glN64_GX - F3DBETA.h
 * Copyright (C) 2003 Orkin
 *
 * Ported from GLideN64 (uCodes/F3DBETA.h)
 *
**/

#ifndef F3DBETA_H
#define F3DBETA_H
#include "Types.h"

#define F3DBETA_PERSPNORM	0xB4
#define F3DBETA_RDPHALF_1	0xB3
#define F3DBETA_RDPHALF_2	0xB2
#define F3DBETA_TRI2		0xB1

void F3DBETA_Vtx( u32 w0, u32 w1 );
void F3DBETA_Tri1( u32 w0, u32 w1 );
void F3DBETA_Tri2( u32 w0, u32 w1 );
void F3DBETA_Quad( u32 w0, u32 w1 );
void F3DBETA_Perpnorm( u32 w0, u32 w1 );
void F3DBETA_Init();

#endif
