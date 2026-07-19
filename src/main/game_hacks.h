/**
 * wii64-ps3 - game_hacks.h
 * Per-game rendering/memory hack flags.
 *
 * After loading a ROM, call GameHacks_Detect() to populate the
 * global 'gameHacks' struct based on the ROM's CRC1.  Rendering
 * and memory code checks these flags instead of hardcoding CRC
 * comparisons scattered across the codebase.
 *
 * To add a new game:
 *   1. Add a new u8 flag field to the GameHacks struct below.
 *   2. Add an else-if branch in GameHacks_Detect() using the CRC.
 *   3. Check the flag in the relevant rendering/memory code.
 **/

#ifndef GAME_HACKS_H
#define GAME_HACKS_H

#include "winlnxdefs.h"
#include "rom.h"

typedef struct _game_hacks {
	/* Memory */
	u8 goldeneye_tlb;         /* GoldenEye 007: TLB address fix */

	/* Combiner / color */
	u8 banjo_tooie_shadow;    /* Banjo-Tooie: shadow combiner fix */
	u8 banjo_tooie_fbtex;     /* Banjo-Tooie: force framebuffer textures */

	/* Textures */
	u8 zelda_warp;            /* Zelda OoT/MM: warp texture TMEM fix */

	/* Alpha / blending */
	u8 force_alpha_opaque;    /* Force all DECAL alpha = 1.0 */

	/* Zelda Majora's Mask specific */
	u8 mm_fix_logo_alpha;     /* MM: fix N64 logo alpha LOAD(TEXEL*_ALPHA) bug */

	/* Reserved for future per-game flags */
	u8 reserved[7];
} GameHacks;

extern GameHacks gameHacks;

/* Call once after ROM_HEADER is populated (inside loadROM). */
void GameHacks_Detect(void);

#endif
