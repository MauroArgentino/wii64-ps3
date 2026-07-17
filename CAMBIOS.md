# wii64-ps3 — Registro de Cambios

## Resumen

Port de Wii64/mupen64 a PlayStation 3 usando PSL1GHT SDK.
Emulador N64 en estado de prueba: solo interpretador puro, sin audio completo,
gráficos RSX incompletos.

---

## Fix 1 — Audio HLE revertido (rspmain.c:44)

**Archivo:** `src/core/rsp/rspmain.c`
**Cambio:** `AudioHle = TRUE` → `AudioHle = FALSE`

El valor `AudioHle = TRUE` llamaba a `processAList()` que es un stub vacío,
produciendo silencio total. Restaurar `FALSE` activa `audio_ucode()` que
procesa realmente la lista de audio del N64.

---

## Fix 2 — Limiter de frames (main.cpp:254)

**Archivo:** `src/main/main.cpp`
**Cambio:** `Timers.limitVIs = 0` → `Timers.limitVIs = 1`

Activa el limitador de frecuencia vertical (VI/s) para evitar que el emulador
ejecute a velocidad sin control cuando la máquina es más rápida que el N64 real.

---

## Fix 3 — Flip consistente en PS3 (VI.cpp)

**Archivo:** `src/video/glN64/VI.cpp`
**Cambio:** En el path `#ifdef PS3` de `VI_UpdateScreen()`, se asegura que siempre
se llamen `VI_RSX_showFPS()`, `VI_RSX_showDEBUG()` y `flip()`, incluyendo la
rama del frame límite. Antes, cuando el frame límite estaba activo, el flip se
saltaba y la pantalla no se actualizaba.

---

## Fix 4 — Intercambio R↔B en texturas (Textures.cpp)

**Archivo:** `src/video/glN64/Textures.cpp`
**Cambio:** En los bloques `#ifdef PS3` de carga de texturas (tanto
`TextureCache_LoadBackground` como `TextureCache_Load`), se agrega intercambio
de bytes a cada texel convertido:

```c
u32 v = GetTexel(src, tx, 0, texInfo->palette);
v = (v >> 8) | (v << 24);
```

Las funciones de conversión de texel (`GetCI4RGBA_RGBA8888`, etc.) producen
`0xRRGGBBAA` en big-endian, pero el RSX con `GCM_TEXTURE_FORMAT_A8R8G8B8`
espera `0xAARRGGBB`. La rotación de 8 bits corrige el orden de bytes.

---

## Fix 5 — Deshabilitar blend en RSX (OpenGL.cpp:2426)

**Archivo:** `src/video/glN64/OpenGL.cpp`
**Cambio:** `rsxSetBlendEnable(context, GCM_TRUE)` → `GCM_FALSE`

El blend estaba habilitado globalmente, causando que todo se renderizara con
transparencia. Los menús y la interfaz de juego aparecían semi-transparentes.

---

## Fix 6 — ClearColor byte order (OpenGL.cpp:2211)

**Archivo:** `src/video/glN64/OpenGL.cpp`
**Cambio:** El color de clear se cambió de un formato que producía artefactos
(`0xRRGGBBAA`) a `0x00RRGGBB` (negro con alpha correcto).

---

## Fix 7 — Restaurar estado de blend después del OSD (VI.cpp)

**Archivo:** `src/video/glN64/VI.cpp`
**Cambio:** Después de dibujar el OSD (FPS, debug), se deshabilita blend de
nuevo:

```c
rsxSetBlendEnable(context, GCM_FALSE);
```

`IPLFont::drawInit()` activa blend y no lo restaura, causando que el juego
completo se renderice con transparencia después de cada frame.

---

## Fix 8 — One-time audioPortStart (audio.c)

**Archivo:** `src/core/n64_audio/audio.c`
**Cambio:** En `play_buffer()`, se agrega flag estático `port_started`:

```c
static int port_started = 0;
if(!port_started){
    audioPortStart(portNum);
    port_started = 1;
}
```

