# AGENTS.md

## What is this

PS3 homebrew N64 emulator (wii64-ps3) -- a 2011 port of Wii64/mupen64 to PlayStation 3 using PSL1GHT SDK. Written in C/C++ for the Cell Broadband Engine (PPU). Proof-of-concept stage: pure interpreter only, no audio, incomplete RSX graphics.

## Build

Requires **PSDK3v2** at `C:/PSDK3v2` and PSL1GHT SDK (set `PSL1GHT` env var). The Makefile builds with `ppu-gcc` targeting Cell.

**PROVEN WORKING CONFIG ON THIS MACHINE (2026-08-03).** The MSYS GNU make 3.81 bundled with PSDK3v2 **crashes or fails with `cp: Command not found` / `No such file` when invoked directly from PowerShell/CMD with a Windows-style PATH.** The Makefile re-exports `PATH := /c/PSDK3v2/...:$(PATH)` (line 3), and mixing MSYS + Windows path forms breaks make 3.81's direct command spawn AND the recursive `$(MAKE)` expansion. The only reliable invocation is **from inside the native MSYS shell**:

```bash
# From PowerShell, run make through the MSYS login shell (the -l login profile is required):
& 'C:/PSDK3v2/mingw/msys/1.0/bin/bash.exe' -lc 'cd /c/Users/tupri/OneDrive/Documentos/PROGRAMACION/wii64-ps3 && export PATH="/c/PSDK3v2/mingw/msys/1.0/bin:/c/PSDK3v2/mingw/bin:/c/PSDK3v2/ps3dev/bin:/c/PSDK3v2/ps3dev/ppu/bin:/c/PSDK3v2/ps3dev/spu/bin:$PATH" && make'

# Env vars inside bash (set by the command above; MSYS paths, NOT Windows C:/ form):
#   export PSL1GHT=/c/PSDK3v2/psl1ght
#   export PS3DEV=/c/PSDK3v2/ps3dev
#   export PS3SDK=c:/PSDK3v2
```

Once inside the MSYS shell, the normal targets work:

```bash
# Release build (glN64 video plugin) -- produces ps364_glN64.self
make

# Debug build (-O0 -g3) -- produces ps364_debug.self
make dbg

# Build a PS3-installable .pkg
make pkg

# Software renderer variant (stale -- Makefile.ps3_soft references dirs that don't exist)
```

**Gotchas when building on this machine:**
- Do NOT call `make.exe` directly from PowerShell/CMD. Use the `bash.exe -lc '...'` wrapper above.
- `SPU pre-built artifacts found` is normal (spu_core_elf.o already exists in `src/platform/ps3/spu_core/`).
- The `%llx` warnings in `wii64_cached_interp.c` are pre-existing (ppu `unsigned long` is 64-bit); harmless.
- The recursive `$(MAKE)` inside the `build` target only works when make's own path resolves cleanly in MSYS -- another reason to stay inside the MSYS shell.
- Some `build/*.o` and the existing `.self` may have come from the original dev machine (`J:/PS3/PSDK3v2`, see `Make_PKG.bat`) -- not necessarily built here. A clean rebuild is the only ground truth.
- No tests, no linter, no formatter, no typecheck, no CI. The only verification is a successful build.

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
- **Cross-platform compat header:** `src/main/winlnxdefs.h` force-included via `-include` in CFLAGS
- **C++ with `-fpermissive -fno-rtti -fno-exceptions`.** Many `.c` files compiled as C++ via the build system (Makefile uses CXX for linking when any .cpp present)
- **Source flattened at build time.** Makefile uses `$(notdir)` on all sources -- all `.o` files go into `build/` flat. Don't rely on subdirectory structure in object names.
- **Shaders** (`.vcg`/`.fcg`) in `src/platform/ps3/shaders/` compiled to binary via `bin2o`.
- **SPU core** (`src/platform/ps3/spu_core/`) compiled separately with `spu-gcc`, embedded into PPU ELF via `bin2s`.
- **Pure interpreter only on RPCS3.** PPC dynarec generates native PPC code that crashes RPCS3 (ppu thread abort). `go()` in `r4300.c` forces `dynacore==2`.
- **audio.c uses port 1.** `ps3audio_backend` (`src/main/ps3audio_backend.cpp`) opens port 0 and conflicts with audio.c -- keep it commented out.
- **Detailed changelog** in `CAMBIOS.md` (Spanish, covers ~26 fixes to audio, graphics, dynarec).
- **Code comments mix English and Spanish.** README is in Spanish.

## Audio reference

- `audioPortConfig.readIndex` is `u32` (index, NOT pointer), `audioDataStart` is `u32` (address)
- `AUDIO_BLOCK_SAMPLES = 256`, `NUM_BUFFERS = 8`, each block = 2048 bytes (256 × 2ch × float)
- Port always plays at 48kHz; `AiDacrateChanged()` forces `real_freq = 48000`
- THREADED_AUDIO path uses polling of `readIndex` in a dedicated thread
- A sine test is available by uncommenting `#define AUDIO_SINE_TEST` at the top of `audio.c`

