/**
 * spu_manager.h - PPU-side SPU thread management
 *
 * Manages up to 6 SPU thread lifecycle, mailbox communication,
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
#define SPU_CMD_NONE       0x00
#define SPU_CMD_PING       0x01
#define SPU_CMD_DMA_TEST   0x02
#define SPU_CMD_TEX_DECODE 0x10
#define SPU_CMD_STOP       0xFF

/* SPU event port */
#define SPU_EVENT_PORT     0

/* Maximum number of SPU workers */
#define SPU_MAX_WORKERS    6

/* Per-worker thread state */
typedef struct {
    sys_spu_thread_t    thread;
    sys_event_queue_t   eq;
    sys_event_port_t    port;
    int                 busy;       /* 1 = processing a job, 0 = idle */
    uint32_t            worker_id;
    uint32_t            state_ls_addr; /* LS address of state struct in this worker */
} spu_worker_thread_t;

/* SPU manager context */
typedef struct {
    sys_spu_group_t     group;
    spu_worker_thread_t workers[SPU_MAX_WORKERS];
    uint32_t            num_workers;
    int                 initialized;
} spu_manager_t;

extern spu_manager_t g_spu_mgr;

/**
 * Initialize the SPU subsystem.
 * Creates up to SPU_MAX_WORKERS managed SPU threads.
 * Returns 0 on success, negative on error.
 */
int spu_manager_init(void);

/**
 * Start the SPU thread group and wait for alive signals.
 * Returns 0 on success, negative on error.
 */
int spu_manager_start(void);

/**
 * Shutdown the SPU subsystem.
 */
void spu_manager_shutdown(void);

/**
 * Send a 32-bit mailbox command to a specific SPU worker.
 * worker_id: 0 to num_workers-1
 * Returns 0 on success.
 */
int spu_send_command_to(uint32_t worker_id, uint32_t cmd, uint32_t arg);

/**
 * Write an 8-byte value to a specific SPU worker's local store.
 * ls_offset: offset in the SPU's local store (16-byte aligned)
 * value: 8 bytes to write
 * Returns 0 on success.
 */
int spu_write_worker_ls(uint32_t worker_id, uint32_t ls_offset, uint64_t value);

/**
 * Read an 8-byte value from a specific SPU worker's local store.
 * Returns 0 on success.
 */
int spu_read_worker_ls(uint32_t worker_id, uint32_t ls_offset, uint64_t *value);

/**
 * Wait for an event from a specific SPU worker.
 * timeout_us: 0 = infinite
 * Returns 0 on success, -1 on timeout.
 */
int spu_wait_worker_event(uint32_t worker_id,
                          uint32_t *event_data2, uint32_t *event_data3,
                          uint64_t timeout_us);

/**
 * Non-blocking check for an event from a specific worker.
 * Returns 1 if event received, 0 if none.
 */
int spu_try_worker_event(uint32_t worker_id,
                         uint32_t *event_data2, uint32_t *event_data3);

/**
 * Find a free (idle) worker.
 * Returns worker_id (0..num_workers-1), or -1 if all busy.
 */
int spu_find_free_worker(void);

/**
 * Mark a worker as busy or idle.
 */
void spu_set_worker_busy(uint32_t worker_id, int busy);

/**
 * Query whether a worker is currently busy.
 * Returns 1 if busy, 0 if idle, -1 if invalid worker_id.
 */
int spu_get_worker_busy(uint32_t worker_id);

/**
 * Get the number of initialized workers.
 */
uint32_t spu_get_num_workers(void);

/**
 * Legacy API: send command to worker 0 (for backward compatibility).
 */
int spu_send_command(uint32_t cmd, uint32_t arg);

/**
 * Send a raw 32-bit value to a worker's mailbox (no cmd packing).
 * Used for sending multi-word parameters after an initial command.
 * Retries if mailbox is full.
 */
int spu_send_raw_to(uint32_t worker_id, uint32_t value);

#ifdef __cplusplus
}
#endif

#endif /* SPU_MANAGER_H */
