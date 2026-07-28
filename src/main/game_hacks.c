/**
 * wii64-ps3 - game_hacks.c
 * Game-specific hacks ported from Wii64 (emukidid).
 * Memory patches to work around core inaccuracy for specific ROMs.
 *
 * Wii64 source: https://github.com/emukidid/Wii64/blob/master/main/gamehacks.c
 **/

#include "game_hacks.h"
#include "../core/n64_memory/memory.h"
#include "GameHackManager.h"
#include "rom.h"
#include <string.h>
#include <stdio.h>

/* Global legacy instance */
GameHacks gameHacks;

static u32 zelda_subscreen_address = 0;

/* --- Memory write helper functions (ported from Wii64) --- */

static void compare_hword_write_hword(u32 compAddr, u32 destAddr, u16 compVal, u16 destVal)
{
	u16 hval;

	address = compAddr;
	read_hword_in_memory();
	hval = hword;
	if (hval == compVal) {
		address = destAddr;
		hword = destVal;
		write_hword_in_memory();
	}
}

static void compare_hword_write_byte(u32 compAddr, u32 destAddr, u16 compVal, u8 destVal)
{
	u16 hval;

	address = compAddr;
	read_hword_in_memory();
	hval = hword;
	if (hval == compVal) {
		address = destAddr;
		byte = destVal;
		write_byte_in_memory();
	}
}

static void compare_byte_write_byte(u32 addr, u8 compVal, u8 destVal)
{
	u8 bval;

	address = addr;
	read_byte_in_memory();
	bval = byte;
	if (bval == compVal) {
		byte = destVal;
		write_byte_in_memory();
	}
}

/* --- Game-specific hack functions --- */

/* Pokemon Snap (U) */
static void hack_pkm_snap_u(void)
{
	/* Pass 1st Level and Controller Fix */
	compare_hword_write_byte(0x80382D1C, 0x80382D0F, 0x802C, 0);
	/* Make Picture selectable */
	compare_hword_write_hword(0x801E3184, 0x801E3184, 0x2881, 0x2001);
	compare_hword_write_hword(0x801E3186, 0x801E3186, 0x0098, 0x0001);
}

/* Pokemon Snap (A) */
static void hack_pkm_snap_a(void)
{
	/* Pass 1st Level and Controller Fix */
	compare_hword_write_byte(0x80382D1C, 0x80382D0F, 0x802C, 0);
	/* Make Picture selectable */
	compare_hword_write_hword(0x801E3C44, 0x801E3C44, 0x2881, 0x2001);
	compare_hword_write_hword(0x801E3C46, 0x801E3C46, 0x0098, 0x0001);
}

/* Pokemon Snap (E) */
static void hack_pkm_snap_e(void)
{
	/* Pass 1st Level and Controller Fix */
	compare_hword_write_byte(0x80381BFC, 0x80381BEF, 0x802C, 0);
	/* Make Picture selectable */
	compare_hword_write_hword(0x801E3824, 0x801E3824, 0x2881, 0x2001);
	compare_hword_write_hword(0x801E3826, 0x801E3826, 0x0098, 0x0001);
}

/* Pokemon Snap (J) */
static void hack_pkm_snap_j(void)
{
	/* Pass 1st Level and Controller Fix */
	compare_hword_write_byte(0x8036D22C, 0x8036D21F, 0x802A, 0);
	/* Make Picture selectable */
	compare_hword_write_hword(0x801E1EC4, 0x801E1EC4, 0x2881, 0x2001);
	compare_hword_write_hword(0x801E1EC6, 0x801E1EC6, 0x0098, 0x0001);
}

/* Top Gear Hyper-Bike (E) */
static void hack_topgear_hb_e(void)
{
	compare_hword_write_byte(0x800021EE, 0x800021EE, 0x0001, 0);
}

/* Top Gear Hyper-Bike (J) */
static void hack_topgear_hb_j(void)
{
	compare_hword_write_byte(0x8000225A, 0x8000225A, 0x0001, 0);
}

