/**
 * debug_pause.c - Debug Pause Mode & Polygon Inspector implementation
 *
 * Two-phase freeze:
 *   Phase 1 (CAPTURING): L1+R1 pressed → paused=1, capturing=1, CPU keeps running.
 *     Next frame: OGL_DrawTriangles records ALL polys, draws ALL (no skip).
 *   Phase 2 (FROZEN): VI_UpdateScreen flips capture frame → frame_frozen=1, cpu_halt=1.
 *     R2 navigates poly_index only. CPU never unhalts.
 */

#include "debug_pause.h"
#include <io/pad.h>
#include <string.h>

/* Global state */
debug_pause_state_t g_debug_pause;

/* Per-frame polygon counter (reset each display list) */
volatile uint32_t g_poly_counter = 0;

/* CPU halt flag - when set, interpreter spins without executing */
volatile int debug_pause_cpu_halt = 0;

/* PS3 pad raw button bits (must match controller-PS3.c) */
#define PS3_BTN_R1       (1<<3)
#define PS3_BTN_L1       (1<<2)
#define PS3_BTN_R2       (1<<1)
#define PS3_BTN_R1L1     (PS3_BTN_R1 | PS3_BTN_L1)

static uint32_t prev_buttons = 0;

/* Internal: reset all pause state */
static void reset_pause_state(void)
{
    g_debug_pause.paused = 0;
    g_debug_pause.capturing = 0;
    g_debug_pause.frame_frozen = 0;
    g_debug_pause.poly_index = 0;
    g_debug_pause.poly_count = 0;
    g_debug_pause.frame_tri_index = 0;
    g_debug_pause.ring_write = 0;
    g_poly_counter = 0;
    debug_pause_cpu_halt = 0;
}

void debug_pause_record_poly(const debug_poly_info_t *info)
{
    if (!g_debug_pause.paused) return;
    if (g_debug_pause.ring_write >= DEBUG_POLY_RING_SIZE) return;
    {
        uint32_t idx = g_debug_pause.ring_write;
        g_debug_pause.ring[idx] = *info;
        g_debug_pause.ring_write = idx + 1;
        g_debug_pause.poly_count = g_debug_pause.ring_write;
    }
}

/* Reset per-frame triangle counter. Called at start of each display list. */
void debug_pause_begin_frame(void)
{
    g_debug_pause.frame_tri_index = 0;
    g_poly_counter = 0;
}

/* Called from VI_UpdateScreen after the capture frame has been flipped. */
void debug_pause_on_frame_done(void)
{
    g_debug_pause.capturing = 0;
    g_debug_pause.frame_frozen = 1;
    debug_pause_cpu_halt = 1;
}

/* Check raw PS3 pad for debug hotkeys. Called from VI_UpdateScreen. */
void debug_pause_poll(void)
{
    padData paddata;
    uint32_t buttons, pressed;

    ioPadGetData(0, &paddata);
    if (!paddata.len) return;

    buttons = ((paddata.button[2] & 0xFF) << 8) | (paddata.button[3] & 0xFF);
    pressed = buttons & ~prev_buttons;  /* rising edge */
    prev_buttons = buttons;

    /* R1+L1 pressed: toggle pause */
    if ((pressed & PS3_BTN_R1L1) == PS3_BTN_R1L1)
    {
        if (g_debug_pause.paused)
        {
            /* Resume - unhalt CPU, clear all pause state */
            reset_pause_state();
        }
        else
        {
            /* Enter CAPTURE phase: arm recording, CPU keeps running */
            reset_pause_state();
            g_debug_pause.paused = 1;
            g_debug_pause.capturing = 1;
            /* CPU stays running - next frame will be captured */
        }
        return;
    }

    /* When frozen, R2 = advance to next polygon (CPU stays halted) */
    if (g_debug_pause.frame_frozen && (pressed & PS3_BTN_R2))
    {
        if (g_debug_pause.poly_count > 0)
        {
            g_debug_pause.poly_index++;
            if (g_debug_pause.poly_index >= g_debug_pause.poly_count)
                g_debug_pause.poly_index = 0;
        }
        /* CPU stays halted. Frame stays frozen. Only the index changes. */
    }
}

/* Called from the interpreter halt loop when CPU is frozen.
 * Polls the pad directly since VI_UpdateScreen is not running. */
void debug_pause_poll_halt(void)
{
    padData paddata;
    uint32_t buttons, pressed;

    ioPadGetData(0, &paddata);
    if (!paddata.len) return;

    buttons = ((paddata.button[2] & 0xFF) << 8) | (paddata.button[3] & 0xFF);
    pressed = buttons & ~prev_buttons;
    prev_buttons = buttons;

    /* R1+L1: resume */
    if ((pressed & PS3_BTN_R1L1) == PS3_BTN_R1L1)
    {
        reset_pause_state();
        return;
    }

    /* R2: step to next polygon, CPU stays halted */
    if (pressed & PS3_BTN_R2)
    {
        if (g_debug_pause.poly_count > 0)
        {
            g_debug_pause.poly_index++;
            if (g_debug_pause.poly_index >= g_debug_pause.poly_count)
                g_debug_pause.poly_index = 0;
        }
        /* CPU stays halted. Frame stays frozen. Only the index changes. */
    }
}
