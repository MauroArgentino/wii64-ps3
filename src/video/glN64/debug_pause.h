/**
 * debug_pause.h - Debug Pause Mode & Polygon Inspector
 *
 * Allows pausing emulation with R1+L1 and inspecting polygons one by one.
 * Press R1+L1 to enter/exit pause. When paused, R2 advances one polygon.
 */

#ifndef DEBUG_PAUSE_H
#define DEBUG_PAUSE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Max polygons recorded in the ring buffer */
#define DEBUG_POLY_RING_SIZE 256

/* Polygon snapshot captured at draw time */
typedef struct {
    uint32_t index;           /* polygon number in current frame */
    uint32_t num_vertices;    /* vertices submitted (3, 6, 9...) */
    float    z_values[3];     /* Z of first 3 vertices */
    float    uv_min_s, uv_max_s;
    float    uv_min_t, uv_max_t;
    uint32_t shader_mode;     /* 0=decal, 1=passTex, 2=passColor, 3=modulate */
    float    alpha_mode;      /* 0.0 or 1.0 */
    uint32_t blend_enable;
    uint32_t blend_src, blend_dst;
    uint32_t alpha_test_enable;
    uint32_t alpha_test_func;
    uint32_t alpha_test_ref;
    /* Texture info */
    uint32_t tex_width, tex_height;
    uint32_t tex_format;      /* RSX GCM texture format */
    uint32_t tex_offset;      /* TMEM offset */
    uint8_t  tex_dirty;       /* 1 = texture changed since last polygon */
    uint8_t  uses_t0, uses_t1;
    uint8_t  padding;
} debug_poly_info_t;

/* Global debug pause state */
typedef struct {
    volatile uint32_t paused;            /* 1 = emulation paused */
    volatile uint32_t step;              /* 1 = advance one polygon, then pause */
    volatile uint32_t poly_index;        /* current polygon being inspected */
    volatile uint32_t poly_count;        /* total polygons in current frame */
    volatile uint32_t frame_count;       /* frames since pause entered */
    volatile uint32_t frame_tri_index;   /* per-frame triangle counter (reset each frame) */
    volatile uint32_t frame_frozen;      /* 1 = frame is frozen, 0 = need to re-render */
    debug_poly_info_t ring[DEBUG_POLY_RING_SIZE];
    uint32_t ring_write;                 /* next write index */
} debug_pause_state_t;

extern debug_pause_state_t g_debug_pause;

/**
 * Call from OGL_DrawTriangles to record polygon info.
 */
void debug_pause_record_poly(const debug_poly_info_t *info);

/**
 * Check if we should block (called from OGL_DrawTriangles).
 * Returns 1 if drawing should proceed, 0 if blocked.
 */
int debug_pause_should_draw(void);

/**
 * Poll PS3 pad for debug hotkeys (R1+L1 toggle, R2 step).
 * Called from VI_UpdateScreen.
 */
void debug_pause_poll(void);

/**
 * Called at the start of each display list to reset per-frame counters.
 */
void debug_pause_begin_frame(void);

/**
 * CPU halt flag. When set, the interpreter loop spins without executing.
 * Set by debug_pause_poll (R2 step), cleared after one frame renders.
 */
extern volatile int debug_pause_cpu_halt;

/**
 * Called from VI_UpdateScreen after a step frame has been rendered.
 * Re-halts the CPU to freeze the frame.
 */
void debug_pause_on_frame_done(void);

#ifdef __cplusplus
}
#endif

#endif /* DEBUG_PAUSE_H */