/* Top Gear Hyper-Bike (U) */
static void hack_topgear_hb_u(void)
{
	compare_hword_write_hword(0x800021EA, 0x800021EA, 0x0001, 0);
}

/* Top Gear Overdrive (E) */
static void hack_topgear_od_e(void)
{
	compare_hword_write_hword(0x80001AB2, 0x80001AB2, 0x0001, 0);
}

/* Top Gear Overdrive (J) */
static void hack_topgear_od_j(void)
{
	hword = 0;
	address = 0x800F7BD4;
	write_hword_in_memory();
	address = 0x800F7BD6;
	write_hword_in_memory();
	address = 0x80001B4E;
	write_hword_in_memory();
}

/* Top Gear Overdrive (U) */
static void hack_topgear_od_u(void)
{
	compare_hword_write_hword(0x80001B4E, 0x80001B4E, 0x0001, 0);
}

/* World Driver Championship (U) */
static void hack_worlddriver_u(void)
{
	word = 0;
	address = 0x80023FE4;
	write_word_in_memory();
}

/* World Driver Championship (E) */
static void hack_worlddriver_e(void)
{
	word = 0;
	address = 0x800241D4;
	write_word_in_memory();
}

/* Pilot Wings 64 (U) - Removes shadow below planes */
static void hack_pilotwings_u(void)
{
	compare_byte_write_byte(0x80263B41, 0x12, 0xFF);
	compare_byte_write_byte(0x802643C1, 0x12, 0xFF);
	compare_byte_write_byte(0x80264581, 0x12, 0xFF);
	compare_byte_write_byte(0x80263FC1, 0x12, 0xFF);
	compare_byte_write_byte(0x80263E81, 0x12, 0xFF);
	compare_byte_write_byte(0x80263E41, 0x12, 0xFF);
	compare_byte_write_byte(0x80264181, 0x12, 0xFF);
	compare_byte_write_byte(0x80264381, 0x12, 0xFF);
	compare_byte_write_byte(0x80264281, 0x12, 0xFF);
	compare_byte_write_byte(0x802639C1, 0x20, 0xFF);
	compare_byte_write_byte(0x802640C1, 0x20, 0xFF);
	compare_byte_write_byte(0x80264541, 0x20, 0xFF);
	compare_byte_write_byte(0x80264201, 0x20, 0xFF);
	compare_byte_write_byte(0x80263D81, 0x20, 0xFF);
	compare_byte_write_byte(0x80263F81, 0x20, 0xFF);
	compare_byte_write_byte(0x80263A81, 0x29, 0xFF);
	compare_byte_write_byte(0x80264101, 0x29, 0xFF);
	compare_byte_write_byte(0x80263E01, 0x29, 0xFF);
	compare_byte_write_byte(0x80264201, 0x29, 0xFF);
	compare_byte_write_byte(0x80264441, 0x29, 0xFF);
	compare_byte_write_byte(0x80263F81, 0x29, 0xFF);
	compare_byte_write_byte(0x802647C1, 0x29, 0xFF);
	compare_byte_write_byte(0x80264941, 0x29, 0xFF);
	compare_byte_write_byte(0x80264281, 0x29, 0xFF);
	compare_byte_write_byte(0x802639C1, 0x3D, 0xFF);
	compare_byte_write_byte(0x802641C1, 0x3D, 0xFF);
	compare_byte_write_byte(0x80263D41, 0x3D, 0xFF);
	compare_byte_write_byte(0x80263EC1, 0x3D, 0xFF);
	compare_byte_write_byte(0x80263F01, 0x3D, 0xFF);
}

