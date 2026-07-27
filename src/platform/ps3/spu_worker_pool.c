/**
 * spu_worker_pool.c - SPU Worker Pool Implementation
 *
 * Wraps spu_manager to provide job-based dispatch.
 * Finds free workers, dispatches texture decode and other jobs via mailbox.
 * Tracks completion via per-worker event queues.
 */

#include "spu_worker_pool.h"
#include "spu_manager.h"
#include <sys/spu.h>
#include <sys/event_queue.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ppu-types.h>

/* Simple timing using Cell timebase register (PPC mftb instruction) */
static inline uint32_t spu_get_time_us(void)
{
    uint32_t tb;
    __asm__ __volatile__("mftb %0" : "=r" (tb));
    /* Cell TB runs at ~800 KHz (bus_clock/2000 = 1.6GHz/2000).
     * us = ticks / 800 */
    return tb / 800u;
}

/* Job ID generator */
static volatile uint32_t g_job_id_counter = 1;

/* Pending job tracking (simple fixed-size table) */
#define MAX_PENDING_JOBS 32
typedef struct {
    uint32_t job_id;
    uint32_t worker_id;
    int      in_use;
    uint32_t submit_tick;        /* gettick() when dispatched (u32 wrapping OK) */
    spu_job_header_t header;
} pending_job_t;

static pending_job_t g_pending_jobs[MAX_PENDING_JOBS];

/* Per-worker accumulated stats */
typedef struct {
    uint64_t total_busy_ticks;  /* sum of (completion - submit) for all completed jobs */
    uint64_t interval_busy_us;  /* wall time (us) worker was busy since last poll */
    uint64_t interval_idle_us;  /* wall time (us) worker was idle since last poll */
    uint32_t jobs_completed;
    uint32_t jobs_failed;
    uint32_t last_poll_tick;    /* gettick() at last get_worker_stats() call */
    uint32_t was_busy_at_poll;  /* busy flag snapshot at last poll */
    uint32_t poll_initialized;  /* 1 if last_poll_tick has been set */
} worker_timing_t;

static worker_timing_t g_worker_timing[SPU_MAX_WORKERS];

/* Timing helper: spu_get_time_us() returns microseconds via mftb.
 * No conversion macros needed since all timing values are in microseconds. */

/* Worker pool structure */
struct spu_worker_pool {
    uint32_t num_workers;
    volatile int initialized;
    spu_pool_stats_t stats;
};

static spu_worker_pool_t g_pool;

spu_worker_pool_t *spu_worker_pool_init(const spu_pool_config_t *config)
{
    (void)config;

    memset(&g_pool, 0, sizeof(g_pool));
    memset(g_pending_jobs, 0, sizeof(g_pending_jobs));
    memset(g_worker_timing, 0, sizeof(g_worker_timing));

    g_pool.num_workers = spu_get_num_workers();
    if (g_pool.num_workers == 0) {
        /* SPU manager not initialized yet - try to init it */
        int ret = spu_manager_init();
        if (ret == 0) {
            ret = spu_manager_start();
        }
        if (ret != 0) {
            printf("[SPU Pool] Failed to initialize SPU manager: %d\n", ret);
            return NULL;
        }
        g_pool.num_workers = spu_get_num_workers();
    }

    g_pool.initialized = 1;
    printf("[SPU Pool] Initialized with %d workers\n", g_pool.num_workers);
    return &g_pool;
}

static uint32_t alloc_job_id(void)
{
    uint32_t id;
    do {
        id = g_job_id_counter;
        g_job_id_counter = id + 1;
    } while (id == 0);  /* skip 0 (invalid) */
    return id;
}

static int find_free_slot(void)
{
    int i;
    for (i = 0; i < MAX_PENDING_JOBS; i++) {
        if (!g_pending_jobs[i].in_use) return i;
    }
    return -1;
}