Antes, `audioPortStart()` se llamaba en cada buffer, lo cual era redundante
y potencialmente problemático.

---

## Fix 9 — fillBuffer reset de posición (audio.c)

**Archivo:** `src/core/n64_audio/audio.c`
**Cambio:** En `fillBuffer()`, se agrega variable estática `last_buffer` para
resetear `pos = 0` cuando cambia el índice del buffer:

```c
static int last_buffer = -1;
if(last_buffer != thread_buffer){
    pos = 0;
    last_buffer = thread_buffer;
}
```

Sin esto, la posición acumulada entre cambios de buffer causaba corrupción
de audio.

---

## Fix 10 — Deshabilitar ps3audio_backend (main.cpp)

**Archivo:** `src/main/main.cpp`
**Cambio:** Se comenta la inicialización de `ps3_audio_backend`:

```c
// ps3_audio_backend
// ps3_audio_init(...)
```

Este backend abría el puerto de audio 0, entrando en conflicto con el
puerto 1 que abre `audio.c` en `InitiateAudio()`. Solo uno puede estar activo.

---

## Fix 11 — Textura dummy negra (OpenGL.cpp:2376)

**Archivo:** `src/video/glN64/OpenGL.cpp`
**Cambio:** La textura dummy (textura por defecto cuando no hay textura cargada)
se cambió de blanco (`0xFFFF`) a negro/transparente (`0x00000000`).

Con blanco, aparecían rectángulos blancos en el escenario donde el emulador
usaba la textura por defecto.

---

## Fix 12 — Limpiar back buffer en flip (rsxutil.cpp)

**Archivo:** `src/main/rsxutil.cpp`
**Cambio:** En la función `flip()`, después del swap de buffers, se limpia
el nuevo back buffer:

```c
rsxSetClearColor(context, 0x00000000);
rsxClearSurface(context, GCM_CLEAR_R |
                          GCM_CLEAR_G |
                          GCM_CLEAR_B |
                          GCM_CLEAR_A |
                          GCM_CLEAR_DEPTH |
                          GCM_CLEAR_STENCIL);
```

Sin esto, al hacer flip se mostraba memoria sin inicializar (imagen corrupta
o artefactos visuales).

---

## Fix 13 — Alpha test con registros NV40 (rsxutil.cpp + rsxutil.h)

**Archivo:** `src/main/rsxutil.cpp`, `src/main/rsxutil.h`
**Cambio:** El SDK de PSL1GHT no expone funciones para alpha test del RSX
(`rsxSetAlphaTestEnable`, `rsxSetAlphaTestFunc`, `rsxSetAlphaTestRef` no
existen en las cabeceras públicas). Se implementan usando registros raw del
NV40:

```c
#define NV40TCL_ALPHA_TEST_ENABLE  0x304
#define NV40TCL_ALPHA_TEST_FUNC    0x308
#define NV40TCL_ALPHA_TEST_REF     0x30C

void rsxSetAlphaTestEnable(gcmContextData *context, u32 enable);
void rsxSetAlphaTestFunc(gcmContextData *context, u32 func);
void rsxSetAlphaTestRef(gcmContextData *context, u32 ref);
```

Los valores de función: `0x203=LEQUAL`, `0x206=GEQUAL`, `0x207=ALWAYS`, etc.

---

## Fix 14 — Corrección de macro RSX_METHOD_ALPHA (rsxutil.cpp:203)

**Archivo:** `src/main/rsxutil.cpp`
**Cambio:** La macro para generar comandos RSX tenía el count y method
invertidos:

```c
// ANTES (incorrecto):
#define RSX_METHOD_ALPHA(method, n) (((n)<<18)|(method))

// DESPUÉS (correcto):
#define RSX_METHOD_ALPHA(method, count) (((u32)(count) << 18) | (method))
```

El formato del RSX command buffer es `((count << 18) | method)`. Con los
bits invertidos se generaba `cmd = 0xc100001` que es method=1, count=49152
→ basura, causando FIFO desync.