/* Pilot Wings 64 (E) - Removes shadow below planes */
static void hack_pilotwings_e(void)
{
	compare_byte_write_byte(0x80264361, 0x12, 0xFF);
	compare_byte_write_byte(0x80264BE1, 0x12, 0xFF);
	compare_byte_write_byte(0x80264DA1, 0x12, 0xFF);
	compare_byte_write_byte(0x802647E1, 0x12, 0xFF);
	compare_byte_write_byte(0x802646A1, 0x12, 0xFF);
	compare_byte_write_byte(0x80264661, 0x12, 0xFF);
	compare_byte_write_byte(0x802649A1, 0x12, 0xFF);
	compare_byte_write_byte(0x80264BA1, 0x12, 0xFF);
	compare_byte_write_byte(0x80264AA1, 0x12, 0xFF);
	compare_byte_write_byte(0x802641E1, 0x20, 0xFF);
	compare_byte_write_byte(0x802648E1, 0x20, 0xFF);
	compare_byte_write_byte(0x80264D61, 0x20, 0xFF);
	compare_byte_write_byte(0x80264A21, 0x20, 0xFF);
	compare_byte_write_byte(0x802645A1, 0x20, 0xFF);
	compare_byte_write_byte(0x802647A1, 0x20, 0xFF);
	compare_byte_write_byte(0x802642A1, 0x29, 0xFF);
	compare_byte_write_byte(0x80264921, 0x29, 0xFF);
	compare_byte_write_byte(0x80264621, 0x29, 0xFF);
	compare_byte_write_byte(0x80264A21, 0x29, 0xFF);
	compare_byte_write_byte(0x80264C61, 0x29, 0xFF);
	compare_byte_write_byte(0x802647A1, 0x29, 0xFF);
	compare_byte_write_byte(0x80264FE1, 0x29, 0xFF);
	compare_byte_write_byte(0x80265161, 0x29, 0xFF);
	compare_byte_write_byte(0x80264AA1, 0x29, 0xFF);
	compare_byte_write_byte(0x802641E1, 0x3D, 0xFF);
	compare_byte_write_byte(0x802649E1, 0x3D, 0xFF);
	compare_byte_write_byte(0x80264561, 0x3D, 0xFF);
	compare_byte_write_byte(0x802646E1, 0x3D, 0xFF);
	compare_byte_write_byte(0x80264721, 0x3D, 0xFF);
}

/* Pilot Wings 64 (J) - Removes shadow below planes */
static void hack_pilotwings_j(void)
{
	compare_byte_write_byte(0x802639B1, 0x12, 0xFF);
	compare_byte_write_byte(0x80264231, 0x12, 0xFF);
	compare_byte_write_byte(0x802643F1, 0x12, 0xFF);
	compare_byte_write_byte(0x80263E31, 0x12, 0xFF);
	compare_byte_write_byte(0x80263CF1, 0x12, 0xFF);
	compare_byte_write_byte(0x80263CB1, 0x12, 0xFF);
	compare_byte_write_byte(0x80263FF1, 0x12, 0xFF);
	compare_byte_write_byte(0x802641F1, 0x12, 0xFF);
	compare_byte_write_byte(0x802640F1, 0x12, 0xFF);
	compare_byte_write_byte(0x80263831, 0x20, 0xFF);
	compare_byte_write_byte(0x80263F31, 0x20, 0xFF);
	compare_byte_write_byte(0x802643B1, 0x20, 0xFF);
	compare_byte_write_byte(0x80264071, 0x20, 0xFF);
	compare_byte_write_byte(0x80263BF1, 0x20, 0xFF);
	compare_byte_write_byte(0x80263DF1, 0x20, 0xFF);
	compare_byte_write_byte(0x802638F1, 0x29, 0xFF);
	compare_byte_write_byte(0x80263F71, 0x29, 0xFF);
	compare_byte_write_byte(0x80263C71, 0x29, 0xFF);
	compare_byte_write_byte(0x80264071, 0x29, 0xFF);
	compare_byte_write_byte(0x802642B1, 0x29, 0xFF);
	compare_byte_write_byte(0x80263DF1, 0x29, 0xFF);
	compare_byte_write_byte(0x80264631, 0x29, 0xFF);
	compare_byte_write_byte(0x802647B1, 0x29, 0xFF);
	compare_byte_write_byte(0x802640F1, 0x29, 0xFF);
	compare_byte_write_byte(0x80263831, 0x3D, 0xFF);
	compare_byte_write_byte(0x80264031, 0x3D, 0xFF);
	compare_byte_write_byte(0x80263BB1, 0x3D, 0xFF);
	compare_byte_write_byte(0x80263D31, 0x3D, 0xFF);
	compare_byte_write_byte(0x80263D71, 0x3D, 0xFF);
}

