/**
 * wii64-ps3 - compatibility.h
 * Game compatibility database.
 *
 * Lookup table of known ROMs indexed by CRC1.
 * Each entry has a rating (0-4) and known issues description.
 *
 * Rating scale:
 *   0 = Roto     - No arranca o crashea
 *   1 = Arranca  - Arranca pero no jugable
 *   2 = Jugable  - Problemas notorios pero jugable
 *   3 = Bueno    - Jugable, menores bugs cosmeticos
 *   4 = Perfecto - Sin problemas visuales/sonoros
 *
 * To add a new game:
 *   1. Add an entry to the CompatDB array in compatibility.c
 *   2. Use CRC1 as the key, add name and rating
 *   3. Add notes about known issues
 **/

#ifndef COMPATIBILITY_H
#define COMPATIBILITY_H

#include "winlnxdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define COMPAT_BROKEN    0
#define COMPAT_BOOTABLE  1
#define COMPAT_PLAYABLE  2
#define COMPAT_GOOD      3
#define COMPAT_PERFECT   4

typedef struct _game_compat_entry {
	u32 crc1;
	const char *name;
	u8 rating;
	const char *notes;
} GameCompatEntry;

/* Lookup a game by CRC1. Returns NULL if not found. */
const GameCompatEntry* Compat_Lookup(u32 crc1);

/* Get a human-readable rating string. */
const char* Compat_RatingString(u8 rating);

/* Get a rating character for compact display. */
char Compat_RatingChar(u8 rating);

#ifdef __cplusplus
}
#endif

#endif
