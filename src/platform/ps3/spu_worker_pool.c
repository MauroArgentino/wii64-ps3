/**
 * spu_worker_pool.c - SPU Worker Pool Implementation
 * Manages N SPU workers with job queue for parallel graphics/audio processing.
 * Uses PSL1GHT sys/spu.h APIs (managed SPU threads).
 */

#include "spu_worker_pool.h"
#include "spu_manager.h"
#include <sys/spu.h>
#include <sys/event_queue.h>
#include <sys/thread.h>
#include <sys/memory.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Per-worker state */
typedef struct {
    sys_spu_thread_t thread;
    sys_event_queue_t eq;
    sys_event_port_t port;
    uint64_t event_key;
    volatile int running;
    uint32_t worker_id;
    volatile uint32_t jobs_completed;
    volatile uint32_t total_cycles;
    volatile uint64_t busy_start_us;
    volatile uint32_t busy_us;
    volatile uint32_t idle_us;
    uint8_t is_audio;
} spu_worker_t;

/* Worker pool structure */
struct spu_worker_pool {
    spu_worker_t *workers;
    uint32_t num_workers;
    sys_spu_group_t group;
    sys_event_queue_t main_eq;
    sys_event_port_t main_port;
    volatile int initialized;
    uint32_t next_job_id;
    spu_pool_stats_t stats;
};

/* Forward declarations */
static void spu_worker_shutdown(spu_worker_t *w);

/* Global default pool */
static spu_worker_pool_t *g_default_pool = NULL;
static volatile uint32_t g_job_id_counter = 1;

/* ============================================================
 * Pool Management
 * ============================================================ */

spu_worker_pool_t *spu_worker_pool_init(const spu_pool_config_t *config) {
    spu_pool_config_t def_config = {4, 64*1024, 32};
    if (!config) config = &def_config;

    if (config->num_workers < 1 || config->num_workers > 6) {
        fprintf(stderr, "[SPU Pool] Invalid worker count: %u (max 6)\n", config->num_workers);
        return NULL;
    }

    spu_worker_pool_t *pool = (spu_worker_pool_t *)malloc(sizeof(spu_worker_pool_t));
    if (!pool) return NULL;
    memset(pool, 0, sizeof(*pool));

    pool->num_workers = config->num_workers;
    pool->workers = (spu_worker_t *)malloc(config->num_workers * sizeof(spu_worker_t));
    if (!pool->workers) {
        free(pool);
        return NULL;
    }
    memset(pool->workers, 0, config->num_workers * sizeof(spu_worker_t));

    /* Create main event queue for receiving results from all workers */
    int ret = sysEventQueueCreate(&pool->main_eq, NULL, SYS_EVENT_QUEUE_KEY_LOCAL, 64);
    if (ret != 0) {
        fprintf(stderr, "[SPU Pool] sysEventQueueCreate failed: 0x%x\n", ret);
        goto fail;
    }

    ret = sysEventPortCreate(&pool->main_port, SYS_EVENT_PORT_LOCAL, 0);
    if (ret != 0) {
        fprintf(stderr, "[SPU Pool] sysEventPortCreate failed: 0x%x\n", ret);
        goto fail;
    }

    ret = sysEventPortConnectLocal(pool->main_port, pool->main_eq);
    if (ret != 0) {
        fprintf(stderr, "[SPU Pool] sysEventPortConnectLocal failed: 0x%x\n", ret);
        goto fail;
    }

    pool->initialized = 1;
    pool->next_job_id = 1;
    memset(&pool->stats, 0, sizeof(spu_pool_stats_t));

    g_default_pool = pool;
    return pool;

fail:
    if (pool->main_port) sysEventPortDestroy(pool->main_port);
    if (pool->main_eq) sysEventQueueDestroy(pool->main_eq, 0);
    free(pool->workers);
    free(pool);
    return NULL;
}

static void spu_worker_shutdown(spu_worker_t *w) {
    w->running = 0;
    if (w->port) sysEventPortDestroy(w->port);
    if (w->eq) sysEventQueueDestroy(w->eq, 0);
}

/* ============================================================
 * Pool API
 * ============================================================ */

