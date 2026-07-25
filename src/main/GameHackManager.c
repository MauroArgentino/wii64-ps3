/**
 * GameHackManager.c - Dynamic per-ROM hack management
 * Replaces static tables with malloc/free lifecycle per loaded ROM.
 */

#include "GameHackManager.h"
#include "compatibility.h"
#include "rom.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Internal manager structure */
struct GameHackManager {
    GameHacks *current_hacks;
    char current_rom_id[5];
};

/* Global instance pointer (used by all callers via extern) */
GameHackManager *g_game_hack_mgr = NULL;

/* Static hack database (migrated from compatibility.c) */
typedef struct {
    const char *rom_id;
    GameHacks hacks;
} HackEntry;

static const HackEntry s_hack_database[] = {
    /* Mario Kart 64 (EU v2) - 0x2577C7D4 */
    { ROM_ID_MK64_EU, {
        .useFBE = 1,
        .alphaBlendMode = 1,        // GREATER 0.1 for HUD
        .depthBits = 16,
        .unpackAlignment = 1,       // I4 minimap font
        .forceAlphaTest = 1,        // Minimap, HUD sprites
        .fixVertexNaN = 1,
        .forceOpaqueAlphaCvg = 1,   // Character select backgrounds
        .internalWidth = 640,
        .internalHeight = 480,
        .displayWidth = 1920,
        .displayHeight = 1080,
    }},

    /* Kirby 64 (USA) */
    { ROM_ID_KIRBY64_US, {
        .useFBE = 1,
        .alphaBlendMode = 2,        // GEQUAL 1 for character faces
        .depthBits = 16,
        .unpackAlignment = 1,
        .forceAlphaTest = 1,        // Character face sprites
        .fixVertexNaN = 1,
        .forceOpaqueAlphaCvg = 1,   // Character model parts
        .internalWidth = 640,
        .internalHeight = 480,
        .displayWidth = 1920,
        .displayHeight = 1080,
    }},

    /* Ocarina of Time (US) */
    { ROM_ID_ZELDA_OOT_US, {
        .useFBE = 1,
        .alphaBlendMode = 1,
        .depthBits = 16,
        .unpackAlignment = 1,
        .forceAlphaTest = 1,
        .fixVertexNaN = 0,
        .forceOpaqueAlphaCvg = 0,
        .internalWidth = 640,
        .internalHeight = 480,
        .displayWidth = 1920,
        .displayHeight = 1080,
    }},

    /* Super Mario 64 (US) */
    { ROM_ID_SM64_US, {
        .useFBE = 1,
        .alphaBlendMode = 1,
        .depthBits = 16,
        .unpackAlignment = 1,
        .forceAlphaTest = 1,
        .fixVertexNaN = 0,
        .forceOpaqueAlphaCvg = 0,
        .internalWidth = 640,
        .internalHeight = 480,
        .displayWidth = 1920,
        .displayHeight = 1080,
    }},

    /* GoldenEye 007 (US) */
    { ROM_ID_GOLDENEYE_US, {
        .useFBE = 1,
        .alphaBlendMode = 1,
        .depthBits = 16,
        .unpackAlignment = 1,
        .forceAlphaTest = 1,
        .fixVertexNaN = 0,
        .forceOpaqueAlphaCvg = 0,
        .internalWidth = 640,
        .internalHeight = 480,
        .displayWidth = 1920,
        .displayHeight = 1080,
    }},

    /* Perfect Dark (US) */
    { ROM_ID_PD_US, {
        .useFBE = 1,
        .alphaBlendMode = 1,
        .depthBits = 16,
        .unpackAlignment = 1,
        .forceAlphaTest = 1,
        .fixVertexNaN = 0,
        .forceOpaqueAlphaCvg = 0,
        .internalWidth = 640,
        .internalHeight = 480,
        .displayWidth = 1920,
        .displayHeight = 1080,
    }},

    /* Banjo-Kazooie (US) */
    { ROM_ID_BANJO_US, {
        .useFBE = 1,
        .alphaBlendMode = 1,
        .depthBits = 16,
        .unpackAlignment = 1,
        .forceAlphaTest = 1,
        .fixVertexNaN = 0,
        .forceOpaqueAlphaCvg = 0,
        .internalWidth = 640,
        .internalHeight = 480,
        .displayWidth = 1920,
        .displayHeight = 1080,
    }},

    /* Terminator */
    { NULL, {0} }
};

