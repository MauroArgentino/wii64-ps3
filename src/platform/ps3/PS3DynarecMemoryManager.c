#include "PS3DynarecMemoryManager.h" // Renombrado
#include <stdio.h>
#include <sys/memory.h>

// Declaración externa para debugging
extern void dbg_printf(const char *fmt,...);

/**
 * PS3 Executable Memory Manager
 * Esta implementación utiliza sys_memory_allocate para obtener páginas
 * que permiten la ejecución de código (RWX), superando el bloqueo de malloc estándar.
 */

#define CODE_CACHE_SIZE (16 * 1024 * 1024) // Ajustado a 16MB para coincidir con RECOMP_CACHE_SIZE

static void* g_code_cache = NULL;

int init_dynarec_memory() {
    sys_addr_t addr;
    // PSDK3v2 usa sysMemoryAllocate (CamelCase)
    uint64_t flags = 0x200; 
    int res = sysMemoryAllocate(CODE_CACHE_SIZE, flags, &addr);
    
    if (res != 0) {
        dbg_printf("Dynarec: Error asignando memoria (%d) flags: %llx\n", res, flags);
        return 0;
    }

    dbg_printf("Dynarec: Memoria asignada en %llx\n", (uint64_t)addr);
    g_code_cache = (void*)(uintptr_t)addr; // Cast sys_addr_t to void* explicitly
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
    if (g_code_cache) sysMemoryFree((sys_addr_t)(uintptr_t)g_code_cache); // Usar la función correcta
}