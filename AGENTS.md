# AGENTS.md

## What is this

PS3 homebrew N64 emulator (wii64-ps3) -- a 2011 port of Wii64/mupen64 to PlayStation 3 using PSL1GHT SDK. Written in C/C++ for the Cell Broadband Engine (PPU). Proof-of-concept stage: pure interpreter only, no audio, incomplete RSX graphics.

## Build

Requires **PSDK3v2** at `C:/PSDK3v2` and PSL1GHT SDK. The Makefile sets `PS3SDK=c:/PSDK3v2` and builds with `ppu-gcc` targeting Cell.

```bash
# Primary build (glN64 video plugin) -- produces ps364_glN64.self
make

# Build a PS3-installable .pkg
make pkg

# Software renderer variant (uses Makefile.ps3_soft, source dirs differ)
# Copy Makefile.ps3_soft -> Makefile.ps3 and run make
```

**Windows shortcut:** `Make_PKG.bat` sets env vars and runs `make pkg` (paths hardcoded to `J:/PS3/PSDK3v2` -- adjust before use).

There are no tests, no linter, no formatter, no typecheck. The only verification is a successful build.

## Architecture

```
src/main/main.cpp        -- entrypoint (PS3 main()), init, menu loop, go() call
src/core/r4300/          -- N64 R4300i CPU (pure_interp.c, r4300.c, PPC dynarec/)
src/core/n64_memory/     -- RDRAM, TLB, DMA, flash RAM, PIF
src/core/rsp/            -- RSP HLE (ucode1-3.cpp)
src/core/n64_input/      -- PS3 pad -> N64 controller mapping
src/core/n64_audio/      -- Audio plugin (audio.c = PS3 port, Audio_#1.1.c = LLE ucode)
src/video/glN64/         -- glN64 graphics (RSX_VideoBackend.cpp bridges to PS3 GPU)
src/video/SoftGFX/       -- Software renderer alternative
src/ui/                  -- Menu system (libgui/ custom widget framework)
src/platform/ps3/        -- PS3-specific code (menu manager, dynarec mem, shaders)
```

Flow: `main()` -> RSX/pad init -> `MenuContext` menu loop -> user picks ROM -> `go()` in `r4300.c` runs emulation -> Square+Triangle combo sets `r4300.stop=1` -> returns to menu.

## Key conventions

- **Big-endian PPC target.** Defines: `-DPPC -D_BIG_ENDIAN -DPS3 -DPPC_DYNAREC -DUSE_RECOMP_CACHE -D__PSL1GHT__`
- **Cross-platform compat header:** `src/main/winlnxdefs.h` is force-included via `-include` in CFLAGS -- provides Windows/Linux type shims.
- **C++ with `-fpermissive -fno-rtti -fno-exceptions`.** Many `.c` files are compiled as C++ via the build system.
- **Source flattened at build time.** Makefile uses `$(notdir)` on all sources -- all `.o` files go into `build/` flat. Don't rely on subdirectory structure in object names.
- **Shaders** (`.vcg`/`.fcg`) in `src/platform/ps3/shaders/` are compiled to binary via `bin2o`.
- **Code comments mix English and Spanish.** README is in Spanish.

## Gotchas

- `Makefile.win` is a historical Dev-C++ file with different source paths (`r4300/` flat, `menu/`, `glN64_GX/`, etc.) -- it does **not** match the current `src/` tree. Do not use it as a reference for current structure.
- `Makefile.ps3_soft` references source directories (`gc_audio`, `mupen64_soft_gfx`, etc.) that don't exist in the current `src/` layout -- it's stale from before the project restructuring.
- `gitk3y.txt` in the repo root contains an exposed GitHub PAT. It's in `.gitignore` but already committed to git history. Do not commit further secrets.
- The `build/` directory contains committed `.o`/`.d` files from a previous build. These are gitignored but present in the working tree.
- No CI/CD. Build verification is entirely manual.

## Audio investigation status (as of 2026-07-15)

### The "chu chu chu" artifact
- **PCM dump inside emulator is CLEAN** -- no artifact in our audio data
- **RPCS3 WAV output has artifact at exactly 187.5Hz** (48000/256 = audio block rate)
- Envelope correlation between our PCM dump and RPCS3 WAV is **-0.03** (uncorrelated)
- Artifact has increasing correlation at harmonics: 187Hz:0.38, 250Hz:0.51, 300Hz:0.58, 375Hz:0.68

### RPCS3 audio port internals (from source analysis)
- RPCS3 uses **tag-based block detection**: writes `-0.0f` sentinels at 6 positions per block
- When game overwrites all tags, thread knows block is ready to mix
- `readIndex` = next block to be mixed (set after advance, before event)
- **Untouched blocks for >10.7ms → silence gap inserted**
- Known crackling issue: RPCS3 #11209 (ring buffer sizing), #13310 (further fixes)

### Fixes attempted (all FAILED to change audio)
1. readIndex as index (was dereferenced as pointer)
2. audioGetPortConfig() refresh before use
3. Removed +1 offset in block index calculation

### Next steps for tomorrow
1. **Sine wave test tone** -- bypass N64 audio entirely, write pure sine to port. If clean → our pipeline is the problem. If clicks → RPCS3 port is the problem
2. **Try THREADED_AUDIO** -- non-threaded path may have timing issues
3. **Research RPCS3 audio workarounds** -- check if there are settings or hacks

### PS3 audio API reference
- `audioPortConfig.readIndex` is `u32` (index, NOT pointer)
- `audioPortConfig.audioDataStart` is `u32` (address)
- `AUDIO_BLOCK_SAMPLES = 256`, `NUM_BUFFERS = 8`
- Each block = 256 × 2 channels × sizeof(float) = 2048 bytes
- Port always plays at 48kHz
