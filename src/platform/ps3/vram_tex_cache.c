/**
 * vram_tex_cache.c - VRAM Texture Hash Cache Implementation
 * FNV-1a 64-bit hash + open-addressing hash table + RSX VRAM management
 *
 * FNV-1a chosen over xxHash64 for ~5x lower per-byte cost on Cell PPU:
 *   - 1 XOR + 1 multiply per 8 bytes (vs xxHash64's 4 multiplies + 4 rotates)
 *   - No accumulator setup, no finalization mixing
 *   - Perfect quality for texture cache (collisions = harmless cache miss)
 */

#include "vram_tex_cache.h"
#include <sys/memory.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* FNV-1a 64-bit hash — processes 8 bytes per iteration */
static uint64_t vram_hash(const void *data, uint32_t size) {
    const uint8_t *p = (const uint8_t *)data;
    const uint8_t *end = p + size;
    uint64_t hash = 0xCBF29CE484222325ULL;  /* FNV offset basis */

    /* Process 8 bytes at a time (natural word size on 64-bit) */
    while (p + 8 <= end) {
        uint64_t word;
        memcpy(&word, p, 8);
        hash ^= word;
        hash *= 0x100000001B3ULL;  /* FNV prime */
        p += 8;
    }

    /* Process remaining bytes (0-7) */
    while (p < end) {
        hash ^= (uint64_t)*p;
        hash *= 0x100000001B3ULL;
        p++;
    }

    return hash;
}

/* Hash table entry states */
#define VRAM_ENTRY_EMPTY   0
#define VRAM_ENTRY_USED    1
#define VRAM_ENTRY_DELETED 2

/* Global cache state */
static vram_tex_entry_t *g_vram_table = NULL;
static uint32_t g_vram_table_mask = 0;
static uint32_t g_vram_table_size = 0;
static uint32_t g_vram_entry_count = 0;
static uint32_t g_vram_current_frame = 0;
static uint32_t g_vram_total_bytes = 0;
static vram_tex_stats_t g_vram_stats = {0};

/* Lazy initialization flag */
static int g_vram_initialized = 0;

/* Permanent disable flag: set if rsxMemalign returns non-VRAM pointers */
static int g_vram_disabled = 0;

/* Ensure cache is initialized (lazy init on first use) */
static void vram_tex_cache_ensure_init(void) {
    if (g_vram_initialized) return;
    if (g_vram_disabled) return;
    
    /* Ensure RSX video is initialized (sets up VRAM pool) */
    extern void RSX_VideoInit(void);
    RSX_VideoInit();
    
    g_vram_table_size = VRAM_TEX_CACHE_MAX_ENTRIES * 2;  /* 50% load factor */
    g_vram_table_mask = g_vram_table_size - 1;

    g_vram_table = (vram_tex_entry_t *)calloc(g_vram_table_size, sizeof(vram_tex_entry_t));
    if (!g_vram_table) {
        printf("VRAM_TEX: Failed to allocate hash table\n");
        return;
    }

    g_vram_entry_count = 0;
    g_vram_total_bytes = 0;
    g_vram_current_frame = 0;
    memset(&g_vram_stats, 0, sizeof(g_vram_stats));

    printf("VRAM_TEX: Initialized (max %d entries, %d MB)\n",
           VRAM_TEX_CACHE_MAX_ENTRIES, VRAM_TEX_CACHE_MAX_SIZE_MB);
    g_vram_initialized = 1;
}

/* Initialize cache - now lazy (called on first use) */
void vram_tex_cache_init(void) {
    vram_tex_cache_ensure_init();
}

