/**
 * wii64-ps3 - compatibility.c
 * Game compatibility database.
 *
 * CRC1 values from N64 ROM headers. Only CRC1 is used for lookup
 * (CRC2 is a secondary check, rarely needed for disambiguation).
 *
 * Sources: mupen64plus GameInfo, GoodTools, No-Intro DATs
 **/

#include "compatibility.h"
#include "rom.h"
#include "../core/n64_memory/memory.h"
#include <string.h>

static const GameCompatEntry CompatDB[] = {
	/* === PERFECT (4) === */
	{ sl(0xE2353D88), "Yoshi's Story (U)",        "YSUE", COMPAT_PERFECT, "" },
	{ sl(0x5C3388C8), "Pilotwings 64 (U)",        "PWUE", COMPAT_PERFECT, "" },
	{ sl(0x5BD43848), "Star Fox 64 (U)",          "SFUE", COMPAT_PERFECT, "" },
	{ sl(0x20A28DA4), "F-Zero X (U)",             "FZUE", COMPAT_PERFECT, "" },
	{ sl(0x57C207DC), "1080 Snowboarding (U)",    "TSUE", COMPAT_PERFECT, "" },
	{ sl(0x39E50726), "Wave Race 64 (U)",         "WRUE", COMPAT_PERFECT, "" },

	/* === GOOD (3) === */
	{ sl(0x93564B0E), "The Legend of Zelda - Ocarina of Time (U)",        "ZLUE", COMPAT_GOOD, "Warp effect minor glitch" },
	{ sl(0x7B42D874), "The Legend of Zelda - Ocarina of Time (E)",        "ZLPE", COMPAT_GOOD, "Warp effect minor glitch" },
	{ sl(0xEC943251), "The Legend of Zelda - Ocarina of Time (J)",        "ZLJE", COMPAT_GOOD, "Warp effect minor glitch" },
	{ sl(0xB20257A0), "The Legend of Zelda - OoT Master Quest (U)",       "ZMUE", COMPAT_GOOD, "Warp effect minor glitch" },
	{ sl(0x27C18287), "The Legend of Zelda - Majora's Mask (U)",          "ZMUE", COMPAT_GOOD, "N64 logo alpha, blur FX" },
	{ sl(0x53E2D283), "The Legend of Zelda - Majora's Mask (E)",          "ZMPE", COMPAT_GOOD, "N64 logo alpha, blur FX" },
	{ sl(0x5085C0D1), "The Legend of Zelda - Majora's Mask (J)",          "ZMJE", COMPAT_GOOD, "N64 logo alpha, blur FX" },
	{ sl(0xD6B46678), "Super Mario 64 (U)",                              "SM6E", COMPAT_GOOD, "Eye alpha, DECAL combiner" },
	{ sl(0x8F5C8DBF), "Super Mario 64 (E)",                              "SM6P", COMPAT_GOOD, "Eye alpha, DECAL combiner" },
	{ sl(0x4B4E3B18), "Super Mario 64 (J)",                              "SM6J", COMPAT_GOOD, "Eye alpha, DECAL combiner" },

	/* === PLAYABLE (2) === */
	{ sl(0x1AE44A37), "Mario Kart 64 (U)",          "NSME", COMPAT_PLAYABLE, "Shadows=cuadrado negro, nubes=rect blanco, minimap rayas, posicion=rect naranja" },
	{ sl(0xE80AD93A), "Mario Kart 64 (E)",          "NMAE", COMPAT_PLAYABLE, "Shadows=cuadrado negro, nubes=rect blanco, minimap rayas, posicion=rect naranja" },
	{ sl(0x2577C7D4), "Mario Kart 64 (E) v2",       "NMAE", COMPAT_PLAYABLE, "Shadows=cuadrado negro, nubes=rect blanco, minimap rayas, posicion=rect naranja" },
	{ sl(0x2C4F5C7A), "Mario Kart 64 (J)",          "NMAJ", COMPAT_PLAYABLE, "Shadows=cuadrado negro, nubes=rect blanco, minimap rayas, posicion=rect naranja" },
	{ sl(0xDCBC50D1), "GoldenEye 007 (U)",          "NZOE", COMPAT_PLAYABLE, "TLB fix applied" },
	{ sl(0x0414CA61), "GoldenEye 007 (E)",          "NZOP", COMPAT_PLAYABLE, "TLB fix applied" },
	{ sl(0xA24F4CF1), "GoldenEye 007 (J)",          "NZOJ", COMPAT_PLAYABLE, "TLB fix applied" },
	{ sl(0xC2E9AA9A), "Banjo-Tooie (U)",            "BTUE", COMPAT_PLAYABLE, "Shadow combiner + FB tex" },
	{ sl(0xC9176D39), "Banjo-Tooie (E)",            "BTPE", COMPAT_PLAYABLE, "Shadow combiner + FB tex" },
	{ sl(0x155B7CDF), "Banjo-Tooie (J)",            "BTJE", COMPAT_PLAYABLE, "Shadow combiner + FB tex" },
	{ sl(0xA6B6E4F5), "Banjo-Kazooie (U)",          "BKUE", COMPAT_PLAYABLE, "" },
	{ sl(0x71C2465B), "Paper Mario (U)",             "PMUE", COMPAT_PLAYABLE, "" },
	{ sl(0x7E4C5899), "Paper Mario (E)",             "PMPE", COMPAT_PLAYABLE, "" },
	{ sl(0x1F4B4239), "StarCraft 64 (U)",           "SCUE", COMPAT_PLAYABLE, "" },
	{ sl(0x1F4B423A), "Perfect Dark (U)",           "PDUE", COMPAT_PLAYABLE, "" },
	{ sl(0xD93B06C4), "Diddy Kong Racing (U)",      "DKRE", COMPAT_PLAYABLE, "" },
	{ sl(0xE7561600), "Diddy Kong Racing (E)",      "DKRP", COMPAT_PLAYABLE, "" },

	/* === BOOTABLE (1) === */
	{ sl(0xA1E4E188), "Donkey Kong 64 (U)",         "DK6E", COMPAT_BOOTABLE, "FB textures, may crash" },
	{ sl(0x34C7B426), "Donkey Kong 64 (E)",         "DK6P", COMPAT_BOOTABLE, "FB textures, may crash" },
	{ sl(0xCA12B547), "Pokemon Snap (U)",            "PSUE", COMPAT_BOOTABLE, "Controller + picture hack applied" },
	{ sl(0x7BB18D40), "Pokemon Snap (A)",            "PSAE", COMPAT_BOOTABLE, "Controller + picture hack applied" },
	{ sl(0x4FF5976F), "Pokemon Snap (E)",            "PSPE", COMPAT_BOOTABLE, "Controller + picture hack applied" },
	{ sl(0xEC0F690D), "Pokemon Snap (J)",            "PSJE", COMPAT_BOOTABLE, "Controller + picture hack applied" },
	{ sl(0x8ECC02F0), "Top Gear Hyper-Bike (U)",     "THBU", COMPAT_BOOTABLE, "Playable fix applied" },
	{ sl(0x5F3F49C6), "Top Gear Hyper-Bike (E)",     "THBE", COMPAT_BOOTABLE, "Playable fix applied" },
	{ sl(0x845B0269), "Top Gear Hyper-Bike (J)",     "THBJ", COMPAT_BOOTABLE, "Playable fix applied" },
	{ sl(0xD741CD80), "Top Gear Overdrive (U)",      "TOOU", COMPAT_BOOTABLE, "Playable fix applied" },
	{ sl(0xD09BA538), "Top Gear Overdrive (E)",      "TOOE", COMPAT_BOOTABLE, "Playable fix applied" },
	{ sl(0x0578F24F), "Top Gear Overdrive (J)",      "TOOJ", COMPAT_BOOTABLE, "Playable fix applied" },
	{ sl(0x308DFEC8), "World Driver Championship (U)", "WDCE", COMPAT_BOOTABLE, "OsRecvMesg NOP applied" },
	{ sl(0xAC062778), "World Driver Championship (E)", "WDPE", COMPAT_BOOTABLE, "OsRecvMesg NOP applied" },

	/* === BROKEN (0) === */

	/* Terminator sentinel */
	{ 0, NULL, NULL, 0, NULL }
};

const GameCompatEntry* Compat_Lookup(u32 crc1) {
	if (!crc1) return NULL;

	const GameCompatEntry *entry = CompatDB;
	while (entry->name) {
		if (entry->crc1 == crc1)
			return entry;
		entry++;
	}
	return NULL;
}

const char* Compat_RatingString(u8 rating) {
	switch (rating) {
		case COMPAT_PERFECT:  return "Perfecto";
		case COMPAT_GOOD:     return "Bueno";
		case COMPAT_PLAYABLE: return "Jugable";
		case COMPAT_BOOTABLE: return "Arranca";
		case COMPAT_BROKEN:   return "Roto";
		default:              return "???";
	}
}

char Compat_RatingChar(u8 rating) {
	switch (rating) {
		case COMPAT_PERFECT:  return 'P';
		case COMPAT_GOOD:     return 'B';
		case COMPAT_PLAYABLE: return 'J';
		case COMPAT_BOOTABLE: return 'A';
		case COMPAT_BROKEN:   return 'R';
		default:              return '?';
	}
}