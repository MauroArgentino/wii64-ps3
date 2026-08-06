/**
 * Mupen64 - exception.c
 * Copyright (C) 2002 Hacktarux
 *               2010 emu_kidid
 *
 * Mupen64 homepage: http://mupen64.emulation64.com
 * email address: hacktarux@yahoo.fr
 *                emu_kidid@gmail.com
 * 
 * If you want to contribute to the project please contact
 * me first (maybe someone is already making what you are
 * planning to do).
 *
 *
 * This program is free software; you can redistribute it and/
 * or modify it under the terms of the GNU General Public Li-
 * cence as published by the Free Software Foundation; either
 * version 2 of the Licence, or any later version.
 *
 * This program is distributed in the hope that it will be use-
 * ful, but WITHOUT ANY WARRANTY; without even the implied war-
 * ranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public Licence for more details.
 *
 * You should have received a copy of the GNU General Public
 * Licence along with this program; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139,
 * USA.
 *
**/

#include "r4300.h"
#include "macros.h"
#include "exception.h"
#include "../n64_memory/memory.h"
#include <ppu-types.h>

#define doBreak()

void address_error_exception()
{
  printf("address_error_exception\n");
  r4300.stop=1;
  doBreak();     
}

void TLB_invalid_exception()
{
  if (r4300.delay_slot) {
    r4300.skip_jump = 1;
    printf("delay slot\nTLB refill exception\n");
    r4300.stop=1;
    doBreak();
  }
  printf("TLB invalid exception\n");
  r4300.stop=1;
  doBreak();
}

void XTLB_refill_exception(unsigned long long int addresse)
{
  printf("XTLB refill exception\n");
  r4300.stop=1;
  doBreak();   
}

/* BUILD 00130: Self-healing exception vectors.
 *
 * On real hardware the IPL copies the cartridge's first 1MB to RDRAM, which
 * places the N64 OS exception stub at 0x80000000 (TLB refill) and 0x80000180
 * (general). This emulator loads the ROM so that code/data segments map at
 * their own bases, so those two vectors are never populated and hold garbage
 * (for SM64: decompressor leftovers). Once the game clears BEV (Status bit 22)
 * the first interrupt jumps into that garbage -> SP/RA corruption -> cascading
 * TLB exceptions -> crash.
 *
 * Fix: scan RDRAM once for the game's installed osSetExceptionHandler-style
 * trampolines (lui k0/k1; addiu k0/k1; jr k0/k1; nop) and copy them into the
 * vectors right before the first BEV=0 exception is taken.
 */

#define EXC_VECTOR_GENERAL 0x80000180
#define EXC_VECTOR_TLBREF  0x80000000
#define EXC_RDRAM_WORDS    0x100000 /* 4MB / 4 */

static int exc_vectors_done = 0;

static u32 exc_read_word(u32 vaddr)
{
   return ((u32 *)rdramb)[(vaddr & 0x3FFFFF) >> 2];
}

static void exc_write_word(u32 vaddr, u32 val)
{
   address = vaddr;
   word = val;
   write_rdram();
}

static int exc_is_trampoline(u32 w0, u32 w1, u32 w2, u32 w3, u32 *target)
{
   int use_k1;
   if ((w0 & 0xFFFF0000) == 0x3C1A0000) use_k1 = 0;   /* lui k0 */
   else if ((w0 & 0xFFFF0000) == 0x3C1B0000) use_k1 = 1; /* lui k1 */
   else return 0;
   if (w3 != 0) return 0;                              /* nop */
   if (!use_k1) {
      if (w2 != 0x03400008) return 0;                  /* jr k0 */
      if ((w1 & 0xFFFF0000) != 0x275A0000) return 0;   /* addiu k0,k0 */
   } else {
      if (w2 != 0x03E00008) return 0;                  /* jr k1 */
      if ((w1 & 0xFFFF0000) != 0x275B0000) return 0;   /* addiu k1,k1 */
   }
   if (target) {
      u32 hi = w0 & 0xFFFF, lo = w1 & 0xFFFF;
      *target = (lo & 0x8000) ? (((hi + 1) << 16) | (lo - 0x10000)) : ((hi << 16) | lo);
   }
   return 1;
}