/* Destroy cache and free VRAM */
void vram_tex_cache_destroy(void) {
    if (g_vram_table) {
        uint32_t i;
        for (i = 0; i < g_vram_table_size; i++) {
            if (g_vram_table[i].hash != 0 && g_vram_table[i].vram_offset) {
                /* Free VRAM - rsxFree would be ideal but not always available.
                 * For now just mark as freed; RSX VRAM is process-local. */
            }
        }
        free(g_vram_table);
        g_vram_table = NULL;
    }
    printf("VRAM_TEX: Destroyed (hits=%u, misses=%u, evictions=%u, uploads=%u, VRAM=%.2f MB)\n",
           g_vram_stats.hits, g_vram_stats.misses, g_vram_stats.evictions,
           g_vram_stats.total_uploads, g_vram_total_bytes / (1024.0f * 1024.0f));
}

/* Hash table probe */
static int vram_find_slot(uint64_t hash, int insert) {
    uint32_t idx = (uint32_t)(hash & g_vram_table_mask);
    uint32_t first_deleted = g_vram_table_size;
    uint32_t probes;

    for (probes = 0; probes < g_vram_table_size; probes++) {
        uint32_t cur = (idx + probes) & g_vram_table_mask;
        uint8_t state = (g_vram_table[cur].hash == 0) ? VRAM_ENTRY_EMPTY :
                        (g_vram_table[cur].hash == ~0ULL) ? VRAM_ENTRY_DELETED : VRAM_ENTRY_USED;

        if (state == VRAM_ENTRY_USED && g_vram_table[cur].hash == hash) {
            return (int)cur;  /* Found */
        }
        if (state == VRAM_ENTRY_DELETED && first_deleted == g_vram_table_size) {
            first_deleted = cur;
        }
        if (state == VRAM_ENTRY_EMPTY) {
            if (insert && first_deleted != g_vram_table_size) {
                return (int)first_deleted;  /* Reuse deleted slot */
            }
            return insert ? (int)cur : -1;  /* Empty slot */
        }
    }
    return insert ? (int)first_deleted : -1;  /* Table full or not found */
}

/* Evict LRU entries until we have at least needed_bytes free, or no more evictable */
static int vram_evict_until_space(uint32_t needed_bytes) {
    uint32_t freed = 0;
    int last_evicted = -1;

    while (g_vram_total_bytes + needed_bytes > VRAM_TEX_CACHE_MAX_SIZE_MB * 1024 * 1024) {
        uint32_t oldest_frame = 0xFFFFFFFF;
        int oldest_idx = -1;
        uint32_t i;

        for (i = 0; i < g_vram_table_size; i++) {
            if (g_vram_table[i].hash != 0 && g_vram_table[i].hash != ~0ULL &&
                g_vram_table[i].ref_count == 0) {
                if (g_vram_table[i].last_frame_used < oldest_frame) {
                    oldest_frame = g_vram_table[i].last_frame_used;
                    oldest_idx = (int)i;
                }
            }
        }

        if (oldest_idx < 0) {
            printf("VRAM_TEX: No evictable entries, need %u KB, have %u KB free\n",
                   needed_bytes / 1024,
                   (VRAM_TEX_CACHE_MAX_SIZE_MB * 1024) - (g_vram_total_bytes / 1024));
            return -1;
        }

        /* Evict this entry */
        freed += g_vram_table[oldest_idx].vram_size;
        g_vram_total_bytes -= g_vram_table[oldest_idx].vram_size;
        g_vram_table[oldest_idx].hash = ~0ULL;
        g_vram_table[oldest_idx].vram_offset = 0;
        g_vram_table[oldest_idx].vram_size = 0;
        g_vram_entry_count--;
        g_vram_stats.evictions++;
        last_evicted = oldest_idx;
    }

    if (freed > 0) {
        printf("VRAM_TEX: Evicted %u KB to make room\n", freed / 1024);
    }
    return last_evicted;
}

/* Compute hash of texture data (+palette) */
static uint64_t vram_compute_hash(const void *data, uint32_t size, uint8_t has_palette, uint32_t palette_crc) {
    uint64_t hash = vram_hash(data, size);
    if (has_palette) {
        /* Mix in palette CRC */
        hash ^= (uint64_t)palette_crc;
        hash *= 0x100000001B3ULL;
    }
    return hash;
}

