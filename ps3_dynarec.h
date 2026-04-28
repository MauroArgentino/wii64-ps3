#ifndef PS3_DYNAREC_H
#define PS3_DYNAREC_H

#include <stddef.h>
#include <stdint.h>

// Inicializa el bloque de memoria con permisos de ejecución
int init_dynarec_memory();

// Sincroniza la caché de instrucciones (obligatorio tras escribir código nuevo)
void flush_icache(void* start, size_t size);

void* get_code_cache_ptr();
void deinit_dynarec_memory();

#endif