/* Manager implementation */
int GameHackManager_Init(GameHackManager **out_mgr) {
    if (!out_mgr) return -1;

    GameHackManager *mgr = (GameHackManager *)malloc(sizeof(GameHackManager));
    if (!mgr) return -1;

    mgr->current_hacks = NULL;
    mgr->current_rom_id[0] = '\0';

    *out_mgr = mgr;
    return 0;
}

int GameHackManager_LoadForROM(GameHackManager *mgr, const char *rom_id_4) {
    if (!mgr) return -1;

    const GameHacks *default_hacks = NULL;

    /* Try to find ROM in compatibility database by ID */
    const HackEntry *entry = s_hack_database;
    while (entry->rom_id) {
        if (rom_id_4 && strncmp(entry->rom_id, rom_id_4, 4) == 0) {
            default_hacks = &entry->hacks;
            break;
        }
        entry++;
    }

    /* Free previous hacks if any */
    if (mgr->current_hacks) {
        free(mgr->current_hacks);
        mgr->current_hacks = NULL;
    }

    /* Allocate new hacks */
    mgr->current_hacks = (GameHacks *)malloc(sizeof(GameHacks));
    if (!mgr->current_hacks) return -1;

    if (default_hacks) {
        *mgr->current_hacks = *default_hacks;
        if (rom_id_4) {
            strncpy(mgr->current_rom_id, rom_id_4, 4);
            mgr->current_rom_id[4] = '\0';
        }
        printf("[GameHackManager] Loaded hacks for ROM: %.4s\n", rom_id_4 ? rom_id_4 : "????");
    } else {
        /* Safe defaults */
        GameHacks def = {
            .useFBE = 1,
            .alphaBlendMode = 1,
            .depthBits = 16,
            .unpackAlignment = 1,
            .forceAlphaTest = 1,
            .fixVertexNaN = 0,
            .forceOpaqueAlphaCvg = 0,
            .internalWidth = 640,
            .internalHeight = 480,
            .displayWidth = 1920,
            .displayHeight = 1080,
        };
        *mgr->current_hacks = def;
        if (rom_id_4) {
            strncpy(mgr->current_rom_id, rom_id_4, 4);
            mgr->current_rom_id[4] = '\0';
        }
        printf("[GameHackManager] Using default hacks for ROM: %.4s\n", rom_id_4 ? rom_id_4 : "????");
    }
    return 0;
}

const GameHacks *GameHackManager_GetHacks(const GameHackManager *mgr) {
    return mgr ? mgr->current_hacks : NULL;
}

GameHacks *GameHackManager_GetMutableHacks(GameHackManager *mgr) {
    return mgr ? mgr->current_hacks : NULL;
}

/**
 * Load hacks by CRC1 (looks up ROM ID from compatibility database).
 */
int GameHackManager_LoadForCRC(GameHackManager *mgr, u32 crc1) {
    if (!mgr || !crc1) return -1;

    const GameCompatEntry *entry = Compat_Lookup(crc1);
    if (!entry || !entry->rom_id) {
        printf("[GameHackManager] CRC 0x%08X not in compat DB\n", crc1);
        return GameHackManager_LoadForROM(mgr, "????");
    }

    return GameHackManager_LoadForROM(mgr, entry->rom_id);
}

void GameHackManager_Unload(GameHackManager *mgr) {
    if (mgr && mgr->current_hacks) {
        free(mgr->current_hacks);
        mgr->current_hacks = NULL;
        mgr->current_rom_id[0] = '\0';
    }
}

void GameHackManager_Destroy(GameHackManager *mgr) {
    if (mgr) {
        GameHackManager_Unload(mgr);
        free(mgr);
    }
}