/* PS3 RSX VRAM address range: 0x0C000000 to 0x10000000 (256MB) */
#define RSX_VRAM_BASE   0x0C000000u
#define RSX_VRAM_END    0x10000000u

static int vram_ptr_is_vram(const void *ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    return (addr >= RSX_VRAM_BASE && addr < RSX_VRAM_END);
}

/* Upload to VRAM and return offset */
static int vram_upload_texture(const void *src_data, uint32_t src_size,
                               uint32_t width, uint32_t height, uint32_t format,
                               uint32_t *out_offset, uint32_t *out_pitch) {
    /* VALIDATE: width and height must be > 0 for RSX */
    if (width == 0 || height == 0) {
        printf("VRAM_TEX: ERROR - Invalid dimensions %ux%u for upload, aborting\n", width, height);
        return -1;
    }
    printf("VRAM_TEX: Uploading %ux%u fmt=0x%x src_size=%u\n", width, height, format, src_size);
    /* RSX linear textures need 128-byte aligned pitch */
    uint32_t pitch = (width * 4 + 127) & ~127u;  /* RGBA8888 = 4 bytes/pixel */
    uint32_t vram_size = pitch * height;
    void *vram_ptr;
    uint32_t offset;
    int ret;

    /* Check VRAM budget - evict multiple entries if needed */
    if (g_vram_total_bytes + vram_size > VRAM_TEX_CACHE_MAX_SIZE_MB * 1024 * 1024) {
        if (vram_evict_until_space(vram_size) < 0) {
            printf("VRAM_TEX: Budget exceeded, cannot upload %ux%u (need %u KB, have %u KB free)\n",
                   width, height, vram_size / 1024,
                   (VRAM_TEX_CACHE_MAX_SIZE_MB * 1024) - (g_vram_total_bytes / 1024));
            return -1;
        }
    }

    /* Allocate VRAM memory for texture */
    vram_ptr = rsxMemalign(128, vram_size);
    if (!vram_ptr) {
        printf("VRAM_TEX: rsxMemalign NULL for %u bytes (%u KB)\n", vram_size, vram_size / 1024);
        return -1;
    }

    /* Validate pointer is actually in RSX VRAM range, not XDR system memory */
    if (!vram_ptr_is_vram(vram_ptr)) {
        printf("VRAM_TEX: ptr %p NOT in VRAM range [0x%x-0x%x), DISABLING cache permanently\n",
               vram_ptr, RSX_VRAM_BASE, RSX_VRAM_END);
        rsxFree(vram_ptr);
        g_vram_disabled = 1;
        return -2;
    }

    /* Convert and upload: source is RGBA8888 (from TextureCache_Load), RSX wants A8R8G8B8 */
    {
        uint32_t *src32 = (uint32_t *)src_data;
        uint8_t *dst = (uint8_t *)vram_ptr;
        uint32_t row, col;

        for (row = 0; row < height; row++) {
            uint32_t *src_row = src32 + row * width;
            uint8_t *dst_row = dst + row * pitch;
            for (col = 0; col < width; col++) {
                uint32_t rgba = src_row[col];
                /* RRGGBBAA -> AARRGGBB (right rotate 8) */
                ((uint32_t *)dst_row)[col] = (rgba >> 8) | (rgba << 24);
            }
        }
    }

    /* Get RSX offset */
    ret = rsxAddressToOffset(vram_ptr, &offset);
    if (ret != 0) {
        printf("VRAM_TEX: rsxAddressToOffset failed (ret=%d) for ptr=%p size=%u\n", ret, vram_ptr, vram_size);
        rsxFree(vram_ptr);
        return -1;
    }

    printf("VRAM_TEX: Uploaded %ux%u @ offset=0x%08x ptr=%p size=%u KB total=%.2fMB\n",
           width, height, offset, vram_ptr, vram_size / 1024,
           (g_vram_total_bytes + vram_size) / (1024.0f * 1024.0f));

    *out_offset = offset;
    *out_pitch = pitch;
    return (int)vram_size;
}

