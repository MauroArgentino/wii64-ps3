/**
 * wii64-ps3 - game_hacks.c
 * Per-game rendering/memory hack detection.
 *
 * Identifies loaded ROM by CRC1 and sets flags in the global
 * 'gameHacks' struct.  All other code reads these flags instead
 * of doing its own CRC checks.
 **/

#include "game_hacks.h"
#include "../core/n64_memory/memory.h"
#include <string.h>

GameHacks gameHacks;

void GameHacks_Detect(void)
{
	memset(&gameHacks, 0, sizeof(GameHacks));

	if (!ROM_HEADER)
		return;

	u32 crc1 = ROM_HEADER->CRC1;

	/* --- GoldenEye 007 (US / EU / JP) --- */
	if (crc1 == sl(0xDCBC50D1) ||   /* US */
	    crc1 == sl(0x0414CA61) ||   /* EU */
	    crc1 == sl(0xA24F4CF1))     /* JP */
	{
		gameHacks.goldeneye_tlb = 1;
	}

	/* --- Banjo-Tooie (US / EU / JP) --- */
	if (crc1 == sl(0xC2E9AA9A) ||   /* US */
	    crc1 == sl(0xC9176D39) ||   /* EU */
	    crc1 == sl(0x155B7CDF))     /* JP */
	{
		gameHacks.banjo_tooie_shadow = 1;
		gameHacks.banjo_tooie_fbtex  = 1;
	}

	/* --- Zelda OoT (US MQ / US / EU / JP) --- */
	if (crc1 == sl(0xB20257A0) ||   /* US (MQ debug) */
	    crc1 == sl(0x93564B0E) ||   /* US */
	    crc1 == sl(0x7B42D874) ||   /* EU */
	    crc1 == sl(0xEC943251))     /* JP */
	{
		gameHacks.zelda_warp = 1;
	}

	/* --- Zelda Majora's Mask (US / EU / JP) --- */
	if (crc1 == sl(0x27C18287) ||   /* US */
	    crc1 == sl(0x53E2D283) ||   /* EU */
	    crc1 == sl(0x5085C0D1))     /* JP */
	{
		gameHacks.zelda_warp = 1;
		gameHacks.mm_fix_logo_alpha = 1;
		gameHacks.mm_fbtex = 1;
	}
}