---

## Fix 15 — Lógica de alpha test en OpenGL.cpp

**Archivo:** `src/video/glN64/OpenGL.cpp`
**Cambio:** En el primer bloque `#ifdef PS3` de `glEnable(GL_ALPHA_TEST)`
(~línea 849), se reemplaza el stub vacío con lógica real usando los
registros NV40 del Fix 13:

```c
#ifdef PS3
    if (gDP.otherMode.alphaCompareMode == G_AC_THRESHOLD)
    {
        if (blendColor.a > 0.0f)
        {
            rsxSetAlphaTestEnable(context, GCM_TRUE);
            rsxSetAlphaTestFunc(context, 0x206); // GEQUAL
            rsxSetAlphaTestRef(context, (u32)(blendColor.a * 255.0f));
        }
        else
            rsxSetAlphaTestEnable(context, GCM_FALSE);
    }
    else if (gDP.otherMode.alphaCompareMode == G_AC_DITHER)
    {
        rsxSetAlphaTestEnable(context, GCM_TRUE);
        rsxSetAlphaTestFunc(context, 0x204); // GREATER
        rsxSetAlphaTestRef(context, 0);
    }
    else
        rsxSetAlphaTestEnable(context, GCM_FALSE);
#endif
```

---

## Fix 16 — Dynarec cache 8MB → 16MB (Recomp-Cache.h)

**Archivo:** `src/core/r4300/ppc/Recomp-Cache.h`
**Cambio:** `RECOMP_CACHE_SIZE` de `0x800000` (8MB) → `0x1000000` (16MB)

El caché de dynarec de 8MB era demasiado pequeño para juegos complejos.
Con 8MB se quedaba sin espacio y el emulador crasheaba.

---

## Fix 17 — Reducción de BSS (múltiples archivos)

**Archivos:**
- `src/core/n64_memory/memory.c` — `rdram` (64MB) y `tlb_LUT_r/w` (32MB cada uno)
  se mueven de arrays estáticos a `malloc()` en `init_memory()` / `free()` en
  `free_memory()`.
- `src/core/n64_memory/memory.h` — `extern u32 *rdram` (era array, ahora puntero)
- `src/core/n64_memory/tlb.c` — `tlb_LUT_r/w` se mueven a malloc
- `src/core/n64_memory/tlb.h` — `extern u32 *tlb_LUT_r`, `*tlb_LUT_w`
- `src/core/r4300/r4300.c` — Se remueve inicialización BSS de `blocks`/`invalid_code`
- `src/core/r4300/ppc/Recompiler.h` — `extern PowerPC_block **blocks`
- `src/core/r4300/Invalid_Code.c` — Se agrega `invalid_code_alloc()` y `invalid_code_free()`
- `src/core/r4300/Invalid_Code.h` — Declaraciones de alloc/free

**Razón:** El ejecutable ELF del PS3 tiene límites estrictos de BSS. Los arrays
grandes causaban `STATUS_ACCESS_VIOLATION` al iniciar el emulador.

---

## Fix 18 — Correcciones del dynarec (múltiples archivos)

**Archivos:**
- `src/core/r4300/ppc/Recomp-Cache.h` — Agregado `#include "PowerPC.h"` para
  resolver `extern PowerPC_block`
- `src/core/r4300/ppc/PPC_Recompiler.c` — Se eliminan funciones stub vacías que
  sombreaban las reales (`recompile_init`, `recompiler_free`, etc.)
- `src/core/r4300/ppc/PPC_Recompiler.c` — Se elimina `current_block_table` duplicado
- `src/core/r4300/ppc/PPC_Recompiler.h` — Declaraciones actualizadas para
  `recompile_init`, `recompiler_free`, `recompile_block`, `recompile_clean`
- `src/core/r4300/Recomp-Cache.c` — Se eliminan funciones stub vacías que
  sombreaban las reales
- `src/core/r4300/ppc/PS3DynarecMemoryManager.c` — Corrección de error C99:
  `(u32)(s32)1` (cast explícito para inicializador designado)