/* Zelda OoT subscreen fix (runtime, called per-frame) */
static void hack_zelda_oot(void)
{
	if (zelda_subscreen_address && rdramb) {
		rdramb[zelda_subscreen_address] = 2;
	}
}

/* --- Public API --- */

void GameHacks_ApplyPerFrame(void)
{
	if (zelda_subscreen_address) {
		hack_zelda_oot();
	}
}

void GameHacks_Detect(void)
{
	u32 curCRC[2];

	memset(&gameHacks, 0, sizeof(GameHacks));
	zelda_subscreen_address = 0;

	if (!ROM_HEADER)
		return;

	/* Copy GameHackManager fields to legacy struct */
	extern GameHackManager *g_game_hack_mgr;
	if (g_game_hack_mgr) {
		const GameHacks *hacks = GameHackManager_GetHacks(g_game_hack_mgr);
		if (hacks) {
			gameHacks.useFBE = hacks->useFBE;
			gameHacks.forceAlphaTest = hacks->forceAlphaTest;
			gameHacks.forceOpaqueAlphaCvg = hacks->forceOpaqueAlphaCvg;
			gameHacks.fixVertexNaN = hacks->fixVertexNaN;
			gameHacks.unpackAlignment = hacks->unpackAlignment;
			gameHacks.alphaBlendMode = hacks->alphaBlendMode;
			gameHacks.depthBits = hacks->depthBits;
			gameHacks.internalWidth = hacks->internalWidth;
			gameHacks.internalHeight = hacks->internalHeight;
			gameHacks.displayWidth = hacks->displayWidth;
			gameHacks.displayHeight = hacks->displayHeight;
		}
	}

	/* Read CRC values for game-specific memory patches */
	curCRC[0] = ROM_HEADER->CRC1;
	curCRC[1] = ROM_HEADER->CRC2;

	/* Pokemon Snap (U) */
	if (curCRC[0] == 0xCA12B547 && curCRC[1] == 0x71FA4EE4) {
		hack_pkm_snap_u();
	}
	/* Pokemon Snap (J) */
	else if (curCRC[0] == 0xEC0F690D && curCRC[1] == 0x32A7438C) {
		hack_pkm_snap_j();
	}
	/* Pokemon Snap (A) */
	else if (curCRC[0] == 0x7BB18D40 && curCRC[1] == 0x83138559) {
		hack_pkm_snap_a();
	}
	/* Pokemon Snap (E) */
	else if (curCRC[0] == 0x4FF5976F && curCRC[1] == 0xACF559D8) {
		hack_pkm_snap_e();
	}
	/* Top Gear Hyper-Bike (E) */
	else if (curCRC[0] == 0x5F3F49C6 && curCRC[1] == 0x0DC714B0) {
		hack_topgear_hb_e();
	}
	/* Top Gear Hyper-Bike (J) */
	else if (curCRC[0] == 0x845B0269 && curCRC[1] == 0x57DE9502) {
		hack_topgear_hb_j();
	}
	/* Top Gear Hyper-Bike (U) */
	else if (curCRC[0] == 0x8ECC02F0 && curCRC[1] == 0x7F8BDE81) {
		hack_topgear_hb_u();
	}
	/* Top Gear Overdrive (E) */
	else if (curCRC[0] == 0xD09BA538 && curCRC[1] == 0x1C1A5489) {
		hack_topgear_od_e();
	}
	/* Top Gear Overdrive (J) */
	else if (curCRC[0] == 0x0578F24F && curCRC[1] == 0x9175BF17) {
		hack_topgear_od_j();
	}
	/* Top Gear Overdrive (U) */
	else if (curCRC[0] == 0xD741CD80 && curCRC[1] == 0xACA9B912) {
		hack_topgear_od_u();
	}
	/* World Driver Championship (U) */
	else if (curCRC[0] == 0x308DFEC8 && curCRC[1] == 0xCE2EB5F6) {
		hack_worlddriver_u();
	}
	/* World Driver Championship (E) */
	else if (curCRC[0] == 0xAC062778 && curCRC[1] == 0xDFADFCB8) {
		hack_worlddriver_e();
	}
	/* Pilot Wings 64 (U) */
	else if (curCRC[0] == 0xC851961C && curCRC[1] == 0x78FCAAFA) {
		hack_pilotwings_u();
	}
	/* Pilot Wings 64 (J) */
	else if (curCRC[0] == 0x09CC4801 && curCRC[1] == 0xE42EE491) {
		hack_pilotwings_j();
	}
	/* Pilot Wings 64 (E) */
	else if (curCRC[0] == 0x1AA05AD5 && curCRC[1] == 0x46F52D80) {
		hack_pilotwings_e();
	}
	/* Zelda OoT - subscreen fix (all versions, runtime per-frame) */
	else if (strncmp((char *)ROM_HEADER->nom, "THE LEGEND OF ZELDA", 19) == 0) {
		zelda_subscreen_address = 0;
		if (curCRC[0] == 0xEC7011B7 && curCRC[1] == 0x7616D72B) {
			/* OoT (U) + (J) (V1.0) */
			zelda_subscreen_address = 0x1DA5CB;
		} else if (curCRC[0] == 0xD43DA81F && curCRC[1] == 0x021E1E19) {
			/* OoT (U) + (J) (V1.1) */
			zelda_subscreen_address = 0x1DA78B;
		} else if (curCRC[0] == 0x693BA2AE && curCRC[1] == 0xB7F14E9F) {
			/* OoT (U) + (J) (V1.2) */
			zelda_subscreen_address = 0x1DAE8B;
		} else if (curCRC[0] == 0xB044B569 && curCRC[1] == 0x373C1985) {
			/* OoT (E) (V1.0) */
			zelda_subscreen_address = 0x1D860B;
		} else if (curCRC[0] == 0xB2055FBD && curCRC[1] == 0x0BAB4E0C) {
			/* OoT (E) (V1.1) */
			zelda_subscreen_address = 0x1D864B;
		} else if (curCRC[0] == 0x1D4136F3 && curCRC[1] == 0xAF63EEA9) {
			/* OoT Master Quest (E) (GC) */
			zelda_subscreen_address = 0x1D8F4B;
		} else if (curCRC[0] == 0x09465AC3 && curCRC[1] == 0xF8CB501B) {
			/* OoT (E) (GC) */
			zelda_subscreen_address = 0x1D8F8B;
		} else if (curCRC[0] == 0xF3DD35BA && curCRC[1] == 0x4152E075) {
			/* OoT (U) (GC) */
			zelda_subscreen_address = 0x1DB78B;
		} else if (curCRC[0] == 0xF034001A && curCRC[1] == 0xAE47ED06) {
			/* OoT Master Quest (U) (GC) */
			zelda_subscreen_address = 0x1DB74B;
		} else if (curCRC[0] == 0xF7F52DB8 && curCRC[1] == 0x2195E636) {
			/* Zelda GC (J) */
			zelda_subscreen_address = 0x1DB78B;
		} else if (curCRC[0] == 0xF611F4BA && curCRC[1] == 0xC584135C) {
			/* Zelda GC (J) */
			zelda_subscreen_address = 0x1DB78B;
		} else if (curCRC[0] == 0xF43B45BA && curCRC[1] == 0x2F0E9B6F) {
			/* Zelda GC Ura (J) */
			zelda_subscreen_address = 0x1DB78B;
		}
	}

	if (zelda_subscreen_address) {
		printf("[GameHacks] Zelda OoT subscreen fix at RDRAM 0x%06X\n", zelda_subscreen_address);
	}
}