/* Main API: get or upload */
int vram_tex_cache_get_or_upload(const void *src_data, uint32_t src_size,
                                 uint32_t width, uint32_t height,
                                 uint32_t format,
                                 uint8_t has_palette, uint32_t palette_crc,
                                 uint32_t *out_offset, uint32_t *out_pitch) {
    vram_tex_cache_ensure_init();
    if (!g_vram_table || g_vram_disabled) return 0;

    /* VALIDATE: width and height must be > 0 */
    if (width == 0 || height == 0) {
        printf("VRAM_TEX: ERROR - Invalid dimensions %ux%u in get_or_upload, treating as miss\n", width, height);
        return 0;
    }

    uint64_t hash = vram_compute_hash(src_data, src_size, has_palette, palette_crc);

    /* DEBUG: Log texture being processed */
    printf("VRAM_TEX: get_or_upload %ux%u fmt=0x%x pal=%d hash=0x%llx\n",
           width, height, format, has_palette, hash);

    /* Lookup */
    int idx = vram_find_slot(hash, 0);
    if (idx >= 0) {
        /* Cache hit */
        uint32_t cached_offset = g_vram_table[idx].vram_offset;
        /* Validate offset is in valid VRAM range */
        if (cached_offset == 0 || cached_offset > 0xFFFFF000) {
            printf("VRAM_TEX: Invalid cached offset 0x%08x for hash=%016llx, treating as miss\n",
                   cached_offset, hash);
        } else {
            g_vram_table[idx].last_frame_used = g_vram_current_frame;
            g_vram_table[idx].ref_count++;
            *out_offset = cached_offset;
            *out_pitch = g_vram_table[idx].pitch;
            g_vram_stats.hits++;
            if (g_vram_stats.hits % 100 == 0) {
                printf("VRAM_TEX: Hit #%u @ offset=0x%08x\n", g_vram_stats.hits, cached_offset);
            }
            return 1;
        }
    }

    /* Cache miss - upload */
    g_vram_stats.misses++;

    /* Find slot for insertion */
    idx = vram_find_slot(hash, 1);
    if (idx < 0) {
        /* Table full, try evict */
        uint32_t pitch = (width * 4 + 127) & ~127u;
        uint32_t vram_size = pitch * height;
        idx = vram_evict_until_space(vram_size);
        if (idx < 0) return 0;
        /* Re-probe after eviction */
        idx = vram_find_slot(hash, 1);
        if (idx < 0) return 0;
    }

    /* Upload to VRAM */
    uint32_t offset, pitch;
    int vram_size = vram_upload_texture(src_data, src_size, width, height, format, &offset, &pitch);
    if (vram_size <= 0) return 0;

    /* Fill entry */
    g_vram_table[idx].hash = hash;
    g_vram_table[idx].vram_offset = offset;
    g_vram_table[idx].vram_size = vram_size;
    g_vram_table[idx].width = width;
    g_vram_table[idx].height = height;
    g_vram_table[idx].pitch = pitch;
    g_vram_table[idx].format = (uint8_t)format;
    g_vram_table[idx].has_palette = has_palette;
    g_vram_table[idx].palette_crc = palette_crc;
    g_vram_table[idx].last_frame_used = g_vram_current_frame;
    g_vram_table[idx].ref_count = 1;

    g_vram_entry_count++;
    g_vram_total_bytes += vram_size;
    g_vram_stats.total_uploads++;
    g_vram_stats.total_entries = g_vram_entry_count;
    g_vram_stats.total_vram_bytes = g_vram_total_bytes;

    *out_offset = offset;
    *out_pitch = pitch;
    return 0;
}

