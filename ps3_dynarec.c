#include "ps3_dynarec.h"
#include <stdio.h>
#include <sys/memory.h>

/**
 * PS3 Executable Memory Manager
 * Esta implementación utiliza sys_memory_allocate para obtener páginas
 * que permiten la ejecución de código (RWX), superando el bloqueo de malloc estándar.
 */

#define CODE_CACHE_SIZE (16 * 1024 * 1024) // Ajustado a 16MB para coincidir con RECOMP_CACHE_SIZE

static void* g_code_cache = NULL;

int init_dynarec_memory() {
    sys_addr_t addr;
    // En PS3, la memoria asignada por sistema con estas flags permite ejecución.
    // La alineación debe ser de 1MB para SYS_MEMORY_PAGE_SIZE_1M.
    int res = sys_memory_allocate(CODE_CACHE_SIZE, SYS_MEMORY_PAGE_SIZE_1M, &addr);
    
    if (res != 0) {
        printf("Dynarec: Error asignando memoria ejecutable (%d)\n", res);
        return 0;
    }

    g_code_cache = (void*)addr;
    return 1;
}

void flush_icache(void* start, size_t size) {
    // En PowerPC, después de escribir instrucciones en memoria, debemos 
    // forzar que la caché de datos se escriba y la de instrucciones se invalide.
    uintptr_t p = (uintptr_t)start & ~(uintptr_t)31;
    uintptr_t end = (uintptr_t)start + size;

    for (; p < end; p += 32) {
        __asm__ __volatile__ (
            "dcbst 0, %0\n" // Data Cache Block Store
            "sync\n"
            "icbi 0, %0\n"  // Instruction Cache Block Invalidate
            : : "r"(p)
        );
    }
    __asm__ __volatile__ ("isync");
}

void* get_code_cache_ptr() {
    return g_code_cache;
}

void deinit_dynarec_memory() {
    if (g_code_cache) sys_memory_free((sys_addr_t)g_code_cache);
}