## Gotchas

- `Makefile.ps3_soft` references source directories that don't exist in the current layout -- stale.
- `Makefile.win` is a Dev-C++ file with flat source paths (no `src/` prefix) -- does **not** match the current tree.
- `gitk3y.txt` in the repo root contains an exposed GitHub PAT. In `.gitignore` but already committed.
- The `build/` directory has committed `.o`/`.d` files from a previous build (gitignored but present in tree).
- ELF has strict BSS limits. Large static arrays (rdram, tlb_LUT) must be heap-allocated via `malloc()` (see Fix 17 in CAMBIOS.md).
- `rsxSetAlphaTestEnable`/`rsxSetAlphaTestFunc`/`rsxSetAlphaTestRef` don't exist in PSL1GHT headers -- implemented via NV40 raw register writes in `src/main/rsxutil.cpp`.
- `src/config.h`: simple config -- `GTK2_SUPPORT 1`, no `WITH_HOME`, no `VCR_SUPPORT`.
- `tools/Wii64TxEditor/` is a separate sub-project in this workspace.
- Menu audio generated by `tools/gen_menu_audio.ps1` (PowerShell script, outputs `menu_audio.wav` at repo root).

## Cached interpreter bugs (found 2026-07-30)

**Root cause:** `r4300.pc` semantics differ between pure and cached interpreter. The pure interpreter advances `r4300.pc` by 4 every instruction; the cached interpreter only sets it in branch/exception ops. `exception_general()` uses `r4300.pc` for `EPC` — gets garbage.

**Fixes applied:**

1. **SYSCALL missing from funct switch** (`wii64_cached_interp.c:335`). Funct `0x0C` not in SPECIAL switch → fell to `default: cached_interp_NI`. With `code=0` (common), NOP override caught it first → SYSCALL became NOP. **Fix:** added `case 0x0C`, excluded from NOP override.

2. **`do_branch_not_taken` double-executes** (`wii64_cached_ops.c:385`). Old `PC++; PC->ops()` — main loop also calls `PC->ops()`. **Fix:** `(PC+1)->ops(); PC++;`.

3. **SYSCALL/BREAK handler PC sync** (`wii64_cached_ops.c:359`). `exception_general()` sets `r4300.pc = 0x80000180` but `PC` not updated. **Fix:** `cached_interpreter_jump_to(r4300.pc)` after `exception_general()`. Removed stale `r4300.pc = PC->addr` line from SYSCALL (main loop now sets it).

4. **Count never advances — black screen** (main loop). Pure interpreter advances `Count` via `update_count()` using PC deltas; cached ALU ops never touch `r4300.pc`, so Count stalls. **Fix:** `Count += 2` per instruction in main loop; removed all `update_count()`/`gen_interrupt()` from individual ops.

5. **r4300.pc stale for exception_general** (main loop). `exception_general()` does `EPC = r4300.pc` — needs current instruction address. Cached ops never update `r4300.pc` for ALU. **Fix:** main loop sets `r4300.pc = PC->addr` before each instruction. Branch handlers set `r4300.pc = (PC+1)->addr` (delay slot) for correct `EPC` on delay-slot exceptions. `r4300.delay_slot = 1` set before branch delay slot execution.

6. **check_cop1_unusable callers** (~53 FPU ops) did `{ PC++; return; }` after exception — advances past exception vector instead of jumping to it. **Fix:** `{ cached_interpreter_jump_to(r4300.pc); return; }`.

7. **Branch overwrites exception PC** — `do_branch_taken` calls `cached_interpreter_jump_to(target)` after delay slot, overwriting exception vector set by `check_cop1_unusable`. **Fix:** save `old_pc = r4300.pc` before delay slot, only jump to target if `r4300.pc == old_pc` (no exception). Same for `do_branch_not_taken` PC++.

8. **BLTZL/BGEZL used wrong helpers** — called `do_branch_taken/not_taken` instead of likely variants. **Fix:** changed to `do_branch_likely_taken/not_taken`.

**Remaining issues:** Trap handlers (TGE, TLT, TEQ, TNE, TGEI, etc.) and `cached_interp_BREAK` set `r4300.stop = 1` instead of calling `exception_general()` — won't crash the emulator but traps silently halt it. Not the cause of the black screen.

## Crash prevention (addressed 2026-07-30)

Known RPCS3 crash: "VM: Access violation writing location 0x0 (unmapped memory)" after prolonged emulation. Root cause: none of the 40+ `malloc`/`calloc`/`rsxMemalign` sites checked for NULL. Once VRAM or heap runs out, writes through NULL pointers crash RPCS3/PS3.

### Fixes applied

