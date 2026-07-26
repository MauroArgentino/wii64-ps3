#include "PS3DynarecMemoryManager.h"
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

/*
 * Use a STATIC BSS array for the code cache instead of memalign().
 *
 * WHY: On RPCS3, heap memory (from memalign/malloc via PSL1GHT's sbrk) is
 * mapped by sysMMapperSearchAndMap with SYS_MEMORY_PROT_READ_WRITE only --
 * no execute permission. When the PPU tries to execute compiled PPC code
 * from heap memory, RPCS3 crashes with:
 *   "VM: Access violation executing location 0xXXXXXXXX (unmapped memory)"
 *
 * The sys_mmapper_change_address_access_right syscall (336) is a STUB in
 * RPCS3 -- it returns CELL_OK without changing anything.
 *
 * SOLUTION: The PS3 ELF loader maps segments per program headers. The data
 * segment (hdr_data) has FLAGS(PF_W | PF_X) = Write + Execute. BSS is part
 * of this segment. A static BSS array therefore gets mapped as EXECUTABLE
 * by the ELF loader, both on RPCS3 and on real PS3 hardware.
 *
 * On real PS3, heap memory is also executable (no NX enforcement), so this
 * change is safe on both platforms.
 */
__attribute__((aligned(1048576)))
static unsigned char s_code_cache[DYNAREC_CACHE_SIZE];

static int s_cache_initialized = 0;

__attribute__((visibility("default"))) int init_dynarec_memory() {
    if (s_cache_initialized) return 1;

    printf("[DYNAREC] Code cache (BSS) at %p, size=%d (0x%x)\n",
           (void*)s_code_cache, DYNAREC_CACHE_SIZE, DYNAREC_CACHE_SIZE);

    memset(s_code_cache, 0, DYNAREC_CACHE_SIZE);

    /* Verify memory is writable */
    volatile unsigned int *test_ptr = (volatile unsigned int *)s_code_cache;
    *test_ptr = 0x12345678;
    if (*test_ptr != 0x12345678) {
        printf("[DYNAREC] ERROR: Memory write test failed!\n");
        return 0;
    }
    *test_ptr = 0;

    printf("[DYNAREC] Memory write test passed\n");
    s_cache_initialized = 1;
    return 1;
}

__attribute__((visibility("default"))) void flush_icache(void* start, size_t size) {
    uintptr_t addr = (uintptr_t)start;
    uintptr_t end = addr + size;
    uintptr_t i;

    for (i = addr & ~127; i < end; i += 128) {
        asm volatile("dcbst 0, %0" : : "r"(i) : "memory");
    }
    asm volatile("sync");

    for (i = addr & ~127; i < end; i += 128) {
        asm volatile("icbi 0, %0" : : "r"(i) : "memory");
    }
    asm volatile("sync; isync");
}

__attribute__((visibility("default"))) void* get_code_cache_ptr() {
    return (void*)s_code_cache;
}

__attribute__((visibility("default"))) void deinit_dynarec_memory() {
    printf("[DYNAREC] Code cache is static BSS, no free needed\n");
    s_cache_initialized = 0;
}