int spu_worker_pool_submit(spu_worker_pool_t *pool, const spu_job_header_t *job_header,
                           int blocking, spu_job_result_t *out_result)
{
    int worker_id;
    int slot;
    uint32_t job_id;

    if (!pool || !pool->initialized || !job_header) return -3;

    /* Find a free SPU worker */
    worker_id = spu_find_free_worker();
    if (worker_id < 0) {
        pool->stats.jobs_failed++;
        return -1;  /* all workers busy */
    }

    /* Allocate a job slot */
    slot = find_free_slot();
    if (slot < 0) {
        pool->stats.jobs_failed++;
        return -1;
    }

    job_id = alloc_job_id();

    /* Track the pending job */
    g_pending_jobs[slot].job_id = job_id;
    g_pending_jobs[slot].worker_id = worker_id;
    g_pending_jobs[slot].in_use = 1;
    g_pending_jobs[slot].submit_tick = spu_get_time_us();
    memcpy(&g_pending_jobs[slot].header, job_header, sizeof(spu_job_header_t));

    /* Mark worker as busy */
    spu_set_worker_busy(worker_id, 1);

    /* Dispatch based on job type */
    switch (job_header->job_type) {
    case SPU_JOB_TEX_DECODE: {
        /* Cast to tex decode payload */
        const spu_job_tex_decode_t *tex = (const spu_job_tex_decode_t *)job_header;

/* Send command word: (SPU_CMD_TEX_DECODE << 24) | num_raw_params (11) */
        spu_send_command_to(worker_id, SPU_CMD_TEX_DECODE, 11);

        /* Send 11 raw 32-bit parameter words via mailbox.
         * Retry on EBUSY (mailbox full) since the SPU consumes words
         * as fast as it reads them. */
        spu_send_raw_to(worker_id, tex->format);
        spu_send_raw_to(worker_id, tex->width);
        spu_send_raw_to(worker_id, tex->height);
        spu_send_raw_to(worker_id, tex->input_ea);
        spu_send_raw_to(worker_id, tex->output_ea);
        spu_send_raw_to(worker_id, tex->palette_ea);
        spu_send_raw_to(worker_id, tex->palette_format);
        spu_send_raw_to(worker_id, job_header->flags);              /* 8th: flags */
        spu_send_raw_to(worker_id, (uint32_t)((uint64_t)tex->input_ea >> 32));     /* 9th: input EA high */
        spu_send_raw_to(worker_id, (uint32_t)((uint64_t)tex->output_ea >> 32));    /* 10th: output EA high */
        spu_send_raw_to(worker_id, tex->output_pitch);            /* 11th: output pitch */

        pool->stats.jobs_submitted++;
        printf("[SPU Pool] TEX_DECODE job %d dispatched to worker %d (%dx%d fmt=%d flags=0x%x)\n",
               job_id, worker_id, tex->width, tex->height, tex->format, job_header->flags);
        break;
    }

    default:
        printf("[SPU Pool] Unknown job type %d\n", job_header->job_type);
        spu_set_worker_busy(worker_id, 0);
        g_pending_jobs[slot].in_use = 0;
        pool->stats.jobs_failed++;
        return -2;
    }

    /* If blocking, wait for completion */
    if (blocking && out_result) {
        uint32_t evt_d2 = 0, evt_d3 = 0;
        int ret = spu_wait_worker_event(worker_id, &evt_d2, &evt_d3, 5000000);
        spu_set_worker_busy(worker_id, 0);
        g_pending_jobs[slot].in_use = 0;

        if (ret == 0) {
            uint32_t elapsed_us_val = spu_get_time_us() - g_pending_jobs[slot].submit_tick;
            g_worker_timing[worker_id].total_busy_ticks += (uint64_t)elapsed_us_val;
            g_worker_timing[worker_id].jobs_completed++;
            out_result->job_id = job_id;
            out_result->status = SPU_JOB_STATUS_DONE;
            out_result->cycles_elapsed = elapsed_us_val;
            out_result->error_code = evt_d3;
            pool->stats.jobs_completed++;
            return 0;
        } else {
            g_worker_timing[worker_id].jobs_failed++;
            out_result->job_id = job_id;
            out_result->status = SPU_JOB_STATUS_ERROR;
            out_result->error_code = 0xDEAD;
            pool->stats.jobs_failed++;
            return -1;
        }
    }

    return 0;
}

int spu_worker_pool_try_get_result(spu_worker_pool_t *pool, spu_job_result_t *out_result)
{
    uint32_t i;
    if (!pool || !pool->initialized || !out_result) return -1;

    /* Check all busy workers for completion events */
    for (i = 0; i < pool->num_workers; i++) {
        uint32_t evt_d2 = 0, evt_d3 = 0;
        int ret = spu_try_worker_event(i, &evt_d2, &evt_d3);
        if (ret == 1) {
            /* Found an event - find the matching pending job */
            int j;
            for (j = 0; j < MAX_PENDING_JOBS; j++) {
                if (g_pending_jobs[j].in_use && g_pending_jobs[j].worker_id == (int)i) {
                    uint32_t elapsed_us_val = spu_get_time_us() - g_pending_jobs[j].submit_tick;
                    g_worker_timing[i].total_busy_ticks += (uint64_t)elapsed_us_val;
                    g_worker_timing[i].jobs_completed++;
                    out_result->job_id = g_pending_jobs[j].job_id;
                    out_result->status = SPU_JOB_STATUS_DONE;
                    out_result->cycles_elapsed = elapsed_us_val;
                    out_result->error_code = evt_d3;  /* SPU returns error in event data3 */
                    spu_set_worker_busy(i, 0);
                    g_pending_jobs[j].in_use = 0;
                    pool->stats.jobs_completed++;
                    return 1;
                }
            }
        }
    }
    return 0;
}