**Memory system** (`memory.c`, `tlb.c`, `dma.c`):
- `init_memory()`: check all 3 malloc returns for rdram, tlb_LUT_r, tlb_LUT_w; return -1 on failure
- All `write_rdram*()`: guard `rdramb` with null check → discard to trash variable
- `virtual_to_physical_address()`, `probe_nop()`: guard tlb_LUT_r/tlb_LUT_w/rdram
- All DMA functions (`dma_pi_*`, `dma_sp_*`, `dma_si_*`): guard `rdram` with early return

**RSX video** (`Textures.cpp`, `OpenGL.cpp`):
- `TextureCache_Load()`: check `dest` malloc AND `rsxMemalign` returns → skip upload on failure
- `TextureCache_LoadBackground()`: same
- `TextureCache_ActivateTexture()`: check `texture->rsxTextureBuffer` → activate dummy if NULL
- `OGL_InitStates()`: check `fp_buffer` allocation return

**CPU core** (`r4300.c`, `pure_interp.c`):
- `init_blocks()`: check `calloc` for `blocks` array, check `malloc` for initial block
- `invalidate_func()`: check `blocks_get` return for NULL

**ROM load** (`main.cpp`, `CurrentRomFrame.cpp`):
- Check `init_memory()` return value → show error and abort load on failure

## TLB refactoring (addressed 2026-07-30)

`pure_interp.c` had ~220 lines of duplicated map/unmap LUT logic in `TLBWI()` and `TLBWR()`, differing only by index (`Index&0x3F` vs `Random`).

Extracted into shared functions in `tlb.c`:
- `tlb_unmap(int index)` — clears LUT entries for a TLB entry
- `tlb_map(int index)` — populates LUT entries from a TLB entry

Also fixed: `TLBWR()` dead code had `i>>2` where `i>>12` was intended (`#ifdef USE_TLB_CACHE` path, never built).

**Signedness fix**: `tlb.h` fields `vpn2`, `pfn_even`, `pfn_odd` changed from `s32` to `u32` — addresses are unsigned in the N64; signed types can cause incorrect sign-extension in comparisons on addresses > 0x80000000.

## Cached interpreter crash fixes (addressed 2026-07-30)

### Crash: FIN_BLOCK / NOTCOMPILED infinite recursion → stack overflow

**Root cause:** `generic_jump_to()` / `cached_interp_recompile_block()` could fail silently (malloc failure, stale block state) without updating `PC`. `FIN_BLOCK` and `NOTCOMPILED` called `PC->ops()` unconditionally, re-executing themselves in infinite recursion.

**Fixes:**
- `cached_interp_FIN_BLOCK()`: save `PC` before `generic_jump_to()`; compare after; if unchanged → `printf("FIN_BLOCK stuck")` + `r4300.stop = 1`
- `cached_interp_NOTCOMPILED()`: check `PC->ops` after `recompile_block()`; if still `NOTCOMPILED` → `printf("NOTCOMPILED stuck")` + `r4300.stop = 1`

### Crash: read_inst OOB on 4MB RDRAM

**Root cause:** `read_inst()` used `(addr & 0xFFFFFF) / 4` as index into `rdram[]` without checking actual RDRAM size. With `RDRAM_SIZE=4MB` (no `USE_EXPANSION`), addresses `0x80400000-0x807FFFFF` read past the allocation.

**Fix:** Added `RDRAM_WORDS` constant (`0x100000`), guard both direct and TLB paths with `if (index < RDRAM_WORDS)`.

### init_block: premature invalid_code clear

**Root cause:** In the `else` branch of `cached_interp_init_block()` (TLB-mapped addresses), `ci.invalid_code[paddr>>12] = 0` was set *before* the recursive `cached_interp_init_block(paddr)` call. If the recursive call failed (2nd malloc failure), the page stayed marked valid with no block array.

**Fix:** Removed premature `invalid_code` clears; let `cached_interp_init_block` manage `invalid_code` internally (line 295).

### PC NULL guard in main loop

`run_cached_interpreter()` main loop: added `if (!PC) { r4300.stop = 1; break; }` before `PC->addr` dereference.

### read_rdram NULL guard

`read_rdram()`, `read_rdramb()`, `read_rdramh()`, `read_rdramd()`: added `if (!rdramb) { *rdword = 0; return; }` matching existing write-side guards.

### Trap handlers use exception_general (addressed 2026-07-30)

All 12 trap opcodes (TEQ, TGE, TGEU, TLT, TLTU, TNE, TGEI, TGEIU, TLTI, TLTIU, TEQI, TNEI) called `r4300.stop = 1` on trap condition instead of raising a proper exception. **Fix:** each now sets `Cause = (13 << 2)` and calls `exception_general()` + `cached_interpreter_jump_to(r4300.pc)` + `return` (skipping PC++).

Also fixed SYSCALL and BREAK in the cached interpreter: they now set `Cause = (8 << 2)` and `Cause = (9 << 2)` respectively before calling `exception_general()`, matching the pure interpreter's behavior.
