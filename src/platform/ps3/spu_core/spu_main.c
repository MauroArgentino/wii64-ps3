/**
 * SPU program for wii64-ps3
 *
 * Runs on Cell SPU (Synergistic Processing Unit).
 * Communication with PPU:
 *   PPU -> SPU: mailbox (32-bit commands)
 *   SPU -> PPU: event queue (spu_thread_send_event)
 *   Bulk data: DMA (mfc_get/mfc_put)
 *
 * Local Store budget: 256KB total
 *   - Code + state: ~8KB
 *   - Stack:        4KB (top of LS)
 *   - Work buffer: ~240KB (for texture decode DMA)
 */

#include <spu_mfcio.h>
#include <sys/spu_thread.h>
#include <sys/spu_event.h>
#include <stdint.h>

/* ---- Protocol constants (must match PPU side) ---- */
#define SPU_CMD_NONE         0x00
#define SPU_CMD_PING         0x01
#define SPU_CMD_DMA_TEST     0x02
#define SPU_CMD_TEX_DECODE   0x10  /* Texture decode: N64 fmt -> RGBA32 */
#define SPU_CMD_STOP         0xFF

#define SPU_EVENT_PORT       0

/* ---- Texture format IDs (must match spu_job.h) ---- */
#define TEX_FMT_I4          0
#define TEX_FMT_IA4         1
#define TEX_FMT_CI4         2
#define TEX_FMT_I8          3
#define TEX_FMT_IA8         4
#define TEX_FMT_CI8         5
#define TEX_FMT_RGBA16      6
#define TEX_FMT_RGBA32      7

/* Texture decode flags */
#define TEX_DECODE_FLAG_SWAP_ENDIAN  0x1  /* Byte-swap RGBA32 for RSX endianness */
#define TEX_DECODE_FLAG_BYTESWAP     TEX_DECODE_FLAG_SWAP_ENDIAN  /* alias for PPU header compatibility */

/* ---- Shared state (in local store at offset 0) ---- */
typedef struct {
    volatile uint32_t cmd;
    volatile uint32_t cmd_arg;
    volatile uint32_t result;
    volatile uint32_t status;       /* 0=idle, 1=busy, 2=done */

uint32_t dma_ea_high;           /* PPU address high 32 bits (for DMA_TEST) */
    uint32_t dma_src_ea_lo;
    uint32_t dma_dst_ea_lo;
    uint32_t dma_size;

    uint32_t ping_count;

    /* ---- Texture decode parameters (written by PPU via mailbox) ---- */
    uint32_t tex_format;
    uint32_t tex_width;
    uint32_t tex_height;
    uint32_t tex_input_ea_lo;
    uint32_t tex_input_ea_high;
    uint32_t tex_output_ea_lo;
    uint32_t tex_output_ea_high;
    uint32_t tex_output_pitch;      /* output row pitch in bytes */
    uint32_t tex_palette_ea_lo;
    uint32_t tex_palette_fmt;       /* 0=none, 1=RGBA16, 2=IA16 */
    uint32_t tex_row_stride;        /* source row stride in bytes (0 = width * bpp) */
    uint32_t tex_flags;             /* TEX_DECODE_FLAG_* */
} spu_state_t;

spu_state_t state __attribute__((aligned(128)));

/*
 * Work buffer for DMA transfers.
 * Placed after the state struct. We use the region from 0x200 to ~0x3D000
 * (~244KB).  Split into two halves for double-buffering:
 *   buf_a: 0x200  (120KB) - input
 *   buf_b: 0x1E200 (120KB) - output
 */
#define WORK_BUF_A   0x0200
#define WORK_BUF_B   0x1E200
#define STRIP_BUF_SZ 0x1D800  /* 120KB per strip buffer */

/* Stack at top of 256KB local store */
static uint8_t spu_stack[4096] __attribute__((aligned(128)))
    __attribute__((section(".stack")));

/* ---- DMA helpers ---- */
static inline void dma_wait(unsigned int tag)
{
    mfc_write_tag_mask(1u << tag);
    mfc_write_tag_update_all();
    mfc_read_tag_status();
}

