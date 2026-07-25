/**
 * GameHackManager.h - Dynamic per-ROM hack management
 * Replaces static tables with malloc/free lifecycle per loaded ROM.
 */

#ifndef GAME_HACK_MANAGER_H
#define GAME_HACK_MANAGER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Per-ROM hack configuration (mutable at runtime) */
typedef struct {
    uint8_t useFBE;              // Framebuffer effects enable
    uint8_t alphaBlendMode;      // 0=off, 1=GREATER 0.1, 2=GEQUAL 1
    uint8_t depthBits;           // 16 or 24
    uint8_t unpackAlignment;     // 1 for I4/IA4 fonts, 4 default
    uint8_t forceAlphaTest;      // Enable alpha test for sprites
    uint8_t fixVertexNaN;        // Clamp NaN/Inf vertices
    uint8_t forceOpaqueAlphaCvg; // Force opaque for alphaCvgSel modes
    uint8_t internalWidth;       // Internal 3D render width (default 640)
    uint8_t internalHeight;      // Internal 3D render height (default 480)
    uint16_t displayWidth;       // Display width (1920 for 1080p)
    uint16_t displayHeight;      // Display height (1080 for 1080p)
} GameHacks;

/* Opaque manager handle */
typedef struct GameHackManager GameHackManager;

/**
 * Initialize the hack manager (call once at startup).
 * @param out_mgr Pointer to store manager handle.
 * @return 0 on success, -1 on failure.
 */
int GameHackManager_Init(GameHackManager **out_mgr);

/**
 * Load hacks for a specific ROM by 4-char ID.
 * @param mgr Manager handle.
 * @param rom_id_4 4-character ROM ID (e.g., "NSME", "CREE", "CLJE", "NK4E").
 * @return 0 on success, -1 if ID not found in database.
 */
int GameHackManager_LoadForROM(GameHackManager *mgr, const char *rom_id_4);

/**
 * Load hacks by CRC (looks up ROM ID from compatibility database).
 */
int GameHackManager_LoadForCRC(GameHackManager *mgr, u32 crc1);

/**
 * Get current active hacks (read-only).
 */
const GameHacks *GameHackManager_GetHacks(const GameHackManager *mgr);

/**
 * Get mutable hacks for runtime tweaking (e.g., settings menu).
 */
GameHacks *GameHackManager_GetMutableHacks(GameHackManager *mgr);

/**
 * Unload current hacks and free memory.
 * Call when ROM unloads or before loading new ROM.
 */
void GameHackManager_Unload(GameHackManager *mgr);

/**
 * Destroy manager and free all resources.
 */
void GameHackManager_Destroy(GameHackManager *mgr);

/* ROM ID constants for known games */
#define ROM_ID_MK64_US    "NSME"
#define ROM_ID_MK64_EU    "NMAE"
#define ROM_ID_MK64_JP    "NMAJ"
#define ROM_ID_KIRBY64_US "NK4E"
#define ROM_ID_KIRBY64_EU "NK4P"
#define ROM_ID_KIRBY64_JP "NK4J"
#define ROM_ID_ZELDA_OOT_US "NZSE"
#define ROM_ID_ZELDA_OOT_EU "NZSP"
#define ROM_ID_ZELDA_MM_US "ZLSE"
#define ROM_ID_SM64_US    "SM6E"
#define ROM_ID_SM64_EU    "SM6P"
#define ROM_ID_GOLDENEYE_US "NZOE"
#define ROM_ID_PD_US      "NPDE"
#define ROM_ID_BANJO_US   "NBKE"

#ifdef __cplusplus
}
#endif

#endif /* GAME_HACK_MANAGER_H */