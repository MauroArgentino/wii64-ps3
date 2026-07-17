// rsxutil.cpp - Thin compatibility layer between Tiny3D and existing code.
// Tiny3D owns RSX initialization, buffer management, and flip logic.
// This file provides the flip wrapper, RSX memory heap management, and
// a bump allocator that bypasses the prebuilt libtiny3d.a allocator.

#include <stdio.h>
#include <string.h>
#include <tiny3d.h>
#include <rsx/gcm_sys.h>
#include "rsxutil.h"

// GCM context pointer - set once after tiny3d_Init() in main.cpp.
gcmContextData *gcm_context = NULL;

// GCM configuration - refreshed from gcmGetConfiguration() after Tiny3D init.
// The prebuilt libtiny3d.a may use a different gcmConfiguration struct layout,
// causing its internal allocator to read wrong values and always return NULL.
// We use this copy with the CORRECT struct layout from current PSL1GHT headers.
gcmConfiguration gcm_cfg;

// Saved heap position from the bump allocator.
static void *heap_save = NULL;

// Refresh gcm_cfg from the hardware. Call AFTER tiny3d_Init() has set up GCM.
void rsxutil_refresh_cfg(void) {
	gcmGetConfiguration(&gcm_cfg);
	printf("[RSX] cfg refreshed: localAddr=%08x localSize=%08x (%dMB)\n",
		(u32)(u64)gcm_cfg.localAddress, gcm_cfg.localSize,
		gcm_cfg.localSize/(1024*1024));
	fflush(stdout);
}

// Save current bump allocator position.
void rsxutil_save_heap(void) {
	heap_save = heap_pointer;
	u32 used = (u32)((u64)heap_pointer - (u64)gcm_cfg.localAddress);
	u32 remain = gcm_cfg.localSize - used;
	printf("[RSX] heap saved: pos=%08x\n", (u32)heap_save);
	printf("[RSX] RSX local: base=%08x size=%08x (%dMB)\n",
		(u32)(u64)gcm_cfg.localAddress, gcm_cfg.localSize,
		gcm_cfg.localSize/(1024*1024));
	printf("[RSX] Used: %08x (%dKB), remaining: %08x (%dKB)\n",
		used, used/1024, remain, remain/1024);
	fflush(stdout);
}

// Restore bump allocator to saved position.
void rsxutil_restore_heap(void) {
	if (heap_save) {
		printf("[RSX] heap restored: %08x -> %08x (reclaimed %dKB)\n",
			(u32)heap_pointer, (u32)heap_save,
			(int)((u32)heap_pointer - (u32)heap_save)/1024);
		heap_pointer = heap_save;
	}
}

// Existing code calls flip() with no args.
// Tiny3D provides tiny3d_Flip() which handles:
//   - Tiny3d_End() (flush pending polygons)
//   - waitFlip() (wait for previous flip)
//   - gcmSetFlip + rsxFlushBuffer
//   - Buffer index swap
//   - setupRenderTarget for new back buffer
//   - Tiny3D state reset (shaders, vertex buffer, etc.)
void flip() {
	tiny3d_Flip();
}
