/**
 * spu_manager.c - PPU-side SPU thread management
 *
 * Initializes and manages up to 6 SPU threads that run the same SPU ELF image.
 * Each thread gets its own local store, event queue, and mailbox.
 * Uses managed SPU threads (OS-scheduled across available SPEs).
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/spu.h>
#include <lv2/spu.h>
#include <sys/event_queue.h>

#include "spu_manager.h"

/* Symbols from bin2s: the SPU ELF binary embedded in our PPU ELF */
extern const u8 spu_core_elf[];
extern const u32 spu_core_elf_size;

spu_manager_t g_spu_mgr;

static const char grp_name[] = "wii64_spu";
static const char thr_fmt[]  = "spu_%d";

#define ptr2ea(x) ((u64)((void*)(x)))

int spu_manager_init(void)
{
    s32 ret;
    uint32_t i;

    memset(&g_spu_mgr, 0, sizeof(g_spu_mgr));
    g_spu_mgr.num_workers = SPU_MAX_WORKERS;

    /* Step 1: Initialize SPU subsystem (up to 6 managed threads, 0 raw SPUs) */
    ret = sysSpuInitialize(SPU_MAX_WORKERS, 0);
    if (ret != 0) {
        printf("[SPU] sysSpuInitialize(%d) failed: %08x\n", SPU_MAX_WORKERS, ret);
        /* Fall back to fewer threads */
        ret = sysSpuInitialize(1, 0);
        if (ret != 0) {
            printf("[SPU] sysSpuInitialize(1) also failed: %08x\n", ret);
            return ret;
        }
        g_spu_mgr.num_workers = 1;
        printf("[SPU] Fallback: only 1 worker available\n");
    }
    printf("[SPU] sysSpuInitialize OK (%d workers)\n", g_spu_mgr.num_workers);

    /* Step 2: Create thread group */
    {
        sysSpuThreadGroupAttribute grpattr;
        memset(&grpattr, 0, sizeof(grpattr));
        grpattr.nameSize = sizeof(grp_name);
        grpattr.nameAddress = (u32)(unsigned long)grp_name;
        grpattr.groupType = 0;
        grpattr.memContainer = 0;

        ret = sysSpuThreadGroupCreate(&g_spu_mgr.group, g_spu_mgr.num_workers, 100, &grpattr);
        if (ret != 0) {
            printf("[SPU] sysSpuThreadGroupCreate(%d) failed: %08x\n", g_spu_mgr.num_workers, ret);
            /* Try with fewer */
            ret = sysSpuThreadGroupCreate(&g_spu_mgr.group, 1, 100, &grpattr);
            if (ret != 0) {
                printf("[SPU] Thread group create failed completely: %08x\n", ret);
                return ret;
            }
            g_spu_mgr.num_workers = 1;
            printf("[SPU] Fallback: group with 1 thread\n");
        }
        printf("[SPU] Thread group created (%d threads)\n", g_spu_mgr.num_workers);
    }

    /* Step 3: Import the SPU ELF image (once, shared by all threads) */
    {
        sysSpuImage image;
        ret = sysSpuImageImport(&image, (void*)spu_core_elf, 0);
        if (ret != 0) {
            printf("[SPU] sysSpuImageImport failed: %08x\n", ret);
            return ret;
        }
        printf("[SPU] SPU image imported OK\n");

        /* Step 4: Initialize each SPU thread */
        for (i = 0; i < g_spu_mgr.num_workers; i++) {
            spu_worker_thread_t *w = &g_spu_mgr.workers[i];
            sysSpuThreadAttribute thrattr;
            sysSpuThreadArgument thrargs;
            char name_buf[16];
            sys_event_queue_attr_t eq_attr;

            w->worker_id = i;
            w->busy = 0;

            /* Create per-worker event queue */
            memset(&eq_attr, 0, sizeof(eq_attr));
            eq_attr.type = SYS_EVENT_QUEUE_PPU;
            eq_attr.attr_protocol = SYS_EVENT_QUEUE_FIFO;

            ret = sysEventQueueCreate(&w->eq, &eq_attr, SYS_EVENT_QUEUE_KEY_LOCAL, 32);
            if (ret != 0) {
                printf("[SPU] Event queue create failed for worker %d: %08x\n", i, ret);
                return ret;
            }

            ret = sysEventPortCreate(&w->port, SYS_EVENT_PORT_LOCAL, 0);
            if (ret != 0) {
                printf("[SPU] Event port create failed for worker %d: %08x\n", i, ret);
                return ret;
            }

            ret = sysEventPortConnectLocal(w->port, w->eq);
            if (ret != 0) {
                printf("[SPU] Event port connect failed for worker %d: %08x\n", i, ret);
                return ret;
            }

            /* Initialize thread */
            memset(&thrattr, 0, sizeof(thrattr));
            /* Use a stack-allocated name (sprintf into local buffer) */
            {
                int nn;
                name_buf[0] = 's'; name_buf[1] = 'p'; name_buf[2] = 'u'; name_buf[3] = '_';
                nn = i;
                if (nn >= 10) { name_buf[4] = '0' + nn / 10; name_buf[5] = '0' + nn % 10; name_buf[6] = '\0'; }
                else          { name_buf[4] = '0' + nn; name_buf[5] = '\0'; }
            }
            thrattr.nameAddress = (u32)(unsigned long)name_buf;
            thrattr.nameSize = 8;
            thrattr.attribute = SPU_THREAD_ATTR_NONE;

            memset(&thrargs, 0, sizeof(thrargs));

            ret = sysSpuThreadInitialize(&w->thread, g_spu_mgr.group, i,
                                          &image, &thrattr, &thrargs);
            if (ret != 0) {
                printf("[SPU] Thread %d init failed: %08x\n", i, ret);
                return ret;
            }

            /* Connect event queue */
            ret = sysSpuThreadConnectEvent(w->thread, w->eq,
                                            SPU_THREAD_EVENT_USER, SPU_EVENT_PORT);
            if (ret != 0) {
                printf("[SPU] Thread %d event connect failed: %08x\n", i, ret);
                return ret;
            }

            printf("[SPU] Worker %d initialized\n", i);
        }
    }

    g_spu_mgr.initialized = 1;
    printf("[SPU] Manager initialized OK (%d workers)\n", g_spu_mgr.num_workers);
    return 0;
}

