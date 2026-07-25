/**
 * debug_pause.c - Debug Pause Mode & Polygon Inspector implementation
 */

#include "debug_pause.h"
#include <io/pad.h>
#include <string.h>

/* Global state */
debug_pause_state_t g_debug_pause;

/* CPU halt flag - when set, interpreter spins without executing */
volatile int debug_pause_cpu_halt = 0;

/* PS3 pad raw button bits (must match controller-PS3.c) */
#define PS3_BTN_R1       (1<<3)
#define PS3_BTN_L1       (1<<2)
#define PS3_BTN_R2       (1<<1)
#define PS3_BTN_R1L1     (PS3_BTN_R1 | PS3_BTN_L1)

static uint32_t prev_buttons = 0;

void debug_pause_record_poly(const debug_poly_info_t *info)
{
    if (!g_debug_pause.paused) return;
    uint32_t idx = g_debug_pause.ring_write % DEBUG_POLY_RING_SIZE;
    g_debug_pause.ring[idx] = *info;
    g_debug_pause.ring_write = idx + 1;
    g_debug_pause.poly_count = g_debug_pause.ring_write;
}

/* Reset per-frame triangle counter. Called at start of each display list. */
void debug_pause_begin_frame(void)
{
    g_debug_pause.frame_tri_index = 0;
}

/* Called from VI_UpdateScreen after the step frame has been rendered. */
void debug_pause_on_frame_done(void)
{
    g_debug_pause.frame_frozen = 1;
    debug_pause_cpu_halt = 1;
}

/* Check raw PS3 pad for debug hotkeys. Called from VI_UpdateScreen. */
void debug_pause_poll(void)
{
    padData paddata;
    ioPadGetData(0, &paddata);
    if (!paddata.len) return;

    uint32_t buttons = ((paddata.button[2] & 0xFF) << 8) | (paddata.button[3] & 0xFF);
    uint32_t pressed = buttons & ~prev_buttons;  /* rising edge */
    prev_buttons = buttons;

    /* R1+L1 pressed: toggle pause */
    if ((pressed & PS3_BTN_R1L1) == PS3_BTN_R1L1)
    {
        if (g_debug_pause.paused)
        {
            /* Resume */
            g_debug_pause.paused = 0;
            g_debug_pause.step = 0;
            g_debug_pause.poly_index = 0;
            g_debug_pause.ring_write = 0;
            g_debug_pause.poly_count = 0;
            g_debug_pause.frame_frozen = 0;
            debug_pause_cpu_halt = 0;
        }
        else
        {
            /* Enter pause */
            g_debug_pause.paused = 1;
            g_debug_pause.step = 0;
            g_debug_pause.poly_index = 0;
            g_debug_pause.ring_write = 0;
            g_debug_pause.poly_count = 0;
            g_debug_pause.frame_frozen = 0;
            debug_pause_cpu_halt = 1;
        }
        return;
    }

    /* When paused, R2 = step to next polygon */
    if (g_debug_pause.paused && (pressed & PS3_BTN_R2))
    {
        g_debug_pause.poly_index++;
        if (g_debug_pause.poly_count > 0 &&
            g_debug_pause.poly_index >= g_debug_pause.poly_count)
            g_debug_pause.poly_index = 0;
        /* Unhalt CPU for one frame so the new polygon gets drawn */
        g_debug_pause.frame_frozen = 0;
        debug_pause_cpu_halt = 0;
    }
}

int debug_pause_should_draw(void)
{
    if (!g_debug_pause.paused) return 1;
    if (g_debug_pause.step)
    {
        g_debug_pause.step = 0;
        return 1;
    }
    return 0;
}

/* Called from the interpreter halt loop when CPU is frozen.
 * Polls the pad directly since VI_UpdateScreen is not running. */
void debug_pause_poll_halt(void)
{
    padData paddata;
    ioPadGetData(0, &paddata);
    if (!paddata.len) return;

    uint32_t buttons = ((paddata.button[2] & 0xFF) << 8) | (paddata.button[3] & 0xFF);
    uint32_t pressed = buttons & ~prev_buttons;
    prev_buttons = buttons;

    /* R1+L1: resume */
    if ((pressed & PS3_BTN_R1L1) == PS3_BTN_R1L1)
    {
        g_debug_pause.paused = 0;
        g_debug_pause.step = 0;
        g_debug_pause.poly_index = 0;
        g_debug_pause.ring_write = 0;
        g_debug_pause.poly_count = 0;
        g_debug_pause.frame_frozen = 0;
        debug_pause_cpu_halt = 0;
        return;
    }

    /* R2: step to next polygon, unhalt CPU for one frame */
    if (pressed & PS3_BTN_R2)
    {
        g_debug_pause.poly_index++;
        if (g_debug_pause.poly_count > 0 &&
            g_debug_pause.poly_index >= g_debug_pause.poly_count)
            g_debug_pause.poly_index = 0;
        g_debug_pause.frame_frozen = 0;
        debug_pause_cpu_halt = 0;
    }
}