static inline void dma_get(void *ls, uint64_t ea, unsigned int size, unsigned int tag)
{
    mfc_get(ls, ea, size, tag, 0, 0);
}

static inline void dma_put(const void *ls, uint64_t ea, unsigned int size, unsigned int tag)
{
    mfc_put((void*)ls, ea, size, tag, 0, 0);
}

static inline uint64_t make_ea(uint32_t high, uint32_t low)
{
    return ((uint64_t)high << 32) | (uint32_t)low;
}

/* Round up to 16-byte boundary for MFC DMA alignment */
static inline uint32_t align16(uint32_t v)
{
    return (v + 15u) & ~15u;
}

/* ---- Lookup tables for texel conversion (in local store) ---- */
static const unsigned char four2eight[16] = {
    0, 17, 34, 51, 68, 85, 102, 119,
    136, 153, 170, 187, 204, 221, 238, 255
};

static const unsigned char five2eight[32] = {
    0, 8, 16, 25, 33, 41, 49, 58,
    66, 74, 82, 90, 99, 107, 115, 123,
    132, 140, 148, 156, 165, 173, 181, 189,
    197, 206, 214, 222, 230, 239, 247, 255
};

/* ---- Texel decode functions ---- */
/* Each reads from src, writes one RGBA32 pixel (4 bytes) to dst. src_idx is the texel index. */

static void decode_i4(const unsigned char *src, unsigned int src_idx, unsigned char *dst)
{
    unsigned int byte_idx = src_idx >> 1;
    unsigned int nibble = src_idx & 1;
    unsigned char val;
    val = src[byte_idx];
    if (nibble) val = val & 0x0F;
    else       val = (val >> 4) & 0x0F;
    val = four2eight[val];
    dst[0] = val; dst[1] = val; dst[2] = val; dst[3] = 255;
}

static void decode_ia4(const unsigned char *src, unsigned int src_idx, unsigned char *dst)
{
    unsigned char val = src[src_idx];
    unsigned char i = four2eight[(val >> 4) & 0x0F];
    unsigned char a = four2eight[val & 0x0F];
    dst[0] = i; dst[1] = i; dst[2] = i; dst[3] = a;
}

static void decode_i8(const unsigned char *src, unsigned int src_idx, unsigned char *dst)
{
    unsigned char val = src[src_idx];
    dst[0] = val; dst[1] = val; dst[2] = val; dst[3] = 255;
}

static void decode_ia8(const unsigned char *src, unsigned int src_idx, unsigned char *dst)
{
    unsigned char val = src[src_idx];
    dst[0] = val; dst[1] = val; dst[2] = val; dst[3] = 255;
}

static void decode_ci4(const unsigned char *src, unsigned int src_idx,
                        const unsigned char *pal, unsigned int pal_fmt,
                        unsigned char *dst)
{
    unsigned int byte_idx = src_idx >> 1;
    unsigned int nibble = src_idx & 1;
    unsigned char idx;
    unsigned short pal_entry;
    idx = src[byte_idx];
    if (nibble) idx = idx & 0x0F;
    else       idx = (idx >> 4) & 0x0F;
    pal_entry = ((unsigned short)pal[idx * 2] << 8) | (unsigned short)pal[idx * 2 + 1];
    if (pal_fmt == 2) {
        /* RGBA16 palette */
        dst[0] = five2eight[(pal_entry >> 11) & 0x1F];
        dst[1] = five2eight[(pal_entry >> 6) & 0x1F];
        dst[2] = five2eight[(pal_entry >> 1) & 0x1F];
        dst[3] = (pal_entry & 1) ? 255 : 0;
    } else {
        /* IA16 palette */
        dst[0] = (unsigned char)(pal_entry >> 8);
        dst[1] = dst[0]; dst[2] = dst[0];
        dst[3] = (unsigned char)(pal_entry & 0xFF);
    }
}

