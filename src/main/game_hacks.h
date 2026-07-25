/**
 * wii64-ps3 - game_hacks.h (Compatibility Wrapper)
 * Legacy per-game hack flags - now backed by GameHackManager.
 *
 * This header provides backward compatibility for code using the old
 * global 'gameHacks' struct. New code should use GameHackManager directly.
 **/

#ifndef GAME_HACKS_H
#define GAME_HACKS_H

#include "winlnxdefs.h"
#include "GameHackManager.h"

/* Legacy struct - maps to GameHacks in GameHackManager.h */
typedef GameHacks _game_hacks;

/* Legacy field name mappings to new GameHacks struct */
#define goldeneye_tlb          useFBE
#define banjo_tooie_shadow     forceAlphaTest
#define banjo_tooie_fbtex      forceOpaqueAlphaCvg
#define zelda_warp             forceAlphaTest
#define force_alpha_opaque     forceOpaqueAlphaCvg
#define mm_fix_logo_alpha      fixVertexNaN
#define mm_fbtex               forceOpaqueAlphaCvg
#define reserved               internalWidth

/* Global instance (backward compatibility) */
extern GameHacks gameHacks;

/* Initialize from GameHackManager (call once after ROM load) */
void GameHacks_Detect(void);

#endif /* GAME_HACKS_H */