- `src/core/r4300/ppc/Recompiler.cpp` — Se eliminan funciones stub vacías
- `src/core/r4300/ppc/Recompiler.h` — Declaraciones unificadas

---

## Fix 19 — Texture bit depth forzado a 32-bit en PS3 (Textures.cpp)

**Archivo:** `src/video/glN64/Textures.cpp`
**Cambio:** En ambos `TextureCache_LoadBackground` (~línea 770) y
`TextureCache_Load` (~línea 1148), se agrega bloque `#ifdef PS3` que fuerza
formato 32-bit RGBA8888 para TODAS las texturas:

```c
#ifdef PS3
    texInfo->textureBytes = (texInfo->realWidth * texInfo->realHeight) << 2;
    if ((texInfo->format == G_IM_FMT_CI) && (gDP.otherMode.textureLUT == G_TT_IA16))
    {
        if (texInfo->size == G_IM_SIZ_4b) GetTexel = GetCI4IA_RGBA8888;
        else                              GetTexel = GetCI8IA_RGBA8888;
    }
    else
        GetTexel = imageFormat[texInfo->size][texInfo->format].Get32;
    glInternalFormat = GL_RGBA8;
    glType = GL_UNSIGNED_BYTE;
#endif
```

**Razón:** Sin esto, texturas CI4/CI8 con paleta RGBA16 usaban formato 16-bit
(`GL_RGB5_A1`), pero el bloque de upload al RSX siempre interpretaba los datos
como u32, causando corrupción.

**Estado:** El usuario reportó que NO hubo cambio visible en las texturas del
HUD de SM64. El problema puede estar en otra parte (endianness de paleta,
framebuffer rendering, estado RSX).

---

## Fix 20 — Forzar resampleo a 48kHz (audio.c:98-105)

**Archivo:** `src/core/n64_audio/audio.c`
**Cambio:** En `AiDacrateChanged()`, se fuerza `real_freq = 48000` siempre:

```c
real_freq = 48000;
freq_ratio = (float)freq / (float)real_freq;
buffer_size = (SystemType != SYSTEM_PAL) ?
               BUFFER_SIZE_48_60 : BUFFER_SIZE_48_50;
```

**Razón:** El puerto de audio del PS3 siempre reproduce a 48kHz. Sin esto,
la frecuencia nativa del N64 (32kHz para la mayoría de juegos) causaba que
el ratio de resampleo fuera >1.0, y el código de buffers no estaba preparado
para eso, produciendo audio tipo "chipmunk".

**Estado:** El efecto chipmunk se eliminó, pero la calidad de audio sigue
siendo mala (resampleo por interpolación lineal, gestión de buffers diseñada
para Wii).

---

## Fix 21 — Completar restauración de go() para interpretador puro (r4300.c)

**Archivo:** `src/core/r4300/r4300.c`
**Cambio:** Se restauró la función `go()` a su versión original con solo
interpretador puro (`dynacore==2`).

Se intentó previamente agregar un fallback a dynarec cuando `dynacore==2`
para usar interpretador puro, pero esto fue revertido porque la función
original ya usa el modo correcto.

**Razón:** El dynarec de PPC del N64 genera código que crashea inmediatamente
en RPCS3 (PPU thread abort en VM, pc=0xa4000040). RPCS3 emula PPC con PPC,
y el dynarec genera instrucciones nativas que RPCS3 no puede ejecutar
correctamente. El interpretador puro es la ÚNICA opción de CPU funcional en
RPCS3.

---

## Fix E — gDPFillRectangle usa fillColor en todos los modos (gDP.cpp:872)

**Archivo:** `src/video/glN64/gDP.cpp`
**Cambio:** Se reemplazó la línea:

```c
OGL_DrawRect( ulx, uly, lrx, lry, (gDP.otherMode.cycleType == G_CYC_FILL) ? &gDP.fillColor.r : &gDP.blendColor.r );
```

por:

