/**
 * spu_manager.h - PPU-side SPU thread management
 *
 * Manages SPU thread lifecycle, mailbox communication,
 * and DMA transfers for the wii64-ps3 SPU offload system.
 */

#ifndef SPU_MANAGER_H
#define SPU_MANAGER_H

#include <sys/spu.h>
#include <sys/event_queue.h>
#include <ppu-types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SPU command protocol (must match spu_main.c) */
#define SPU_CMD_NONE       0x00000000
#define SPU_CMD_PING       0x00000001
#define SPU_CMD_DMA_TEST   0x00000002
#define SPU_CMD_LOAD_STATE 0x00000010
#define SPU_CMD_EXEC       0x00000011
#define SPU_CMD_STOP       0x000000FF

/* SPU event port */
#define SPU_EVENT_PORT     0

/* SPU thread state (mirrored in SPU local store via DMA) */
typedef struct {
    volatile uint32_t cmd;
    volatile uint32_t cmd_arg;
    volatile uint32_t result;
    volatile uint32_t status;
    uint32_t dma_ea_high;
    uint32_t dma_src_ea_lo;
    uint32_t dma_dst_ea_lo;
    uint32_t dma_size;
    uint32_t ping_count;
} spu_state_t;

/* SPU manager context */
typedef struct {
    sys_spu_group_t group;
    sys_spu_thread_t thread;
    sys_event_queue_t event_queue;
    sys_event_port_t event_port;
    u64 event_key;
    int initialized;
} spu_manager_t;

extern spu_manager_t g_spu_mgr;

/**
 * Initialize the SPU subsystem.
 * Call once at startup before any other SPU functions.
 * Returns 0 on success, negative on error.
 */
int spu_manager_init(void);

/**
 * Start the SPU thread group and wait for the SPU alive signal.
 * Call after spu_manager_init().
 * Returns 0 on success, negative on error.
 */
int spu_manager_start(void);

/**
 * Shutdown the SPU subsystem.
 * Call when the emulator exits or ROM is closed.
 */
void spu_manager_shutdown(void);

/**
 * Send a 32-bit command to the SPU via mailbox.
 * Format: upper 8 bits = command ID, lower 24 bits = argument.
 * Blocks if the SPU mailbox is full.
 */
int spu_send_command(uint32_t cmd, uint32_t arg);

/**
 * DMA data from PPU main memory to SPU local store.
 * ls_offset: offset in SPU local store (must be 16-byte aligned)
 * ea: effective address in PPU main memory (must be 16-byte aligned)
 * size: transfer size in bytes (must be 16-byte aligned, max 16KB)
 */
int spu_dma_to_spu(uint32_t ls_offset, void *ea, uint32_t size);

/**
 * DMA data from SPU local store to PPU main memory.
 */
int spu_dma_from_spu(void *ea, uint32_t ls_offset, uint32_t size);

/**
 * Wait for an event from the SPU.
 * timeout_us: timeout in microseconds (0 = infinite)
 * Returns 0 on success, -1 on timeout.
 * event_data0/1/2 are filled with event payload.
 */
int spu_wait_event(uint32_t *event_data0, uint32_t *event_data1, uint32_t *event_data2,
                   uint64_t timeout_us);

/**
 * Non-blocking check for an SPU event.
 * Returns 1 if event received, 0 if no event.
 */
int spu_try_event(uint32_t *event_data0, uint32_t *event_data1, uint32_t *event_data2);

#ifdef __cplusplus
}
#endif

#endif /* SPU_MANAGER_H */