int spu_manager_start(void)
{
    s32 ret;
    uint32_t i;
    uint32_t alive_count = 0;

    if (!g_spu_mgr.initialized) return -1;

    /* Start the thread group */
    ret = sysSpuThreadGroupStart(g_spu_mgr.group);
    if (ret != 0) {
        printf("[SPU] sysSpuThreadGroupStart failed: %08x\n", ret);
        return ret;
    }
    printf("[SPU] Thread group started\n");

    /* Wait for all workers to signal alive */
    for (i = 0; i < g_spu_mgr.num_workers; i++) {
        spu_worker_thread_t *w = &g_spu_mgr.workers[i];
        sys_event_t event;

        ret = sysEventQueueReceive(w->eq, &event, 2000000);
        if (ret != 0) {
            printf("[SPU] Worker %d alive timeout\n", i);
        } else {
            alive_count++;
            w->state_ls_addr = (uint32_t)event.data_3;
            printf("[SPU] Worker %d alive! state_ls=0x%x\n", i, w->state_ls_addr);
        }
    }

    printf("[SPU] %d/%d workers alive\n", alive_count, g_spu_mgr.num_workers);
    return (alive_count > 0) ? 0 : -1;
}

int spu_send_command_to(uint32_t worker_id, uint32_t cmd, uint32_t arg)
{
    spu_worker_thread_t *w;
    uint32_t mbox_val;
    s32 ret;

    if (!g_spu_mgr.initialized) return -1;
    if (worker_id >= g_spu_mgr.num_workers) return -2;

    w = &g_spu_mgr.workers[worker_id];
    mbox_val = (cmd << 24) | (arg & 0x00FFFFFF);

    ret = sysSpuThreadWriteMb(w->thread, mbox_val);
    if (ret != 0) {
        printf("[SPU] WriteMb failed worker %d: %08x\n", worker_id, ret);
        return ret;
    }
    return 0;
}

int spu_send_command(uint32_t cmd, uint32_t arg)
{
    return spu_send_command_to(0, cmd, arg);
}

int spu_write_worker_ls(uint32_t worker_id, uint32_t ls_offset, uint64_t value)
{
    spu_worker_thread_t *w;
    s32 ret;

    if (!g_spu_mgr.initialized) return -1;
    if (worker_id >= g_spu_mgr.num_workers) return -2;

    w = &g_spu_mgr.workers[worker_id];
    ret = sysSpuThreadWriteLocalStorage(w->thread, ls_offset, value, 0);
    return ret;
}

