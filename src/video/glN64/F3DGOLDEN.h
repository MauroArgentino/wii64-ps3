/**
 * glN64_GX - F3DGOLDEN.h
 * Copyright (C) 2003 Orkin
 *
 * F3DGOLDEN: GoldenEye 007 custom microcode
 * Based on F3D with two modifications:
 *   - 0xBD (POPMTX) repurposed as second MoveWord
 *   - 0xB1 (Tri4) replaced with packed TriX command
**/

#ifndef F3DGOLDEN_H
#define F3DGOLDEN_H

#define F3DGOLDEN_TRIX		0xB1

void F3DGOLDEN_TriX( u32 w0, u32 w1 );
void F3DGOLDEN_Init();

#endif
