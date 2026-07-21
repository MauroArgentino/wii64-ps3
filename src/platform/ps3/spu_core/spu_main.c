/**
 * SPU program skeleton for wii64-ps3
 *
 * This runs on the Cell SPU (Synergistic Processing Unit).
 * Communication with PPU:
 *   PPU -> SPU: mailbox (32-bit commands)
 *   SPU -> PPU: event queue (spu_thread_send_event)
 *   Bulk data: DMA (mfc_get/mfc_put)
 *
 * Local Store budget: 256KB total
 *   - Code:       ~8KB
 *   - Stack:       4KB (top of LS)
 *   - DMA buffers: 32KB (2x 16KB double-buffered)
 *   - State:       1KB
 *   - Free:       ~211KB (for future interpreter/RSP/Audio)
 */

#include <spu_mfcio.h>
#include <sys/spu_thread.h>
#include <sys/spu_event.h>
#include <stdint.h>

/* ---- Protocol constants (must match PPU side) ---- */
#define SPU_CMD_NONE       0x00000000
#define SPU_CMD_PING       0x00000001  /* PPU sends, SPU echoes back via event */
#define SPU_CMD_DMA_TEST   0x00000002  /* DMA a block from PPU, compute, DMA back */
#define SPU_CMD_LOAD_STATE 0x00000010  /* Load N64 CPU state into SPU */
#define SPU_CMD_EXEC       0x00000011  /* Execute N64 instructions */
#define SPU_CMD_STOP       0x000000FF  /* Terminate SPU thread */

#define SPU_EVENT_PORT     0           /* event queue port number */

/* ---- DMA double-buffer region ---- */
#define DMA_BUF_SIZE      16384       /* 16KB per buffer */
#define DMA_BUF_A         0x0000
#define DMA_BUF_B         0x4000      /* 16KB offset */

/* ---- Shared state (in local store) ---- */
typedef struct {
    volatile uint32_t cmd;             /* current command from PPU */
    volatile uint32_t cmd_arg;         /* argument for command */
    volatile uint32_t result;          /* result for PPU to read */
    volatile uint32_t status;          /* 0=idle, 1=busy, 2=done */

    /* DMA buffer management */
    uint32_t dma_ea_high;              /* effective address high 32 bits */
    uint32_t dma_src_ea_lo;            /* DMA source (PPU) addr low 32 */
    uint32_t dma_dst_ea_lo;            /* DMA dest (PPU) addr low 32 */
    uint32_t dma_size;                 /* DMA transfer size */

    /* Ping test result */
    uint32_t ping_count;

    /* N64 state placeholder (future) */
    /* uint32_t n64_gpr[32]; */
    /* uint32_t n64_pc; */
    /* ... */
} spu_state_t;

/* State placed at a fixed offset in LS for easy PPU access via DMA */
spu_state_t state __attribute__((aligned(128)));

/* Stack at top of 256KB local store */
static uint8_t stack[4096] __attribute__((aligned(128)))
    __attribute__((section(".stack")));

/* ---- DMA helpers ---- */
static inline void dma_wait(unsigned int tag)
{
    mfc_write_tag_mask(1 << tag);
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

/* ---- Command handlers ---- */

static void handle_ping(uint32_t arg)
{
    state.ping_count++;
    state.result = 0xDEAD0000 | (state.ping_count & 0xFFFF);
    state.status = 2;

    /* Notify PPU via event queue */
    spu_thread_send_event(SPU_EVENT_PORT, SPU_CMD_PING, state.result);
}

static void handle_dma_test(uint32_t arg)
{
    /*
     * arg = packed: high 16 bits = tag (ignored), low 16 bits = size in bytes
     * PPU has set up dma_src_ea_lo and dma_dst_ea_lo.
     * We DMA a block from PPU into our local buffer,
     * XOR each byte with 0xFF (simple compute test),
     * then DMA it back.
     */
    unsigned int size = arg & 0xFFFF;
    if (size > DMA_BUF_SIZE) size = DMA_BUF_SIZE;

    uint64_t src_ea = make_ea(state.dma_ea_high, state.dma_src_ea_lo);
    uint64_t dst_ea = make_ea(state.dma_ea_high, state.dma_dst_ea_lo);

    /* DMA source -> local store buffer A */
    dma_get((void*)DMA_BUF_A, src_ea, size, 0);
    dma_wait(0);

    /* Compute: XOR each byte */
    volatile unsigned char *buf = (volatile unsigned char*)DMA_BUF_A;
    unsigned int i;
    for (i = 0; i < size; i++) {
        buf[i] ^= 0xFF;
    }

    /* DMA local store buffer A -> PPU destination */
    dma_put((const void*)DMA_BUF_A, dst_ea, size, 1);
    dma_wait(1);

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
    /* Initialize state */
    state.cmd = SPU_CMD_NONE;
    state.status = 0;
    state.ping_count = 0;
    state.dma_ea_high = (uint32_t)(ea_arg >> 32);

    /* Signal PPU: SPU is alive */
    spu_thread_send_event(SPU_EVENT_PORT, 0, 0x12345678);

    /* Main command loop */
    for (;;) {
        /* Wait for a command from PPU (blocks on mailbox) */
        unsigned int mbox_val = spu_read_in_mbox();
        uint32_t cmd = mbox_val >> 24;        /* upper 8 bits = command */
        uint32_t arg = mbox_val & 0x00FFFFFF; /* lower 24 bits = argument */

        state.cmd = cmd;
        state.status = 1; /* busy */

        switch (cmd) {
        case SPU_CMD_PING:
            handle_ping(arg);
            break;
        case SPU_CMD_DMA_TEST:
            handle_dma_test(arg);
            break;
        case SPU_CMD_STOP:
            handle_stop();
            break; /* won't reach here */
        default:
            /* Unknown command: report error */
            state.result = 0xBAD00000 | cmd;
            state.status = 2;
            spu_thread_send_event(SPU_EVENT_PORT, cmd, 0xFFFFFFFF);
            break;
        }
    }

    return 0;
}