int spu_worker_pool_wait_job(spu_worker_pool_t *pool, uint32_t job_id,
                             uint64_t timeout_us, spu_job_result_t *out_result)
{
    int j;
    uint32_t worker_id;
    uint32_t evt_d2, evt_d3;
    int ret;

    if (!pool || !pool->initialized) return -2;

    /* Find the pending job */
    for (j = 0; j < MAX_PENDING_JOBS; j++) {
        if (g_pending_jobs[j].in_use && g_pending_jobs[j].job_id == job_id) {
            break;
        }
    }
    if (j >= MAX_PENDING_JOBS) return -2;  /* job not found */

    worker_id = g_pending_jobs[j].worker_id;
    ret = spu_wait_worker_event(worker_id, &evt_d2, &evt_d3, timeout_us);
    spu_set_worker_busy(worker_id, 0);
    g_pending_jobs[j].in_use = 0;

    if (ret == 0) {
        uint32_t elapsed_us_val = spu_get_time_us() - g_pending_jobs[j].submit_tick;
        g_worker_timing[worker_id].total_busy_ticks += (uint64_t)elapsed_us_val;
        g_worker_timing[worker_id].jobs_completed++;
        if (out_result) {
            out_result->job_id = job_id;
            out_result->status = SPU_JOB_STATUS_DONE;
            out_result->cycles_elapsed = elapsed_us_val;
            out_result->error_code = 0;
        }
        pool->stats.jobs_completed++;
        return 0;
    }
    return -1;
}

void spu_worker_pool_get_stats(spu_worker_pool_t *pool, spu_pool_stats_t *out_stats) {
    if (pool && out_stats) *out_stats = pool->stats;
}

int spu_worker_pool_get_worker_stats(spu_worker_pool_t *pool, spu_worker_stats_t *stats, int max_entries) {
    int count;
    int i;
    uint32_t now_tick;

    if (!pool || !pool->initialized || !stats) return 0;

    now_tick = spu_get_time_us();
    count = (int)pool->num_workers < max_entries ? (int)pool->num_workers : max_entries;

    for (i = 0; i < count; i++) {
        worker_timing_t *wt = &g_worker_timing[i];
        int is_busy_now;

        is_busy_now = spu_get_worker_busy(i);

        if (!wt->poll_initialized) {
            wt->last_poll_tick = now_tick;
            wt->was_busy_at_poll = (uint32_t)is_busy_now;
            wt->poll_initialized = 1;
        } else {
            uint32_t elapsed_us;

            elapsed_us = now_tick - wt->last_poll_tick;

            if (wt->was_busy_at_poll) {
                wt->interval_busy_us += elapsed_us;
            } else {
                wt->interval_idle_us += elapsed_us;
            }

            wt->last_poll_tick = now_tick;
            wt->was_busy_at_poll = (uint32_t)is_busy_now;
        }

        stats[i].worker_id = (uint32_t)i;
        stats[i].jobs_completed = wt->jobs_completed;
        stats[i].busy_us = (uint32_t)(wt->interval_busy_us & 0xFFFFFFFF);
        stats[i].idle_us = (uint32_t)(wt->interval_idle_us & 0xFFFFFFFF);
        stats[i].total_cycles = (uint32_t)(wt->total_busy_ticks & 0xFFFFFFFF);
        stats[i].is_audio = 0;
    }
    return count;
}

void spu_worker_pool_destroy(spu_worker_pool_t *pool) {
    if (!pool) return;
    pool->initialized = 0;
}

spu_worker_pool_t *spu_worker_pool_get_default(void) {
    return &g_pool;
}

uint32_t spu_job_next_id(void) {
    return alloc_job_id();
}

/* Job execute stubs (for PPU fallback path) */
int spu_job_tl_vertex_execute(const spu_job_tl_vertex_t *job) { (void)job; return 0; }
int spu_job_tex_decode_execute(const spu_job_tex_decode_t *job) { (void)job; return 0; }
int spu_job_vtx_validate_execute(const spu_job_vtx_validate_t *job) { (void)job; return 0; }
int spu_job_audio_resample_execute(const void *job) { (void)job; return 0; }
