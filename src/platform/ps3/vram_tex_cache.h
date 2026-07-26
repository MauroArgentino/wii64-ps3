/**
 * vram_tex_cache.h - VRAM Texture Hash Cache for PS3 RSX
 *
 * Persistent VRAM cache keyed by content hash (xxHash64).
 * Avoids re-uploading identical textures to RSX VRAM.
 */

#ifndef VRAM_TEX_CACHE_H
#define VRAM_TEX_CACHE_H

#include <stdint.h>
#include <rsx/rsx.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration */
#define VRAM_TEX_CACHE_MAX_ENTRIES    4096
#define VRAM_TEX_CACHE_MAX_SIZE_MB    64    /* PS3 has 256MB VRAM total; reserve rest for framebuffers, etc. */
#define VRAM_TEX_HASH_SEED            0x9E3779B97F4A7C15ULL

/* VRAM cache entry */
typedef struct {
    uint64_t hash;              /* xxHash64 of texture data (+palette if CI) */
    uint32_t vram_offset;       /* RSX VRAM offset (from rsxAddressToOffset) */
    uint32_t vram_size;         /* Size in bytes (aligned to 128B pitch) */
    uint32_t width, height;     /* Texture dimensions */
    uint32_t pitch;             /* Row pitch in bytes (128-byte aligned) */
    uint8_t  format;            /* GCM texture format */
    uint8_t  has_palette;       /* 1 if CI texture (palette included in hash) */
    uint32_t palette_crc;       /* CRC of palette if CI */
    uint32_t last_frame_used;   /* For LRU eviction */
    uint32_t ref_count;         /* Reference count (0 = evictable) */
} vram_tex_entry_t;

/* Cache statistics */
typedef struct {
    uint32_t hits;
    uint32_t misses;
    uint32_t evictions;
    uint32_t total_entries;
    uint32_t total_vram_bytes;
    uint32_t total_uploads;
} vram_tex_stats_t;

/* Initialize VRAM texture cache (call once at startup) */
void vram_tex_cache_init(void);

/* Shutdown and free all VRAM allocations */
void vram_tex_cache_destroy(void);

/* Lookup or upload texture to VRAM.
 * Input:
 *   src_data    - Pointer to texture data in RDRAM (already converted to RGBA8888)
 *   src_size    - Size of src_data in bytes
 *   width, height - Texture dimensions
 *   format      - GCM texture format (e.g., GCM_TEXTURE_FORMAT_A8R8G8B8 | GCM_TEXTURE_FORMAT_LIN)
 *   has_palette - 1 if CI texture (palette data follows src_data)
 *   palette_crc - CRC of palette if has_palette
 * Output:
 *   out_offset  - VRAM offset to use with rsxLoadTexture
 *   out_pitch   - Row pitch in bytes
 * Returns: 1 = cache hit (reused existing), 0 = cache miss (new upload) */
int vram_tex_cache_get_or_upload(
    const void *src_data, uint32_t src_size,
    uint32_t width, uint32_t height,
    uint32_t format,
    uint8_t has_palette, uint32_t palette_crc,
    uint32_t *out_offset, uint32_t *out_pitch
);

/* Invalidate cache entries overlapping a RDRAM range.
 * Call when CPU writes to texture memory in RDRAM. */
void vram_tex_cache_invalidate_range(uint32_t rdram_addr, uint32_t size);

/* Get statistics for OSD */
void vram_tex_cache_get_stats(vram_tex_stats_t *out_stats);

/* Advance frame counter (call once per frame) for LRU */
void vram_tex_cache_new_frame(void);

/* Debug: dump all entries */
void vram_tex_cache_dump(void);

#ifdef __cplusplus
}
#endif

#endif /* VRAM_TEX_CACHE_H */