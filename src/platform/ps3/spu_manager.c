/**
 * spu_manager.c - PPU-side SPU thread management
 *
 * Initializes and manages the SPU thread that runs the N64 emulation core.
 * Uses managed SPU threads (OS-scheduled across available SPEs).
 *
 * SPU ELF is embedded at build time via bin2s.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/spu.h>
#include <lv2/spu.h>
#include <sys/event_queue.h>

#include "spu_manager.h"
#include "../../debug.h"

/* Symbols from bin2s: the SPU ELF binary embedded in our PPU ELF */
extern const u8 spu_core_elf[];
extern const u32 spu_core_elf_size;

spu_manager_t g_spu_mgr;

/* Attribute name strings (must stay alive - stored by EA) */
static const char grp_name[] = "wii64_spu";
static const char thr_name[] = "spu_core";

#define ptr2ea(x) ((u64)((void*)(x)))

int spu_manager_init(void)
{
    s32 ret;

    memset(&g_spu_mgr, 0, sizeof(g_spu_mgr));

    /* Step 1: Initialize SPU subsystem (1 managed thread, 0 raw SPUs) */
    ret = sysSpuInitialize(1, 0);
    if (ret != 0) {
        printf("[SPU] sysSpuInitialize failed: %08x\n", ret);
        return ret;
    }
    DBG_SPU("[SPU] sysSpuInitialize OK\n");

    /* Step 2: Create event queue for SPU->PPU events */
    sys_event_queue_attr_t eq_attr;
    memset(&eq_attr, 0, sizeof(eq_attr));
    eq_attr.type = SYS_EVENT_QUEUE_PPU;
    eq_attr.attr_protocol = SYS_EVENT_QUEUE_FIFO;

    ret = sysEventQueueCreate(&g_spu_mgr.event_queue, &eq_attr,
                              0, 32);
    if (ret != 0) {
        printf("[SPU] sysEventQueueCreate failed: %08x\n", ret);
        return ret;
    }
    DBG_SPU("[SPU] Event queue created\n");

    /* Create event port to receive SPU events */
    ret = sysEventPortCreate(&g_spu_mgr.event_port, SYS_EVENT_PORT_LOCAL, 0);
    if (ret != 0) {
        printf("[SPU] sysEventPortCreate failed: %08x\n", ret);
        return ret;
    }

    ret = sysEventPortConnectLocal(g_spu_mgr.event_port, g_spu_mgr.event_queue);
    if (ret != 0) {
        printf("[SPU] sysEventPortConnectLocal failed: %08x\n", ret);
        return ret;
    }
    DBG_SPU("[SPU] Event port connected\n");

    /* Step 3: Import the embedded SPU ELF image */
    sysSpuImage image;
    ret = sysSpuImageImport(&image, (void*)spu_core_elf, 0);
    if (ret != 0) {
        printf("[SPU] sysSpuImageImport failed: %08x (elf=%08lx, size=%08x)\n",
               ret, (unsigned long)spu_core_elf, spu_core_elf_size);
        return ret;
    }
    DBG_SPU("[SPU] SPU image imported OK (size=%08x)\n", spu_core_elf_size);

    /* Step 4: Create thread group */
    sysSpuThreadGroupAttribute grpattr;
    memset(&grpattr, 0, sizeof(grpattr));
    grpattr.nameSize = sizeof(grp_name);
    grpattr.nameAddress = (u32)(unsigned long)grp_name;
    grpattr.groupType = 0;
    grpattr.memContainer = 0;

    ret = sysSpuThreadGroupCreate(&g_spu_mgr.group, 1, 100, &grpattr);
    if (ret != 0) {
        printf("[SPU] sysSpuThreadGroupCreate failed: %08x\n", ret);
        return ret;
    }
    DBG_SPU("[SPU] Thread group created\n");

    /* Step 5: Initialize the SPU thread */
    sysSpuThreadAttribute thrattr;
    memset(&thrattr, 0, sizeof(thrattr));
    thrattr.nameAddress = (u32)(unsigned long)thr_name;
    thrattr.nameSize = sizeof(thr_name);
    thrattr.attribute = SPU_THREAD_ATTR_NONE;

    sysSpuThreadArgument thrargs;
    memset(&thrargs, 0, sizeof(thrargs));

    ret = sysSpuThreadInitialize(&g_spu_mgr.thread, g_spu_mgr.group, 0,
                                  &image, &thrattr, &thrargs);
    if (ret != 0) {
        printf("[SPU] sysSpuThreadInitialize failed: %08x\n", ret);
        return ret;
    }
    DBG_SPU("[SPU] Thread initialized\n");

    /* Step 6: Connect event queue to the SPU thread */
    ret = sysSpuThreadConnectEvent(g_spu_mgr.thread, g_spu_mgr.event_queue,
                                    SPU_THREAD_EVENT_USER, SPU_EVENT_PORT);
    if (ret != 0) {
        printf("[SPU] sysSpuThreadConnectEvent failed: %08x\n", ret);
        return ret;
    }
    DBG_SPU("[SPU] Event queue connected to thread\n");

    g_spu_mgr.initialized = 1;
    DBG_SPU("[SPU] Manager initialized OK\n");

    return 0;
}