```c
OGL_DrawRect( ulx, uly, lrx, lry, &gDP.fillColor.r );
```

**Razón:** En modos 1CYCLE/2CYCLE/COPY, `gDPFillRectangle` usaba `gDP.blendColor`
como color del rectángulo. Pero `blendColor` es el color de blending, no el de
relleno. Cuando `blendColor` estaba en blanco (por uso previo en otros draw
calls) y el blend por defecto era `ONE/ZERO` (opaco), los cuadros de diálogo
(por ejemplo, los de SM64) se renderizaban como rectángulos blancos sólidos en
lugar de negros semitransparentes. Usar `fillColor` siempre es correcto según
la especificación N64.

**Estado:** Verificar que los cuadros de diálogo de SM64 ahora se renderizan
como negros semitransparentes con texto blanco.

---

## Fix F — SetConstant: case SHADE agregado (Combiner.h)

**Archivo:** `src/video/glN64/Combiner.h`
**Cambio:** Se agregó `case SHADE:` a ambos switches (color y alpha) del macro
`SetConstant`.

El macro `SetConstant` resuelve la fuente de color del combiner a un valor RGB
real. Faltaba el caso `SHADE` (valor 4), que es una de las fuentes más comunes
en combiners del N64 (ej. `G_CC_MODULATEI`). Sin este caso, cuando el combiner
decía SHADE, el switch caía sin asignar nada y el vertex color retenía el valor
inicializado por defecto: `{1.0f, 1.0f, 1.0f, 0.0f}` — blanco con **alpha 0**.

En `SHADER_MODULATE` (texture * vertex), esto causaba:
- RGB: `texture.rgb * 1.0 = texture.rgb` (correcto)
- Alpha: `texture.a * 0.0 = 0.0` (¡transparente!)

**Solución:**
- Color SHADE → `(1.0, 1.0, 1.0)` (blanco, sin modificar textura)
- Alpha SHADE → `1.0` (passthrough de alpha de textura)

---

## Fix G — SetConstant: default alpha = 1.0 (Combiner.h)

**Archivo:** `src/video/glN64/Combiner.h`
**Cambio:** Se agregó `default: constant.a = 1.0f;` al switch de alpha en
`SetConstant`.

Fuentes de alpha como `TEXEL0_ALPHA`, `TEXEL1_ALPHA`, `COMBINED`, `LOD_FRAUC`
no tenían case en el switch. El alpha quedaba en 0 (valor por defecto del
inicializador `rect[0].color`). Esto causaba que TODOS los textured rects con
estas fuentes de alpha muy comunes del N64 se renderizaran con alpha=0.

**Solución:** El default pone alpha=1.0, que es el comportamiento correcto para
rectángulos texturizados 2D donde el alpha real viene de la textura, no del
vertex color.

**Nota:** Fix E (gDPFillRectangle fillColor) NO afecta los cuadros de diálogo de
SM64 porque éstos se dibujan con `gDPTextureRectangle`, no con
`gDPFillRectangle`. La causa real era el vertex color alpha=0 de los Fixes F+G.

---

## Fix H — Resampler: upper boundary clamping (audio.c:187-227)

**Archivo:** `src/core/n64_audio/audio.c`
**Cambio:** Se agregó parámetro `in_frames` a `copy_to_buffer()` y se clampean
`idx2` e `idx3` al límite superior del buffer de entrada.

El resampler Catmull-Rom necesita 4 samples (s0, s1, s2, s3) por cada punto
de salida. El código original solo clampeaba `idx0` (límite inferior) pero
`idx2 = idx+1` e `idx3 = idx+2` podían leer más allá del final del chunk de
audio, accediendo a memoria inválida/garbage. Esto causaba distorsión severa
("parlante roto") al final de cada chunk de audio.

---

## Fix I — Resampler: cálculo preciso de consumo de input (audio.c:242-244)

**Archivo:** `src/core/n64_audio/audio.c`
**Cambio:** Se reemplazó `lengthi = rlengthi * freq_ratio` (truncamiento a int)
con la fórmula exacta: `in_consumed = (rlengthi-1) * freq_ratio + 1`.

