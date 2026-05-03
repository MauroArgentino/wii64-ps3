#include <ppu-types.h> // Para long

// Stub para satisfacer el enlazador en glN64.cpp
// Esta función es interna al plugin glN64_GX y debe ser implementada
// o stubeada si no es necesaria para la PS3.
#ifdef __cplusplus
extern "C"
#endif
void OGL_ReadScreen(void** dest, long* width, long* height) {
    // Implementación vacía para evitar error de undefined reference
}