static void decode_ci8(const unsigned char *src, unsigned int src_idx,
                        const unsigned char *pal, unsigned int pal_fmt,
                        unsigned char *dst)
{
    unsigned char idx = src[src_idx];
    unsigned short pal_entry;
    pal_entry = ((unsigned short)pal[idx * 2] << 8) | (unsigned short)pal[idx * 2 + 1];
    if (pal_fmt == 2) {
        dst[0] = five2eight[(pal_entry >> 11) & 0x1F];
        dst[1] = five2eight[(pal_entry >> 6) & 0x1F];
        dst[2] = five2eight[(pal_entry >> 1) & 0x1F];
        dst[3] = (pal_entry & 1) ? 255 : 0;
    } else {
        dst[0] = (unsigned char)(pal_entry >> 8);
        dst[1] = dst[0]; dst[2] = dst[0];
        dst[3] = (unsigned char)(pal_entry & 0xFF);
    }
}

static void decode_rgba16(const unsigned char *src, unsigned int src_idx, unsigned char *dst)
{
    unsigned int off = src_idx * 2;
    unsigned short px = ((unsigned short)src[off] << 8) | (unsigned short)src[off + 1];
    dst[0] = five2eight[(px >> 11) & 0x1F];
    dst[1] = five2eight[(px >> 6) & 0x1F];
    dst[2] = five2eight[(px >> 1) & 0x1F];
    dst[3] = (px & 1) ? 255 : 0;
}

static void decode_rgba32(const unsigned char *src, unsigned int src_idx, unsigned char *dst)
{
    unsigned int off = src_idx * 4;
    if (state.tex_flags & TEX_DECODE_FLAG_BYTESWAP) {
        /* Byte-swap for RSX endianness: ARGB (big-endian) -> BGRA (little-endian) */
        dst[0] = src[off + 3];  /* B */
        dst[1] = src[off + 2];  /* G */
        dst[2] = src[off + 1];  /* R */
        dst[3] = src[off + 0];  /* A */
    } else {
        /* N64 RGBA32 is stored as R,G,B,A bytes */
        dst[0] = src[off];
        dst[1] = src[off + 1];
        dst[2] = src[off + 2];
        dst[3] = src[off + 3];
    }
}

/* Return bytes per texel for a given format */
static unsigned int format_bpp(unsigned int fmt)
{
    switch (fmt) {
    case TEX_FMT_I4:    return 4;   /* 4 bits per texel */
    case TEX_FMT_IA4:   return 4;
    case TEX_FMT_CI4:   return 4;
    case TEX_FMT_I8:    return 8;
    case TEX_FMT_IA8:   return 8;
    case TEX_FMT_CI8:   return 8;
    case TEX_FMT_RGBA16: return 16;
    case TEX_FMT_RGBA32: return 32;
    default: return 32;
    }
}