El código original truncaba el resultado de `rlengthi * freq_ratio` a entero.
Esto perdía una fracción de input frame por cada chunk procesado. A 60fps,
la pérdida se acumulaba (~1 frame perdido por N64 frame), causando que el
resampler periódicamente re-lea la misma región del input → eco/repetición.

**Ejemplo con freq_ratio=0.6667:**
- 804 output frames: old consumía 800 (truncaba), nuevo consume 536 (correcto)
- Total por frame N64: old perdía ~1 input frame, nuevo es exacto

---

## Fix J — Buffers ampliados y más numerosos (audio.c:44-57)

**Archivo:** `src/core/n64_audio/audio.c`
**Cambio:** `NUM_BUFFERS` de 4 a 8, `BUFFER_SIZE` de 3840 a 4800 bytes.

El buffer anterior de 3200 bytes era borderline para el audio resampled a
48kHz (~3204 bytes/frame NTSC). Con 4800 bytes hay margen suficiente para
acumular datos entre llamadas a `play_buffer()` sin overflow. Más buffers (8)
dan más margen para la latencia del puerto de audio PS3.

---

## Estado Actual

| Componente | Estado |
|------------|--------|
| Menú / UI | Funcional |
| Interpretador puro | Funcional |
| Dynarec PPC | No funciona en RPCS3 (crash) |
| Gráficos RSX | Parcialmente funcional (Fixes A/A2: blend, Fix B: sprites, Fix D: alpha dither) |
| Texturas del HUD de SM64 | Pipeline verificado correcto; blend Fix A debería resolver (pendiente prueba) |
| Cuadros de diálogo de SM64 | Fix E aplicado: usa fillColor en vez de blendColor (pendiente prueba) |
| Audio | Funcional pero baja calidad |
| FPS | ~15 (SM64), ~10 (Zelda MM) — límite del interpretador |
| Fog | No implementado (sin función RSX) |
| Polygon offset | No implementado (sin función RSX) |

---

## Problemas Pendientes

### 1. Texturas del HUD de SM64
Las texturas de vidas/símbolos de Mario siguen corruptas a pesar del Fix 19
(forzar 32-bit). Posibles causas:
- Endianness incorrecta en `gDPLoadTLUT` (carga de paleta CI)
- El HUD puede usar framebuffer rendering que no pasa por `TextureCache_Load`
- Estado RSX incorrecto (blend, depth test) durante dibujado de HUD

### 2. Calidad de audio
El resampleo por interpolación lineal produce audio de baja calidad. Necesita:
- Resampleo de mayor calidad (cúbico o sinc)
- Reescritura del buffer manager para PS3 audioPort API

### 3. Dynarec en RPCS3
El dynarec genera código PPC nativo que RPCS3 no puede ejecutar.
Requeriría reescritura mayor del recompiler.

### 4. Funciones gráficas faltantes del RSX
- `rsxSetFogEnable` / `rsxSetFogColor` — No existen en PSL1GHT
- `rsxSetPolygonOffsetEnable` / `rsxSetPolygonOffset` — No existen en PSL1GHT
- Solución: registros NV40 raw (como se hizo con alpha test)

---

## Fix 24 — readIndex u64 → u32 (audio.c)

**Archivo:** `src/core/n64_audio/audio.c`
**Cambio:** `playOneBlock()` lee `config.readIndex` como `u32` en vez de `u64`.

`config.readIndex` es un puntero a un `u32` en memoria compartida que contiene el
índice del bloque actual (0-7). Antes se leía con `*(u64*)`, que en big-endian PPC
produce `(index << 32 | garbage)`. Como `2^32 % 8 == 0`, el resultado de
`(current_block + 1) % 8` dependía solo de los 4 bytes de basura, no del índice real.
Esto causaba que siempre se escribiera al mismo bloque → el hardware avanzaba sin
datos nuevos → artefacto "tu tu tu tu".

