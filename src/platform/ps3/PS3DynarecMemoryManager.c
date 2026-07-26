#include "PS3DynarecMemoryManager.h"
#include <malloc.h>
#include <string.h>
#include <ppu-asm.h>
#include <stdio.h>

/*
 * DYNAREC_CACHE_SIZE must equal RECOMP_CACHE_SIZE (Recomp-Cache.h).
 * Cannot include Recomp-Cache.h here because it references PowerPC_func
 * which is not available in this translation unit.
 * If you change this value, update RECOMP_CACHE_SIZE in Recomp-Cache.h too.
 */
#define DYNAREC_CACHE_SIZE (64 * 1024 * 1024)

static void* s_code_cache = NULL;

__attribute__((visibility("default"))) int init_dynarec_memory() {
    if (s_code_cache) return 1;

    // En PS3 homebrew, el heap suele tener permisos RWX o permite ejecución.
    // Alineamos a 4KB por seguridad.
    s_code_cache = memalign(4096, DYNAREC_CACHE_SIZE);
    if (!s_code_cache) {
        printf("[DYNAREC] ERROR: memalign failed for %d bytes\n", DYNAREC_CACHE_SIZE);
        return 0;
    }
    
    printf("[DYNAREC] Code cache allocated at %p, size=%d (0x%x)\n", 
           s_code_cache, DYNAREC_CACHE_SIZE, DYNAREC_CACHE_SIZE);

    memset(s_code_cache, 0, DYNAREC_CACHE_SIZE);
    
    // Verificar que la memoria es escribible
    volatile unsigned int *test_ptr = (volatile unsigned int *)s_code_cache;
    *test_ptr = 0x12345678;
    if (*test_ptr != 0x12345678) {
        printf("[DYNAREC] ERROR: Memory write test failed!\n");
        return 0;
    }
    *test_ptr = 0;
    
    printf("[DYNAREC] Memory write test passed\n");
    printf("[DYNAREC] To verify execution, test a small PPC snippet after flush\n");
    
    return 1;
}

__attribute__((visibility("default"))) void flush_icache(void* start, size_t size) {
    // Sincronización del caché de instrucciones para PowerPC (arquitectura de la PS3)
    // Esto es vital para que el procesador vea el nuevo código generado en RAM.
    uintptr_t addr = (uintptr_t)start;
    uintptr_t end = addr + size;
    uintptr_t i;

    // dcbst: Sincroniza el caché de datos con la memoria principal
    for (i = addr & ~127; i < end; i += 128) {
        asm volatile("dcbst 0, %0" : : "r"(i) : "memory");
    }
    asm volatile("sync");

    // icbi: Invalida el caché de instrucciones para forzar la recarga
    for (i = addr & ~127; i < end; i += 128) {
        asm volatile("icbi 0, %0" : : "r"(i) : "memory");
    }
    asm volatile("sync; isync");
}

__attribute__((visibility("default"))) void* get_code_cache_ptr() {
    return s_code_cache;
}

__attribute__((visibility("default"))) void deinit_dynarec_memory() {
    if (s_code_cache) {
        printf("[DYNAREC] Freeing code cache at %p\n", s_code_cache);
        free(s_code_cache);
        s_code_cache = NULL;
    }
}