/* ---- Texture decode: strip-based processing ---- */
static void handle_tex_decode(void)
{
    unsigned int fmt = state.tex_format;
    unsigned int w = state.tex_width;
    unsigned int h = state.tex_height;
    unsigned int bpp = format_bpp(fmt);
    unsigned int src_bpp_bytes;   /* bytes per texel in source */
    unsigned int row_bytes;       /* bytes per source row */
    unsigned int out_row_bytes;   /* bytes per output row (RGBA32 logical) */
    unsigned int out_pitch_bytes; /* bytes per output row (VRAM pitch) */
    unsigned int strip_h;         /* rows per strip */
    unsigned int y0;
    uint64_t src_ea, dst_ea, pal_ea;
    const unsigned char *pal = (const unsigned char *)0;  /* palette in LS */
    unsigned int has_palette = 0;

    if (w == 0 || h == 0) {
        state.result = 0;
        state.status = 2;
        spu_thread_send_event(SPU_EVENT_PORT, SPU_CMD_TEX_DECODE, 0);
        return;
    }

    /* Validate EAs are not null */
    if (state.tex_input_ea_lo == 0 || state.tex_output_ea_lo == 0) {
        state.result = 0;
        state.status = 2;
        spu_thread_send_event(SPU_EVENT_PORT, SPU_CMD_TEX_DECODE, 0xFFFFFFFF);
        return;
    }

    /* Bytes per texel in source format */
    src_bpp_bytes = (bpp + 7) / 8;
    if (src_bpp_bytes == 0) src_bpp_bytes = 1;

    row_bytes = (w * bpp + 7) / 8;
    out_row_bytes = w * 4;
    out_pitch_bytes = state.tex_output_pitch ? state.tex_output_pitch : out_row_bytes;

    /* Calculate strip height: fit input + output in work buffers */
    {
        unsigned int max_rows_input = STRIP_BUF_SZ / (row_bytes ? row_bytes : 1);
        unsigned int max_rows_output = STRIP_BUF_SZ / (out_row_bytes ? out_row_bytes : 1);
        strip_h = max_rows_input;
        if (max_rows_output < strip_h) strip_h = max_rows_output;
        if (strip_h > h) strip_h = h;
        if (strip_h == 0) strip_h = 1;
    }

    src_ea = make_ea(state.tex_input_ea_high, state.tex_input_ea_lo);
    dst_ea = make_ea(state.tex_output_ea_high, state.tex_output_ea_lo);

    /* Load palette into LS if needed (CI4/CI8) */
    if (fmt == TEX_FMT_CI4 || fmt == TEX_FMT_CI8) {
        unsigned int pal_entries = (fmt == TEX_FMT_CI4) ? 16 : 256;
        unsigned int pal_bytes = pal_entries * 2;  /* each palette entry is 2 bytes (RGBA16 or IA16) */
        unsigned int pal_dma_size = align16(pal_bytes);
        pal_ea = make_ea(state.tex_input_ea_high, state.tex_palette_ea_lo);
        /* Load palette into start of work buffer B */
        dma_get((void*)WORK_BUF_B, pal_ea, pal_dma_size, 2);
        dma_wait(2);
        pal = (const unsigned char *)WORK_BUF_B;
        has_palette = 1;
    }

    /* Process texture in horizontal strips */
    for (y0 = 0; y0 < h; y0 += strip_h) {
        unsigned int cur_h = h - y0;
        unsigned int src_offset;
        unsigned int out_offset;
        unsigned int x, y;
        unsigned int in_dma_size;
        unsigned int out_dma_size;

        if (cur_h > strip_h) cur_h = strip_h;

        in_dma_size = align16(row_bytes * cur_h);
        out_dma_size = align16(out_pitch_bytes * cur_h);

        /* DMA source rows into work buffer A */
        src_offset = (y0 * row_bytes);
        dma_get((void*)WORK_BUF_A, src_ea + src_offset, in_dma_size, 0);
        dma_wait(0);

        /* Decode strip - write with pitch padding to match VRAM layout */
        for (y = 0; y < cur_h; y++) {
            unsigned char *in_row = (unsigned char *)(WORK_BUF_A + y * row_bytes);
            unsigned char *out_row = (unsigned char *)(WORK_BUF_B + y * out_pitch_bytes);

            for (x = 0; x < w; x++) {
                unsigned int texel_idx = x;  /* within this row */
                unsigned char *dst_px = out_row + x * 4;

                switch (fmt) {
                case TEX_FMT_I4:
                    decode_i4(in_row, texel_idx, dst_px);
                    break;
                case TEX_FMT_IA4:
                    decode_ia4(in_row, texel_idx, dst_px);
                    break;
                case TEX_FMT_I8:
                    decode_i8(in_row, texel_idx, dst_px);
                    break;
                case TEX_FMT_IA8:
                    decode_ia8(in_row, texel_idx, dst_px);
                    break;
                case TEX_FMT_CI4:
                    decode_ci4(in_row, texel_idx, pal, state.tex_palette_fmt, dst_px);
                    break;
                case TEX_FMT_CI8:
                    decode_ci8(in_row, texel_idx, pal, state.tex_palette_fmt, dst_px);
                    break;
                case TEX_FMT_RGBA16:
                    decode_rgba16(in_row, texel_idx, dst_px);
                    break;
                case TEX_FMT_RGBA32:
                    decode_rgba32(in_row, texel_idx, dst_px);
                    break;
                default:
                    dst_px[0] = 255; dst_px[1] = 0; dst_px[2] = 255; dst_px[3] = 255;
                    break;
                }
            }
        }

        /* DMA output strip back to PPU */
        out_offset = (y0 * out_pitch_bytes);
        dma_put((const void*)WORK_BUF_B, dst_ea + out_offset, out_dma_size, 1);
        dma_wait(1);
    }

    state.result = w * h;
    state.status = 2;
    spu_thread_send_event(SPU_EVENT_PORT, SPU_CMD_TEX_DECODE, w * h);
}