Corregido a `*(u32*)(uintptr_t)config.readIndex` para leer solo el índice.

---

## Fix 25 — THREADED_AUDIO activado (audio.c)

**Archivo:** `src/core/n64_audio/audio.c`
**Cambio:** Se define `THREADED_AUDIO` y se reescribe el path threaded de `play_buffer()`.

El path anterior usaba `sysEventQueueReceive` para esperar bloques nuevos. El path
nuevo usa polling de `readIndex` (igual que el SPU module de ps3soundlib) en un
thread dedicado que corre independientemente del R4300.

Flujo del thread de audio:
1. Espera `first_audio` para no arrancar antes de tener datos
2. Espera `buffer_full` (señal de `add_to_buffer()`)
3. Drena el buffer escribiendo bloques al puerto PS3 vía polling
4. Señala `buffer_empty` para liberar el slot del ring buffer

Se agregó variable `drain_level` para capturar el tamaño del buffer antes de que
`add_to_buffer()` lo resetee, evitando race condition entre producer/consumer.

---

## Fix 26 — AUDIO_SINE_TEST desactivado

**Archivo:** `src/core/n64_audio/audio.c`
**Cambio:** `#define AUDIO_SINE_TEST` → `// #define AUDIO_SINE_TEST`

El test de sine fue útil para confirmar que el artefacto venía del puerto de audio
RPCS3, no de nuestra pipeline. Ahora se desactiva para probar con audio real del N64.

---

## Fix 27 — Tiny3D migration (infrastructure bridge)

**Archivos:** `src/main/rsxutil.h`, `src/main/rsxutil.cpp`, `src/main/main.cpp`, `Makefile`
**Cambio:** Reemplazo del RSX init manual por Tiny3D library como capa de RSX management.

### Problema
El RSX init manual (`rsxInit` + `init_screen` + `setRenderTarget`) requería gestión
completa de buffers, command buffer, y render targets. Tiny3D ya maneja todo esto
internamente y proporciona una API más estable.

