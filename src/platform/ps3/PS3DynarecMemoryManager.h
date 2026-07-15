#ifndef PS3_DYNAREC_MEMORY_MANAGER_H
#define PS3_DYNAREC_MEMORY_MANAGER_H

#include <stddef.h> // For size_t
#include <ppu-types.h> // For u32, u64, etc.

#ifdef __cplusplus
extern "C" {
#endif

// Interfaz para la gestión de memoria ejecutable específica de PS3
int init_dynarec_memory();
void flush_icache(void* start, size_t size);
void* get_code_cache_ptr();
void deinit_dynarec_memory();

#ifdef __cplusplus
}
#endif

#endif // PS3_DYNAREC_MEMORY_MANAGER_H