#ifndef WII64_CACHED_INTERP_H
#define WII64_CACHED_INTERP_H

#include <ppu-types.h>
#include "recomp.h"

struct precomp_block
{
   precomp_instr* block;
   u32 start;
   u32 end;
   u32 xxhash;
};

struct cached_interp
{
   char* invalid_code;
   struct precomp_block** blocks;
   struct precomp_block* actual;
};

extern struct cached_interp ci;

void init_cached_blocks(void);
void free_cached_blocks(void);
void cached_interp_init_block(u32 address);
void cached_interp_recompile_block(u32 address);
void cached_interpreter_jump_to(u32 address);
void run_cached_interpreter(void);
void invalidate_cached_code(u32 address, u32 size);

#endif