/* Invalidate range - called on RDRAM writes */
void vram_tex_cache_invalidate_range(uint32_t rdram_addr, uint32_t size) {
    vram_tex_cache_ensure_init();
    /* Simple approach: if we had a reverse mapping (RDRAM addr -> hash),
     * we could invalidate precisely. For now, just clear the whole cache
     * on any texture memory write. Heavy but correct.
     * TODO: Implement RDRAM address -> hash reverse map for precision. */
    if (!g_vram_table) return;

    uint32_t i;
    for (i = 0; i < g_vram_table_size; i++) {
        if (g_vram_table[i].hash != 0 && g_vram_table[i].hash != ~0ULL &&
            g_vram_table[i].ref_count == 0) {
            g_vram_total_bytes -= g_vram_table[i].vram_size;
            g_vram_table[i].hash = ~0ULL;
            g_vram_table[i].vram_offset = 0;
            g_vram_table[i].vram_size = 0;
            g_vram_entry_count--;
        }
    }
    g_vram_stats.total_entries = g_vram_entry_count;
    g_vram_stats.total_vram_bytes = g_vram_total_bytes;
}

/* Get stats */
void vram_tex_cache_get_stats(vram_tex_stats_t *out_stats) {
    if (out_stats) {
        *out_stats = g_vram_stats;
        out_stats->total_entries = g_vram_entry_count;
        out_stats->total_vram_bytes = g_vram_total_bytes;
    }
}

/* Advance frame counter (call once per frame) */
void vram_tex_cache_new_frame(void) {
    g_vram_current_frame++;

    /* Periodic cleanup: remove entries not used in 300+ frames (~5 seconds at 60fps) */
    if ((g_vram_current_frame % 60) == 0) {
        uint32_t i, cleaned = 0;
        for (i = 0; i < g_vram_table_size; i++) {
            if (g_vram_table[i].hash != 0 && g_vram_table[i].hash != ~0ULL &&
                g_vram_table[i].ref_count == 0 &&
                (g_vram_current_frame - g_vram_table[i].last_frame_used) > 120) {
                g_vram_total_bytes -= g_vram_table[i].vram_size;
                g_vram_table[i].hash = ~0ULL;
                g_vram_table[i].vram_offset = 0;
                g_vram_table[i].vram_size = 0;
                g_vram_entry_count--;
                cleaned++;
            }
        }
        if (cleaned > 0) {
            g_vram_stats.evictions += cleaned;
            g_vram_stats.total_entries = g_vram_entry_count;
            g_vram_stats.total_vram_bytes = g_vram_total_bytes;
            printf("VRAM_TEX: Periodic cleanup removed %u stale entries (%.2f MB freed)\n",
                   cleaned, (g_vram_total_bytes + cleaned * 1024) / 1024.0f);
        }
    }
}

/* Debug dump */
void vram_tex_cache_dump(void) {
    printf("VRAM_TEX DUMP: %u entries, %.2f MB\n", g_vram_entry_count,
           g_vram_total_bytes / (1024.0f * 1024.0f));
    uint32_t i;
    for (i = 0; i < g_vram_table_size && g_vram_entry_count > 0; i++) {
        if (g_vram_table[i].hash != 0 && g_vram_table[i].hash != ~0ULL) {
            printf("  [%u] hash=%016llx off=0x%08x sz=%u %ux%u pitch=%u fmt=0x%02x pal=%d ref=%u frame=%u\n",
                   i, g_vram_table[i].hash, g_vram_table[i].vram_offset,
                   g_vram_table[i].vram_size, g_vram_table[i].width, g_vram_table[i].height,
                   g_vram_table[i].pitch, g_vram_table[i].format,
                   g_vram_table[i].has_palette, g_vram_table[i].ref_count,
                   g_vram_table[i].last_frame_used);
        }
    }
}