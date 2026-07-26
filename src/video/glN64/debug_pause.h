/**
 * debug_pause.h - Debug Pause Mode & Polygon Inspector
 *
 * Two-phase freeze:
 *   1. L1+R1 arms capture → next frame renders all polys (no skip), recorded
 *   2. After that frame flips → freeze CPU permanently
 *   3. R2 navigates captured polygon list (CPU stays halted)
 *   4. L1+R1 again → resume game
 */

#ifndef DEBUG_PAUSE_H
#define DEBUG_PAUSE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Max polygons recorded per frozen frame */
#define DEBUG_POLY_RING_SIZE 512

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
    volatile uint32_t capturing;         /* 1 = capturing current frame */
    volatile uint32_t frame_frozen;      /* 1 = frame captured, CPU halted */
    volatile uint32_t poly_index;        /* current polygon being inspected */
    volatile uint32_t poly_count;        /* total polygons captured */
    volatile uint32_t frame_tri_index;   /* per-frame triangle counter */
    debug_poly_info_t ring[DEBUG_POLY_RING_SIZE];
    uint32_t ring_write;                 /* next write index */
} debug_pause_state_t;

extern debug_pause_state_t g_debug_pause;

/* Per-frame polygon counter (reset each display list) */
extern volatile uint32_t g_poly_counter;

/**
 * Call from OGL_DrawTriangles to record polygon info.
 */
void debug_pause_record_poly(const debug_poly_info_t *info);

/**
 * Called at the start of each display list to reset per-frame counters.
 */
void debug_pause_begin_frame(void);

/**
 * Called from VI_UpdateScreen after a capture frame has been flipped.
 * Transitions to frozen state.
 */
void debug_pause_on_frame_done(void);

/**
 * Poll PS3 pad for debug hotkeys (R1+L1 toggle, R2 step).
 * Called from VI_UpdateScreen when paused.
 */
void debug_pause_poll(void);

/**
 * CPU halt flag. When set, interpreter spins without executing.
 */
extern volatile int debug_pause_cpu_halt;

/**
 * Called from the interpreter halt loop when CPU is frozen.
 * Polls the pad directly since VI_UpdateScreen is not running.
 */
void debug_pause_poll_halt(void);

#ifdef __cplusplus
}
#endif

#endif /* DEBUG_PAUSE_H */