/* ---- Other command handlers ---- */

static void handle_ping(uint32_t arg)
{
    state.ping_count++;
    state.result = 0xDEAD0000 | (state.ping_count & 0xFFFF);
    state.status = 2;
    spu_thread_send_event(SPU_EVENT_PORT, SPU_CMD_PING, state.result);
}

static void handle_dma_test(uint32_t arg)
{
    unsigned int size = arg & 0xFFFF;
    if (size > STRIP_BUF_SZ) size = STRIP_BUF_SZ;

    {
        uint64_t src_ea = make_ea(state.dma_ea_high, state.dma_src_ea_lo);
        uint64_t dst_ea = make_ea(state.dma_ea_high, state.dma_dst_ea_lo);
        unsigned int i;
        volatile unsigned char *buf;

        dma_get((void*)WORK_BUF_A, src_ea, align16(size), 0);
        dma_wait(0);

        buf = (volatile unsigned char*)WORK_BUF_A;
        for (i = 0; i < size; i++) {
            buf[i] ^= 0xFF;
        }

        dma_put((const void*)WORK_BUF_A, dst_ea, align16(size), 1);
        dma_wait(1);
    }

    state.result = size;
    state.status = 2;
    spu_thread_send_event(SPU_EVENT_PORT, SPU_CMD_DMA_TEST, size);
}

static void handle_stop(void)
{
    state.status = 2;
    state.result = 0;
    spu_thread_send_event(SPU_EVENT_PORT, SPU_CMD_STOP, 0);
    spu_thread_exit(0);
}

/* ---- Main SPU loop ---- */
int main(unsigned long long ea_arg, unsigned long long arg2)
{
    state.cmd = SPU_CMD_NONE;
    state.status = 0;
    state.ping_count = 0;
    state.dma_ea_high = (uint32_t)(ea_arg >> 32);

    /* Signal PPU: SPU is alive. data_3 = state LS address for WriteLocalStorage */
    spu_thread_send_event(SPU_EVENT_PORT, 0x12345678, (unsigned int)((unsigned long)&state));

    for (;;) {
        unsigned int mbox_val = spu_read_in_mbox();
        uint32_t cmd = mbox_val >> 24;
        uint32_t arg = mbox_val & 0x00FFFFFF;

        state.cmd = cmd;
        state.status = 1;

        switch (cmd) {
        case SPU_CMD_PING:
            handle_ping(arg);
            break;
        case SPU_CMD_DMA_TEST:
            handle_dma_test(arg);
            break;
case SPU_CMD_TEX_DECODE:
            /* Parameters arrive as raw 32-bit mailbox words following the command.
             * arg = number of raw words to read (11). */
            {
                unsigned int nparams = arg;
                if (nparams >= 1) state.tex_format    = spu_read_in_mbox();
                if (nparams >= 2) state.tex_width     = spu_read_in_mbox();
                if (nparams >= 3) state.tex_height    = spu_read_in_mbox();
                if (nparams >= 4) state.tex_input_ea_lo  = spu_read_in_mbox();
                if (nparams >= 5) state.tex_output_ea_lo = spu_read_in_mbox();
                if (nparams >= 6) state.tex_palette_ea_lo = spu_read_in_mbox();
                if (nparams >= 7) state.tex_palette_fmt = spu_read_in_mbox();
                if (nparams >= 8) state.tex_flags       = spu_read_in_mbox();
                if (nparams >= 9) state.tex_input_ea_high = spu_read_in_mbox();
                if (nparams >= 10) state.tex_output_pitch = spu_read_in_mbox();
                if (nparams >= 11) state.tex_output_ea_high = spu_read_in_mbox();
            }
            handle_tex_decode();
            break;
        case SPU_CMD_STOP:
            handle_stop();
            break;
        default:
            state.result = 0xBAD00000 | cmd;
            state.status = 2;
            spu_thread_send_event(SPU_EVENT_PORT, cmd, 0xFFFFFFFF);
            break;
        }
    }

    return 0;
}