### Solución
1. **rsxutil.h** — Bridge header:
   - `#include <tiny3d.h>` + `#include <libfont.h>`
   - `#define context gcm_context` (evita linker conflict con Tiny3D's `tiny_gcmContextData *context`)
   - `#define display_width Video_Resolution.width` / `display_height Video_Resolution.height`
   - `rsxMemalign` → `tiny3d_AllocTexture` wrapper (sin align param, siempre 128-byte aligned)
   - `rsxFree` → no-op wrapper (Tiny3D bump allocator no soporta free)
   - Declaraciones de `gcm_context`, alpha test functions, `flip()`

2. **rsxutil.cpp** — Thin wrapper:
   - Define `gcmContextData *gcm_context = NULL`
   - `flip()` llama `tiny3d_Flip()` (buffer swap, wait, render target reset)

3. **main.cpp** — Init:
   - `tiny3d_Init(1<<20)` reemplaza `host_addr`/`init_screen()`/`setRenderTarget(curr_fb)`
   - `gcm_context = (gcmContextData *) tiny3d_Get_GCM_Context()`

4. **Makefile** — LIBS: `-ltiny3d -lfont` agregados

### Por qué funciona sin reescribir GraphicsRSX/IPLFont
El `#define rsxMemalign compat_rsunalign` redirige transparentemente las **30 llamadas**
a `rsxMemalign` a través del allocator de Tiny3D, previniendo la corrupción de memoria
que ocurriría si ambos allocators bump-allocate del mismo pool de RSX local memory.

### Riesgo
Si falla, `git checkout a68ecdf` revierte todo (commit anterior).

---

## Fix 28 — RSX local memory heap save/restore (OGL_Start/OGL_Stop)

**Archivos:** `src/main/rsxutil.h`, `src/main/rsxutil.cpp`, `src/video/glN64/OpenGL.cpp`
**Cambio:** Guarda/restaura la posición del bump allocator de Tiny3D entre ciclos de emulación.

### Problema
El bump allocator de Tiny3D (`heap_pointer`) solo avanza; `rsxFree` es no-op.
Cada ciclo OGL_Start/OGL_Stop (ROM load → emulation → ROM exit) consumía
memoria RSX local permanentemente. Tras ~4 ciclos, el pool se agotaba:
FBtex, fp_buffer, y todas las rsxMemalign volvían NULL. Crash en memcpy
con destino 0x0 (CIA=0x52e08, LR=0x531cc).

TTY.log confirmó: `FBtex=00000000`, `fp_buffer=00000000` — pool exhausto.

### Solución
1. **rsxutil.h** — `extern void *heap_pointer` + `rsxutil_save_heap()` / `rsxutil_restore_heap()`
2. **rsxutil.cpp** — Implementación: `heap_save = heap_pointer` / `heap_pointer = heap_save`
3. **OGL_Start (line ~555)** — `rsxutil_save_heap()` antes de FBtex allocation
4. **OGL_Stop (line ~617)** — NULL pointers + `rsxutil_restore_heap()` después de Destroy

### Por qué funciona
- `heap_pointer` es global (no static) en mm.c → accessible via extern
- Menú y Tiny3D alocan ANTES de OGL_Start → heap_save los preserva
- OGL alocan DESPUÉS de heap_save → heap_restore los libera completamente
- Al restaurar, el heap vuelve al punto donde estaba antes de OGL_Start,
  pero los datos en memoria siguen ahí (no se borran), así que el menú
  puede seguir usando sus recursos (fp_buffer, texturas) intactos

### Ciclo de vida
```
tiny3d_Init → menu allocs → [SAVE HEAP] → OGL_Start → emulation → OGL_Stop → [RESTORE HEAP] → menu continua
```

---

## Fix 29 — FrameBuffer textures para PS3/RSX

**Archivos:** `FrameBuffer.cpp`, `FrameBuffer.h`, `VI.cpp`
**Cambio:** Implementación completa del sistema FrameBuffer→textura para RSX

### Problema
Las funciones `FrameBuffer_SaveBuffer`, `FrameBuffer_RenderBuffer` y `FrameBuffer_RestoreBuffer`
tenían stubs vacíos en PS3. `VI_UpdateScreen` no llamaba a ninguna función de gestión de
framebuffers. Resultado: texturas de framebuffer (usadas por juegos como SMW64 para
screens de selección, fondos, etc.) se muestreaban como datos vacíos → negro o corrupto.

### Solución
1. **FrameBuffer_SaveBuffer** — Copia RDRAM→RSX con nearest-neighbor upscale:
   - Para FB existente: re-copia datos de RDRAM al buffer RSX ya alocado
   - Para FB nuevo: `rsxMemalign(128, textureBytes)`, copia con conversión
   - Formato: RGBA5551/RGBA8888→A8R8G8B8 con byte-swap `(pixel>>8)|(pixel<<24)`
   - Set up completo del descriptor `gcmTexture`

2. **FrameBuffer_RenderBuffer** — Dibuja quad fullscreen con la textura FB:
   - Proyección ortográfica `0..VI.width × VI.height..0`
   - Viewport completo `display_width × display_height`
   - Carga VP/FP/viewport + dibuja `GCM_TYPE_QUADS` con PASSTEX shader

3. **FrameBuffer_RestoreBuffer** — Mismo quad para restaurar color image

4. **VI_UpdateScreen** — Añadido ciclo save→render→restore (patrón del path GX):
   - `if (OGL.frameBufferTextures)` check con `FrameBuffer_FindBuffer`
   - Save → Render → ShowFPS → Restore → `VI.lastOrigin` update

### Notas
- `OGL.frameBufferTextures` default=0, habilitable desde menú PS3
- `#include "VI.h"` añadido a FrameBuffer.cpp para acceso a `VI.width/height`
- `rsxSetFragmentProgramParameter` + `rsxLoadFragmentProgramLocation` añadidos
  a RenderBuffer/RestoreBuffer (necesarios para cargar el shader en RSX)
