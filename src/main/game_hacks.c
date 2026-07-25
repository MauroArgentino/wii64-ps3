/**
 * wii64-ps3 - game_hacks.c (Compatibility Wrapper)
 * Legacy per-game hack detection - now backed by GameHackManager.
 **/

#include "game_hacks.h"
#include "../core/n64_memory/memory.h"
#include "GameHackManager.h"
#include "rom.h"
#include <string.h>
#include <stdio.h>

/* Global legacy instance */
GameHacks gameHacks;

/* Initialize legacy gameHacks from GameHackManager */
void GameHacks_Detect(void)
{
	memset(&gameHacks, 0, sizeof(GameHacks));

	if (!ROM_HEADER)
		return;

	extern GameHackManager *g_game_hack_mgr;
	if (!g_game_hack_mgr) return;

	const GameHacks *hacks = GameHackManager_GetHacks(g_game_hack_mgr);
	if (!hacks) return;

	/* Copy relevant fields from new GameHacks to legacy gameHacks */
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