static void exc_install_vectors(void)
{
   int i;
   u32 t;
   int general_valid, refill_valid, have_general, have_refill;
   u32 gFound[4] = {0}, rFound[4] = {0};
   u32 gT = 0, rT = 0;

   if (exc_vectors_done) return;
   exc_vectors_done = 1;
   if (!rdramb) return;

   general_valid = exc_is_trampoline(exc_read_word(EXC_VECTOR_GENERAL),
                                     exc_read_word(EXC_VECTOR_GENERAL + 4),
                                     exc_read_word(EXC_VECTOR_GENERAL + 8),
                                     exc_read_word(EXC_VECTOR_GENERAL + 12), &t);
   refill_valid = exc_is_trampoline(exc_read_word(EXC_VECTOR_TLBREF),
                                    exc_read_word(EXC_VECTOR_TLBREF + 4),
                                    exc_read_word(EXC_VECTOR_TLBREF + 8),
                                    exc_read_word(EXC_VECTOR_TLBREF + 12), &t);

   printf("[VECFIX] general@%08X=%08X %08X %08X %08X refill@%08X=%08X %08X %08X %08X valid=%d/%d\n",
      EXC_VECTOR_GENERAL,
      exc_read_word(EXC_VECTOR_GENERAL), exc_read_word(EXC_VECTOR_GENERAL + 4),
      exc_read_word(EXC_VECTOR_GENERAL + 8), exc_read_word(EXC_VECTOR_GENERAL + 12),
      EXC_VECTOR_TLBREF,
      exc_read_word(EXC_VECTOR_TLBREF), exc_read_word(EXC_VECTOR_TLBREF + 4),
      exc_read_word(EXC_VECTOR_TLBREF + 8), exc_read_word(EXC_VECTOR_TLBREF + 12),
      general_valid, refill_valid);

   if (general_valid && refill_valid) { printf("[VECFIX] both vectors already valid, skip\n"); return; }

   have_general = general_valid;
   have_refill = refill_valid;

   for (i = 0; i < EXC_RDRAM_WORDS - 3; i++) {
      u32 w0 = ((u32 *)rdramb)[i];
      u32 w1 = ((u32 *)rdramb)[i + 1];
      u32 w2 = ((u32 *)rdramb)[i + 2];
      u32 w3 = ((u32 *)rdramb)[i + 3];
      if (exc_is_trampoline(w0, w1, w2, w3, &t)) {
         u32 vaddr = 0x80000000 + i * 4;
         printf("[VECFIX] found trampoline @%08X -> %08X\n", vaddr, t);
         if (!have_general) {
            gFound[0] = w0; gFound[1] = w1; gFound[2] = w2; gFound[3] = w3;
            gT = t; have_general = 1;
         } else if (!have_refill && t != gT) {
            rFound[0] = w0; rFound[1] = w1; rFound[2] = w2; rFound[3] = w3;
            rT = t; have_refill = 1;
         }
         if (have_general && have_refill) break;
      }
   }

   if (!general_valid) {
      if (have_general && gT) {
         for (i = 0; i < 4; i++) exc_write_word(EXC_VECTOR_GENERAL + i * 4, gFound[i]);
         printf("[VECFIX] installed general vector -> %08X\n", gT);
      } else {
         printf("[VECFIX] WARNING: no trampoline found for general vector\n");
      }
   }
   if (!refill_valid) {
      if (have_refill && rT) {
         for (i = 0; i < 4; i++) exc_write_word(EXC_VECTOR_TLBREF + i * 4, rFound[i]);
         printf("[VECFIX] installed refill vector -> %08X\n", rT);
      } else {
         printf("[VECFIX] WARNING: no distinct trampoline found for refill vector\n");
      }
   }
}

void TLB_refill_exception(u32 address, int w)
{
  int usual_handler = 0, i = 0;
  
  exc_install_vectors();
  if (!dynacore && w != 2) {
    update_count();
  }
  Cause = (w == 1) ? (3 << 2):(2 << 2);
  BadVAddr = address;
  Context = (Context & 0xFF80000F) | ((address >> 9) & 0x007FFFF0);
  EntryHi = address & 0xFFFFE000;
  if (Status & 0x2) { // Test de EXL
    r4300.pc = 0x80000180;
    
    if(r4300.delay_slot==1 || r4300.delay_slot==3) {
      Cause |= 0x80000000;
    }
    else {
      Cause &= 0x7FFFFFFF;
    }
  }
  else {
	if(w==2) {
		EPC = address;
	} else {
		EPC = r4300.pc;
	}
       
    Cause &= ~0x80000000;
    Status |= 0x2; //EXL=1

    if (address >= 0x80000000 && address < 0xc0000000) {
      usual_handler = 1;
    }
    for (i=0; i<32; i++) {
      if (address >= tlb_e[i].start_even && address <= tlb_e[i].end_even) {
        usual_handler = 1;
      }
      if (address >= tlb_e[i].start_odd && address <= tlb_e[i].end_odd) {
        usual_handler = 1;
      }
    }
    if (usual_handler) {
      r4300.pc = 0x80000180;
    }
    else {
      r4300.pc = 0x80000000;
    }
  }
  
  if(r4300.delay_slot==1 || r4300.delay_slot==3) {
    Cause |= 0x80000000;
    EPC-=4;
  }
  else {
    Cause &= 0x7FFFFFFF;
  }
  
  if(w != 2) {
    EPC-=4;  //wii64: wtf is w != 2 ?
  }
   
  r4300.last_pc = r4300.pc;
   
  
  if (r4300.delay_slot) {
    r4300.skip_jump = r4300.pc;
    r4300.next_interrupt = 0;
  }
  
}

void TLB_mod_exception()
{
  printf("TLB mod exception\n");
  r4300.stop=1;
  doBreak();
}

void integer_overflow_exception()
{
  printf("integer overflow exception\n");
  r4300.stop=1;
  doBreak();
}

void coprocessor_unusable_exception()
{
  printf("coprocessor_unusable_exception\n");
  r4300.stop=1;
  doBreak();
}

void exception_general()
{
  update_count();
  Status |= 2;
   
  EPC = r4300.pc;

  if(r4300.delay_slot==1 || r4300.delay_slot==3) {
    Cause |= 0x80000000;
    EPC-=4;
  }
  else {
    Cause &= 0x7FFFFFFF;
  }
  r4300.pc = 0x80000180;
  r4300.last_pc = r4300.pc;

    if (r4300.delay_slot) {
      r4300.skip_jump = r4300.pc;
      r4300.next_interrupt = 0;
    }

}
