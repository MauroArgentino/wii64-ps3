#ifndef __RSXUTIL_H__
#define __RSXUTIL_H__

#include <ppu-types.h>
#include <rsx/rsx.h>
#include <tiny3d.h>
#include <libfont.h>

// Tiny3D owns the RSX context. Our code accesses it via this pointer,
// set once after tiny3d_Init().
extern gcmContextData *gcm_context;

// Redirect all existing 'context' references to our gcm_context.
// Tiny3D internally defines its own 'context' as tiny_gcmContextData*,
// which has identical layout but would cause a linker duplicate.
#define context gcm_context

// Tiny3D provides Video_Resolution; bridge to our display_width/height
#define display_width  Video_Resolution.width
#define display_height Video_Resolution.height

// NV40 Alpha Test register writes (alpha_test.cpp)
void rsxSetAlphaTestEnable(gcmContextData *ctx, u32 enable);
void rsxSetAlphaTestFunc(gcmContextData *ctx, u32 func);
void rsxSetAlphaTestRef(gcmContextData *ctx, u32 ref);

// Flip wrapper - calls tiny3d_Flip() which handles buffer swap,
// wait, render target setup, and Tiny3D state reset.
void flip();

// RSX local memory bump allocator.
// We bypass tiny3d_AllocTexture entirely because the prebuilt libtiny3d.a
// may have been compiled against a different PSL1GHT version where the
// gcmConfiguration struct layout differs. This causes its internal
// config.localSize to read from the wrong offset, making it always return NULL.
// Instead, we do our own bump allocation using the CURRENT gcmGetConfiguration.
extern void *heap_pointer;
extern gcmConfiguration gcm_cfg;  // Updated via rsxutil_refresh_cfg()

static inline void* compat_rsunalign(u32 alignment, u32 size)
{
	void *pointer = heap_pointer;
	pointer = (void *)((((u64) pointer) + (alignment-1)) & (-(s64)alignment));
	if ((u64) pointer + size > ((u64)(void *) gcm_cfg.localAddress) + gcm_cfg.localSize)
		return NULL;
	heap_pointer = (void *)((u64) pointer + size);
	return pointer;
}
#define rsxMemalign compat_rsunalign

static inline void compat_rsxFree(void* p) { (void)p; }
#define rsxFree compat_rsxFree

// Save current heap position. Call BEFORE OGL_Start allocates.
// At this point the heap includes Tiny3D internals + menu resources.
void rsxutil_save_heap(void);

// Restore heap to the saved position. Call AFTER OGL_Stop frees.
// This reclaims all OGL RSX memory while keeping menu resources intact.
void rsxutil_restore_heap(void);

// Refresh gcm_cfg from hardware. Call after tiny3d_Init().
void rsxutil_refresh_cfg(void);

#endif