int spu_manager_start(void)
{
    s32 ret;

    if (!g_spu_mgr.initialized) return -1;

    /* Start the SPU thread group */
    ret = sysSpuThreadGroupStart(g_spu_mgr.group);
    if (ret != 0) {
        printf("[SPU] sysSpuThreadGroupStart failed: %08x\n", ret);
        return ret;
    }
    DBG_SPU("[SPU] Thread group started\n");

    /* Wait for SPU to signal it's alive (the 0x12345678 event) */
    sys_event_t event;
    ret = sysEventQueueReceive(g_spu_mgr.event_queue, &event, 2000000);
    if (ret != 0) {
        printf("[SPU] Timed out waiting for SPU alive signal\n");
        return ret;
    }
    DBG_SPU("[SPU] SPU alive! event data: %016lx %016lx\n",
           (unsigned long)event.data_2, (unsigned long)event.data_3);

    return 0;
}

int spu_send_command(uint32_t cmd, uint32_t arg)
{
    if (!g_spu_mgr.initialized) return -1;

    uint32_t mbox_val = (cmd << 24) | (arg & 0x00FFFFFF);
    s32 ret = sysSpuThreadWriteMb(g_spu_mgr.thread, mbox_val);
    if (ret != 0) {
        printf("[SPU] sysSpuThreadWriteMb failed: %08x (cmd=%08x arg=%08x)\n", ret, cmd, arg);
        return ret;
    }
    return 0;
}

int spu_dma_to_spu(uint32_t ls_offset, void *ea, uint32_t size)
{
    if (!g_spu_mgr.initialized) return -1;

    /* For managed SPU threads, use sysSpuThreadWriteLocalStorage for small transfers.
     * This is a per-word syscall so only suitable for state sync, not bulk DMA.
     * Bulk DMA requires raw SPU or a different approach. */
    (void)ls_offset;
    (void)ea;
    (void)size;
    DBG_SPU("[SPU] spu_dma_to_spu: not implemented for managed threads\n");
    return -1;
}

int spu_dma_from_spu(void *ea, uint32_t ls_offset, uint32_t size)
{
    if (!g_spu_mgr.initialized) return -1;

    (void)ls_offset;
    (void)ea;
    (void)size;
    DBG_SPU("[SPU] spu_dma_from_spu: not implemented for managed threads\n");
    return -1;
}

int spu_wait_event(uint32_t *event_data0, uint32_t *event_data1, uint32_t *event_data2,
                   uint64_t timeout_us)
{
    sys_event_t event;
    s32 ret;

    ret = sysEventQueueReceive(g_spu_mgr.event_queue, &event, timeout_us);
    if (ret != 0) return -1;

    if (event_data0) *event_data0 = (uint32_t)(event.data_2 & 0x00FFFFFF);
    if (event_data1) *event_data1 = (uint32_t)event.data_3;
    if (event_data2) *event_data2 = (uint32_t)(event.data_2 >> 32);

    return 0;
}

int spu_try_event(uint32_t *event_data0, uint32_t *event_data1, uint32_t *event_data2)
{
    sys_event_t event;
    s32 ret;

    /* Use 0 timeout for non-blocking check */
    ret = sysEventQueueReceive(g_spu_mgr.event_queue, &event, 0);
    if (ret != 0) return 0;

    if (event_data0) *event_data0 = (uint32_t)(event.data_2 & 0x00FFFFFF);
    if (event_data1) *event_data1 = (uint32_t)event.data_3;
    if (event_data2) *event_data2 = (uint32_t)(event.data_2 >> 32);

    return 1;
}

void spu_manager_shutdown(void)
{
    if (!g_spu_mgr.initialized) return;

    DBG_SPU("[SPU] Shutting down...\n");

    /* Send stop command */
    spu_send_command(SPU_CMD_STOP, 0);

    /* Wait for thread group to finish */
    u32 cause, status;
    sysSpuThreadGroupJoin(g_spu_mgr.group, &cause, &status);
    DBG_SPU("[SPU] Thread group joined (cause=%d status=%d)\n", cause, status);

    /* Cleanup */
    sysSpuThreadDisconnectEvent(g_spu_mgr.thread, SPU_THREAD_EVENT_USER, SPU_EVENT_PORT);
    sysEventPortDisconnect(g_spu_mgr.event_port);
    sysEventPortDestroy(g_spu_mgr.event_port);
    sysEventQueueDestroy(g_spu_mgr.event_queue, 0);

    g_spu_mgr.initialized = 0;
    DBG_SPU("[SPU] Shutdown complete\n");
}