int spu_worker_pool_submit(spu_worker_pool_t *pool, const spu_job_header_t *job_header,
                           int blocking, spu_job_result_t *out_result) {
    if (!pool || !pool->initialized || !job_header) return -3;

    /* Assign job ID */
    uint32_t job_id = __sync_fetch_and_add(&pool->next_job_id, 1);
    if (job_id == 0) job_id = __sync_fetch_and_add(&pool->next_job_id, 1);

    /* Stub: job submission will be implemented when SPU ELFs are ready.
     * For now, immediately return a "done" result. */
    if (out_result) {
        out_result->job_id = job_id;
        out_result->status = SPU_JOB_STATUS_DONE;
        out_result->cycles_elapsed = 0;
        out_result->error_code = 0;
    }

    pool->stats.jobs_submitted++;
    pool->stats.jobs_completed++;

    return 0;
}

int spu_worker_pool_try_get_result(spu_worker_pool_t *pool, spu_job_result_t *out_result) {
    if (!pool || !pool->initialized) return -1;

    sys_event_t event;
    int ret = sysEventQueueReceive(pool->main_eq, &event, 0);
    if (ret == 0 && out_result) {
        memcpy(out_result, &event, sizeof(spu_job_result_t));
        pool->stats.jobs_completed++;
        return 1;
    }
    return 0;
}

int spu_worker_pool_wait_job(spu_worker_pool_t *pool, uint32_t job_id,
                             uint64_t timeout_us, spu_job_result_t *out_result) {
    if (!pool || !pool->initialized) return -2;

    sys_event_t event;
    int ret = sysEventQueueReceive(pool->main_eq, &event, timeout_us);
    if (ret == 0) {
        if (out_result) memcpy(out_result, &event, sizeof(spu_job_result_t));
        pool->stats.jobs_completed++;
        return 0;
    }
    return -1;
}

void spu_worker_pool_get_stats(spu_worker_pool_t *pool, spu_pool_stats_t *out_stats) {
    if (pool && out_stats) *out_stats = pool->stats;
}

int spu_worker_pool_get_worker_stats(spu_worker_pool_t *pool, spu_worker_stats_t *stats, int max_entries) {
    if (!pool || !pool->initialized || !stats) return 0;
    int count = pool->num_workers < max_entries ? pool->num_workers : max_entries;
    int i;
    for (i = 0; i < count; i++) {
        spu_worker_t *w = &pool->workers[i];
        stats[i].worker_id = w->worker_id;
        stats[i].jobs_completed = w->jobs_completed;
        stats[i].total_cycles = w->total_cycles;
        stats[i].is_audio = w->is_audio;
        stats[i].busy_us = w->busy_us;
        stats[i].idle_us = w->idle_us;
        /* Reset interval counters */
        w->busy_us = 0;
        w->idle_us = 0;
    }
    return count;
}

void spu_worker_pool_destroy(spu_worker_pool_t *pool) {
    if (!pool) return;
    pool->initialized = 0;

    {
        uint32_t i;
        for (i = 0; i < pool->num_workers; i++) {
            spu_worker_shutdown(&pool->workers[i]);
        }
    }

    if (pool->main_port) sysEventPortDestroy(pool->main_port);
    if (pool->main_eq) sysEventQueueDestroy(pool->main_eq, 0);

    free(pool->workers);
    if (g_default_pool == pool) g_default_pool = NULL;
    free(pool);
}

spu_worker_pool_t *spu_worker_pool_get_default(void) {
    return g_default_pool;
}

uint32_t spu_job_next_id(void) {
    return __sync_fetch_and_add(&g_job_id_counter, 1);
}

/* ============================================================
 * Job Implementations (Stubs - will be moved to SPU ELFs)
 * ============================================================ */

int spu_job_tl_vertex_execute(const spu_job_tl_vertex_t *job) {
    (void)job;
    return 0;
}

int spu_job_tex_decode_execute(const spu_job_tex_decode_t *job) {
    (void)job;
    return 0;
}

int spu_job_vtx_validate_execute(const spu_job_vtx_validate_t *job) {
    (void)job;
    return 0;
}

int spu_job_audio_resample_execute(const void *job) {
    (void)job;
    return 0;
}
