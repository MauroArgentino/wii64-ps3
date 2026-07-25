/**
 * spu_worker_pool.h - SPU Worker Pool Manager
 * Manages 4 graphics SPUs + 1 audio SPU with job queue.
 */

#ifndef SPU_WORKER_POOL_H
#define SPU_WORKER_POOL_H

#include <stdint.h>
#include <ppu-types.h>
#include "spu_job.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SPU_NUM_GRAPHICS  4
#define SPU_NUM_AUDIO     1
#define SPU_NUM_TOTAL     (SPU_NUM_GRAPHICS + SPU_NUM_AUDIO)

/* Per-worker runtime stats for OSD display */
typedef struct {
    uint32_t worker_id;
    uint32_t jobs_completed;
    uint32_t total_cycles;
    uint32_t busy_us;         /* microseconds busy in last interval */
    uint32_t idle_us;         /* microseconds idle in last interval */
    uint8_t  is_audio;        /* 1 = audio worker, 0 = graphics */
    uint8_t  padding[3];
} spu_worker_stats_t;

/* Opaque handle for the worker pool */
typedef struct spu_worker_pool spu_worker_pool_t;

/**
 * Initialize the SPU worker pool.
 * @param config Pool configuration (num_workers 1-4, stack_size, mailbox_size)
 * @return Opaque pool handle, or NULL on failure
 */
spu_worker_pool_t *spu_worker_pool_init(const spu_pool_config_t *config);

/**
 * Submit a job to the worker pool.
 * @param pool Pool handle
 * @param job_header Pointer to job header (must include job-specific payload after header)
 * @param blocking If 1, wait for completion and return result; if 0, return immediately
 * @param out_result If blocking, filled with result; if non-blocking, NULL
 * @return 0 on success, -1 on queue full, -2 on invalid job, -3 on pool not initialized
 */
int spu_worker_pool_submit(spu_worker_pool_t *pool,
                           const spu_job_header_t *job_header,
                           int blocking,
                           spu_job_result_t *out_result);

/**
 * Check for completed non-blocking jobs.
 * @param pool Pool handle
 * @param out_result Filled with completed job result
 * @return 1 if result available, 0 if none pending, -1 on error
 */
int spu_worker_pool_try_get_result(spu_worker_pool_t *pool, spu_job_result_t *out_result);

/**
 * Wait for a specific job ID to complete.
 * @param pool Pool handle
 * @param job_id Job ID to wait for
 * @param timeout_us Timeout in microseconds (0 = infinite)
 * @param out_result Filled with result
 * @return 0 on success, -1 on timeout, -2 on error
 */
int spu_worker_pool_wait_job(spu_worker_pool_t *pool,
                             uint32_t job_id,
                             uint64_t timeout_us,
                             spu_job_result_t *out_result);

/**
 * Get pool statistics.
 */
typedef struct {
    uint32_t jobs_submitted;
    uint32_t jobs_completed;
    uint32_t jobs_failed;
    uint32_t total_cycles;
    uint32_t queue_depth;
} spu_pool_stats_t;

void spu_worker_pool_get_stats(spu_worker_pool_t *pool, spu_pool_stats_t *out_stats);

/**
 * Get per-worker stats for OSD display.
 * @param stats Array of SPU_NUM_TOTAL entries, filled with current state
 * @return Number of active workers
 */
int spu_worker_pool_get_worker_stats(spu_worker_pool_t *pool, spu_worker_stats_t *stats, int max_entries);

/**
 * Shutdown and free the worker pool.
 */
void spu_worker_pool_destroy(spu_worker_pool_t *pool);

/* Global default pool (for convenience) */
spu_worker_pool_t *spu_worker_pool_get_default(void);

/* Job ID generator */
uint32_t spu_job_next_id(void);

#ifdef __cplusplus
}
#endif

#endif /* SPU_WORKER_POOL_H */