int spu_read_worker_ls(uint32_t worker_id, uint32_t ls_offset, uint64_t *value)
{
    spu_worker_thread_t *w;
    s32 ret;

    if (!g_spu_mgr.initialized) return -1;
    if (worker_id >= g_spu_mgr.num_workers) return -2;

    w = &g_spu_mgr.workers[worker_id];
    ret = sysSpuThreadReadLocalStorage(w->thread, ls_offset, value, 0);
    return ret;
}

int spu_wait_worker_event(uint32_t worker_id,
                          uint32_t *event_data2, uint32_t *event_data3,
                          uint64_t timeout_us)
{
    spu_worker_thread_t *w;
    sys_event_t event;
    s32 ret;

    if (!g_spu_mgr.initialized) return -1;
    if (worker_id >= g_spu_mgr.num_workers) return -2;

    w = &g_spu_mgr.workers[worker_id];
    ret = sysEventQueueReceive(w->eq, &event, timeout_us);
    if (ret != 0) return -1;

    if (event_data2) *event_data2 = (uint32_t)event.data_2;
    if (event_data3) *event_data3 = (uint32_t)event.data_3;
    return 0;
}

int spu_try_worker_event(uint32_t worker_id,
                         uint32_t *event_data2, uint32_t *event_data3)
{
    return spu_wait_worker_event(worker_id, event_data2, event_data3, 0);
}

int spu_find_free_worker(void)
{
    uint32_t i;
    if (!g_spu_mgr.initialized) return -1;

    for (i = 0; i < g_spu_mgr.num_workers; i++) {
        if (!g_spu_mgr.workers[i].busy) {
            return (int)i;
        }
    }
    return -1;
}

void spu_set_worker_busy(uint32_t worker_id, int busy)
{
    if (worker_id < g_spu_mgr.num_workers) {
        g_spu_mgr.workers[worker_id].busy = busy;
    }
}

int spu_get_worker_busy(uint32_t worker_id)
{
    if (!g_spu_mgr.initialized) return -1;
    if (worker_id >= g_spu_mgr.num_workers) return -1;
    return g_spu_mgr.workers[worker_id].busy;
}

uint32_t spu_get_num_workers(void)
{
    return g_spu_mgr.num_workers;
}

int spu_send_raw_to(uint32_t worker_id, uint32_t value)
{
    spu_worker_thread_t *w;
    s32 ret;
    int retries;

    if (!g_spu_mgr.initialized) return -1;
    if (worker_id >= g_spu_mgr.num_workers) return -2;

    w = &g_spu_mgr.workers[worker_id];
    retries = 100000;
    do {
        ret = sysSpuThreadWriteMb(w->thread, value);
        if (ret != 0) {
            /* Mailbox full - spin a bit and retry */
            volatile int d;
            for (d = 0; d < 200; d++) {}
        }
    } while (ret != 0 && retries-- > 0);

    if (ret != 0) {
        printf("[SPU] Raw send failed worker %d: %08x\n", worker_id, ret);
    }
    return ret;
}

void spu_manager_shutdown(void)
{
    uint32_t i;

    if (!g_spu_mgr.initialized) return;

    printf("[SPU] Shutting down...\n");

    /* Send stop to all workers */
    for (i = 0; i < g_spu_mgr.num_workers; i++) {
        spu_send_command_to(i, SPU_CMD_STOP, 0);
    }

    /* Wait for group to finish */
    {
        u32 cause, status;
        sysSpuThreadGroupJoin(g_spu_mgr.group, &cause, &status);
        printf("[SPU] Thread group joined (cause=%d status=%d)\n", cause, status);
    }

    /* Cleanup */
    for (i = 0; i < g_spu_mgr.num_workers; i++) {
        spu_worker_thread_t *w = &g_spu_mgr.workers[i];
        sysSpuThreadDisconnectEvent(w->thread, SPU_THREAD_EVENT_USER, SPU_EVENT_PORT);
        sysEventPortDisconnect(w->port);
        sysEventPortDestroy(w->port);
        sysEventQueueDestroy(w->eq, 0);
    }

    g_spu_mgr.initialized = 0;
    printf("[SPU] Shutdown complete\n");
}
