#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ppu-types.h>
#include "r4300.h"
#include "wii64_cached_interp.h"
#include "exception.h"
#include "../n64_memory/memory.h"
#include "macros.h"
#include "interrupt.h"

extern u32 op;

void cached_interp_SLL(void)
{
   rrd32 = (u32)(rrt32) << rsa;
   sign_extended(rrd);
   PC++;
}

void cached_interp_SRL(void)
{
   rrd32 = (u32)rrt32 >> rsa;
   sign_extended(rrd);
   PC++;
}

void cached_interp_SRA(void)
{
   rrd32 = (s32)rrt32 >> rsa;
   sign_extended(rrd);
   PC++;
}

void cached_interp_SLLV(void)
{
   rrd32 = (u32)(rrt32) << (rrs32 & 0x1F);
   sign_extended(rrd);
   PC++;
}

void cached_interp_SRLV(void)
{
   rrd32 = (u32)rrt32 >> (rrs32 & 0x1F);
   sign_extended(rrd);
   PC++;
}

void cached_interp_SRAV(void)
{
   rrd32 = (s32)rrt32 >> (rrs32 & 0x1F);
   sign_extended(rrd);
   PC++;
}

void cached_interp_MFHI(void)
{
   LOW_WORD(rrd) = LOW_WORD(r4300.hi);
   sign_extended(rrd);
   PC++;
}

void cached_interp_MTHI(void)
{
   r4300.hi = rrs;
   PC++;
}

void cached_interp_MFLO(void)
{
   LOW_WORD(rrd) = LOW_WORD(r4300.lo);
   sign_extended(rrd);
   PC++;
}

void cached_interp_MTLO(void)
{
   r4300.lo = rrs;
   PC++;
}

void cached_interp_DSLLV(void)
{
   rrd = rrt << (rrs32 & 0x3F);
   PC++;
}

void cached_interp_DSRLV(void)
{
   rrd = (u64)rrt >> (rrs32 & 0x3F);
   PC++;
}

void cached_interp_DSRAV(void)
{
   rrd = (s64)rrt >> (rrs32 & 0x3F);
   PC++;
}

void cached_interp_MULT(void)
{
   s64 result = (s64)(s32)rrs32 * (s64)(s32)rrt32;
   r4300.lo = (s64)(s32)(result & 0xFFFFFFFF);
   r4300.hi = result >> 32;
   PC++;
}

void cached_interp_MULTU(void)
{
   u64 result = (u64)(u32)rrs32 * (u64)(u32)rrt32;
   r4300.lo = (s64)(s32)(result & 0xFFFFFFFF);
   r4300.hi = result >> 32;
   PC++;
}

void cached_interp_DIV(void)
{
   if (rrt32)
   {
      r4300.lo = (s64)(rrs32 / rrt32);
      r4300.hi = (s64)(rrs32 % rrt32);
   }
   PC++;
}

void cached_interp_DIVU(void)
{
   if (rrt32)
   {
      r4300.lo = (s64)(s32)((u32)rrs32 / (u32)rrt32);
      r4300.hi = (s64)(s32)((u32)rrs32 % (u32)rrt32);
   }
   PC++;
}

void cached_interp_DMULT(void)
{
   r4300.lo = rrs * rrt;
   r4300.hi = 0;
   asm("mulld %0,%1,%2; mulhdu %3,%1,%2"
       : "=r"(r4300.lo), "=r"(r4300.hi)
       : "r"(rrs), "r"(rrt));
   PC++;
}

void cached_interp_DMULTU(void)
{
   asm("mulld %0,%1,%2; mulhdu %3,%1,%2"
       : "=r"(r4300.lo), "=r"(r4300.hi)
       : "r"(rrs), "r"(rrt));
   PC++;
}

void cached_interp_DDIV(void)
{
   if (rrt)
   {
      r4300.lo = (s64)rrs / (s64)rrt;
      r4300.hi = (s64)rrs % (s64)rrt;
   }
   PC++;
}

void cached_interp_DDIVU(void)
{
   if (rrt)
   {
      r4300.lo = (u64)rrs / (u64)rrt;
      r4300.hi = (u64)rrs % (u64)rrt;
   }
   PC++;
}

void cached_interp_ADD(void)
{
   rrd32 = rrs32 + rrt32;
   sign_extended(rrd);
   PC++;
}

void cached_interp_ADDU(void)
{
   rrd32 = rrs32 + rrt32;
   sign_extended(rrd);
   PC++;
}

void cached_interp_SUB(void)
{
   rrd32 = rrs32 - rrt32;
   sign_extended(rrd);
   PC++;
}

void cached_interp_SUBU(void)
{
   rrd32 = rrs32 - rrt32;
   sign_extended(rrd);
   PC++;
}

void cached_interp_AND(void)
{
   rrd = rrs & rrt;
   PC++;
}

void cached_interp_OR(void)
{
   rrd = rrs | rrt;
   PC++;
}

void cached_interp_XOR(void)
{
   rrd = rrs ^ rrt;
   PC++;
}

void cached_interp_NOR(void)
{
   rrd = ~(rrs | rrt);
   PC++;
}

void cached_interp_SLT(void)
{
   rrd = (rrs < rrt) ? 1 : 0;
   PC++;
}

void cached_interp_SLTU(void)
{
   rrd = ((u64)rrs < (u64)rrt) ? 1 : 0;
   PC++;
}

void cached_interp_DADD(void)
{
   rrd = rrs + rrt;
   PC++;
}

void cached_interp_DADDU(void)
{
   rrd = rrs + rrt;
   PC++;
}

void cached_interp_DSUB(void)
{
   rrd = rrs - rrt;
   PC++;
}

void cached_interp_DSUBU(void)
{
   rrd = rrs - rrt;
   PC++;
}

void cached_interp_TEQ(void)
{
   if (rrs == rrt)
   {
      printf("trap exception in teq\n");
      r4300.stop = 1;
   }
   PC++;
}

void cached_interp_TGE(void)
{
   if (rrs >= rrt)
   {
      printf("trap exception in tge\n");
      r4300.stop = 1;
   }
   PC++;
}

void cached_interp_TGEU(void)
{
   if ((u64)rrs >= (u64)rrt)
   {
      printf("trap exception in tgeu\n");
      r4300.stop = 1;
   }
   PC++;
}

void cached_interp_TLT(void)
{
   if (rrs < rrt)
   {
      printf("trap exception in tlt\n");
      r4300.stop = 1;
   }
   PC++;
}

void cached_interp_TLTU(void)
{
   if ((u64)rrs < (u64)rrt)
   {
      printf("trap exception in tltu\n");
      r4300.stop = 1;
   }
   PC++;
}

void cached_interp_TNE(void)
{
   if (rrs != rrt)
   {
      printf("trap exception in tne\n");
      r4300.stop = 1;
   }
   PC++;
}

void cached_interp_DSLL(void)
{
   rrd = rrt << rsa;
   PC++;
}

void cached_interp_DSRL(void)
{
   rrd = (u64)rrt >> rsa;
   PC++;
}

void cached_interp_DSRA(void)
{
   rrd = (s64)rrt >> rsa;
   PC++;
}

void cached_interp_DSLL32(void)
{
   rrd = rrt << (32 + rsa);
   PC++;
}

void cached_interp_DSRL32(void)
{
   rrd = (u64)rrt >> (32 + rsa);
   PC++;
}

void cached_interp_DSRA32(void)
{
   rrd = (s64)rrt >> (32 + rsa);
   PC++;
}

void cached_interp_SYSCALL(void)
{
   r4300.pc = PC->addr;
   exception_general();
   PC++;
}

void cached_interp_BREAK(void)
{
   printf("BREAK\n");
   r4300.stop = 1;
   PC++;
}

void cached_interp_SYNC(void)
{
   PC++;
}

static void do_branch_taken(s32 offset)
{
   u32 target = PC->addr + 4 + offset;
   (PC + 1)->ops();
   cached_interpreter_jump_to(target);
}

static void do_branch_not_taken(void)
{
   PC++;
   PC->ops();
}

static void do_branch_likely_taken(s32 offset)
{
   u32 target = PC->addr + 4 + offset;
   (PC + 1)->ops();
   cached_interpreter_jump_to(target);
}

static void do_branch_likely_not_taken(void)
{
   PC += 2;
}

void cached_interp_BLTZ(void)
{
   r4300.pc = PC->addr;
   if (irs < 0)
      do_branch_taken(iimmediate << 2);
   else
      do_branch_not_taken();
   update_count();
   if (r4300.next_interrupt <= Count) gen_interrupt();
}

void cached_interp_BGEZ(void)
{
   r4300.pc = PC->addr;
   if (irs >= 0)
      do_branch_taken(iimmediate << 2);
   else
      do_branch_not_taken();
   update_count();
   if (r4300.next_interrupt <= Count) gen_interrupt();
}

void cached_interp_BLTZL(void)
{
   r4300.pc = PC->addr;
   if (irs < 0)
      do_branch_taken(iimmediate << 2);
   else
      do_branch_not_taken();
   update_count();
   if (r4300.next_interrupt <= Count) gen_interrupt();
}

void cached_interp_BGEZL(void)
{
   r4300.pc = PC->addr;
   if (irs >= 0)
      do_branch_taken(iimmediate << 2);
   else
      do_branch_not_taken();
   update_count();
   if (r4300.next_interrupt <= Count) gen_interrupt();
}

void cached_interp_BLTZAL(void)
{
   r4300.pc = PC->addr;
   r4300.gpr[31] = PC->addr + 8;
   if (irs < 0)
      do_branch_taken(iimmediate << 2);
   else
      do_branch_not_taken();
   update_count();
   if (r4300.next_interrupt <= Count) gen_interrupt();
}

void cached_interp_BGEZAL(void)
{
   r4300.pc = PC->addr;
   r4300.gpr[31] = PC->addr + 8;
   if (irs >= 0)
      do_branch_taken(iimmediate << 2);
   else
      do_branch_not_taken();
   update_count();
   if (r4300.next_interrupt <= Count) gen_interrupt();
}

void cached_interp_BLTZALL(void)
{
   r4300.pc = PC->addr;
   r4300.gpr[31] = PC->addr + 8;
   if (irs < 0)
      do_branch_likely_taken(iimmediate << 2);
   else
      do_branch_likely_not_taken();
   update_count();
   if (r4300.next_interrupt <= Count) gen_interrupt();
}

void cached_interp_BGEZALL(void)
{
   r4300.pc = PC->addr;
   r4300.gpr[31] = PC->addr + 8;
   if (irs >= 0)
      do_branch_likely_taken(iimmediate << 2);
   else
      do_branch_likely_not_taken();
   update_count();
   if (r4300.next_interrupt <= Count) gen_interrupt();
}

void cached_interp_TGEI(void)
{
   if (irs >= iimmediate)
   {
      printf("trap exception in tgei\n");
      r4300.stop = 1;
   }
   PC++;
}

void cached_interp_TGEIU(void)
{
   if ((u64)irs >= (u64)(s32)iimmediate)
   {
      printf("trap exception in tgeiu\n");
      r4300.stop = 1;
   }
   PC++;
}

void cached_interp_TLTI(void)
{
   if (irs < iimmediate)
   {
      printf("trap exception in tlti\n");
      r4300.stop = 1;
   }
   PC++;
}

void cached_interp_TLTIU(void)
{
   if ((u64)irs < (u64)(s32)iimmediate)
   {
      printf("trap exception in tltiu\n");
      r4300.stop = 1;
   }
   PC++;
}

void cached_interp_TEQI(void)
{
   if (irs == iimmediate)
   {
      printf("trap exception in teqi\n");
      r4300.stop = 1;
   }
   PC++;
}

void cached_interp_TNEI(void)
{
   if (irs != iimmediate)
   {
      printf("trap exception in tnei\n");
      r4300.stop = 1;
   }
   PC++;
}

void cached_interp_J(void)
{
   r4300.pc = PC->addr;
   u32 target = (PC->addr & 0xF0000000) | (jinst_index << 2);
   (PC + 1)->ops();
   cached_interpreter_jump_to(target);
   update_count();
   if (r4300.next_interrupt <= Count) gen_interrupt();
}

void cached_interp_JAL(void)
{
   r4300.pc = PC->addr;
   r4300.gpr[31] = PC->addr + 8;
   u32 target = (PC->addr & 0xF0000000) | (jinst_index << 2);
   (PC + 1)->ops();
   cached_interpreter_jump_to(target);
   update_count();
   if (r4300.next_interrupt <= Count) gen_interrupt();
}

void cached_interp_JR(void)
{
   r4300.pc = PC->addr;
   u32 addr = rrs32;
   if (addr & 3)
   {
      printf("misaligned jump target in JR: 0x%08x\n", addr);
      r4300.stop = 1;
      PC++;
      return;
   }
   (PC + 1)->ops();
   cached_interpreter_jump_to(addr);
   update_count();
   if (r4300.next_interrupt <= Count) gen_interrupt();
}

void cached_interp_JALR(void)
{
   r4300.pc = PC->addr;
   u32 addr = rrs32;
   if (addr & 3)
   {
      printf("misaligned jump target in JALR: 0x%08x\n", addr);
      r4300.stop = 1;
      PC++;
      return;
   }
   rrd = (s64)((PC + 1)->addr + 4);
   (PC + 1)->ops();
   cached_interpreter_jump_to(addr);
   update_count();
   if (r4300.next_interrupt <= Count) gen_interrupt();
}

void cached_interp_BEQ(void)
{
   r4300.pc = PC->addr;
   if (irs32 == irt32)
      do_branch_taken(iimmediate << 2);
   else
      do_branch_not_taken();
   update_count();
   if (r4300.next_interrupt <= Count) gen_interrupt();
}

void cached_interp_BNE(void)
{
   r4300.pc = PC->addr;
   if (irs32 != irt32)
      do_branch_taken(iimmediate << 2);
   else
      do_branch_not_taken();
   update_count();
   if (r4300.next_interrupt <= Count) gen_interrupt();
}

void cached_interp_BLEZ(void)
{
   r4300.pc = PC->addr;
   if (irs32 <= 0)
      do_branch_taken(iimmediate << 2);
   else
      do_branch_not_taken();
   update_count();
   if (r4300.next_interrupt <= Count) gen_interrupt();
}

void cached_interp_BGTZ(void)
{
   r4300.pc = PC->addr;
   if (irs32 > 0)
      do_branch_taken(iimmediate << 2);
   else
      do_branch_not_taken();
   update_count();
   if (r4300.next_interrupt <= Count) gen_interrupt();
}

void cached_interp_BEQL(void)
{
   r4300.pc = PC->addr;
   if (irs32 == irt32)
      do_branch_likely_taken(iimmediate << 2);
   else
      do_branch_likely_not_taken();
   update_count();
   if (r4300.next_interrupt <= Count) gen_interrupt();
}

void cached_interp_BNEL(void)
{
   r4300.pc = PC->addr;
   if (irs32 != irt32)
      do_branch_likely_taken(iimmediate << 2);
   else
      do_branch_likely_not_taken();
   update_count();
   if (r4300.next_interrupt <= Count) gen_interrupt();
}

void cached_interp_BLEZL(void)
{
   r4300.pc = PC->addr;
   if (irs32 <= 0)
      do_branch_likely_taken(iimmediate << 2);
   else
      do_branch_likely_not_taken();
   update_count();
   if (r4300.next_interrupt <= Count) gen_interrupt();
}

void cached_interp_BGTZL(void)
{
   r4300.pc = PC->addr;
   if (irs32 > 0)
      do_branch_likely_taken(iimmediate << 2);
   else
      do_branch_likely_not_taken();
   update_count();
   if (r4300.next_interrupt <= Count) gen_interrupt();
}

void cached_interp_ADDI(void)
{
   irt32 = irs32 + iimmediate;
   sign_extended(irt);
   PC++;
}

void cached_interp_ADDIU(void)
{
   irt32 = irs32 + iimmediate;
   sign_extended(irt);
   PC++;
}

void cached_interp_SLTI(void)
{
   if (irs < (s64)iimmediate)
      irt = 1;
   else
      irt = 0;
   PC++;
}

void cached_interp_SLTIU(void)
{
   if ((u64)irs < (u64)(s64)(s32)iimmediate)
      irt = 1;
   else
      irt = 0;
   PC++;
}

void cached_interp_ANDI(void)
{
   irt = irs & (unsigned short)iimmediate;
   PC++;
}

void cached_interp_ORI(void)
{
   irt = irs | (unsigned short)iimmediate;
   PC++;
}

void cached_interp_XORI(void)
{
   irt = irs ^ (unsigned short)iimmediate;
   PC++;
}

void cached_interp_LUI(void)
{
   irt = (s64)(s32)((unsigned short)iimmediate << 16);
   PC++;
}

void cached_interp_DADDI(void)
{
   irt = irs + iimmediate;
   PC++;
}

void cached_interp_DADDIU(void)
{
   irt = irs + iimmediate;
   PC++;
}

void cached_interp_LB(void)
{
   r4300.pc = PC->addr;
   address = iimmediate + irs32;
   rdword = (unsigned long long int*)PC->f.i.rt;
   read_byte_in_memory();
   sign_extendedb(*PC->f.i.rt);
   PC++;
}

void cached_interp_LH(void)
{
   r4300.pc = PC->addr;
   address = iimmediate + irs32;
   rdword = (unsigned long long int*)PC->f.i.rt;
   read_hword_in_memory();
   sign_extendedh(*PC->f.i.rt);
   PC++;
}

void cached_interp_LW(void)
{
   r4300.pc = PC->addr;
   address = iimmediate + irs32;
   rdword = (unsigned long long int*)PC->f.i.rt;
   read_word_in_memory();
   sign_extended(*PC->f.i.rt);
   PC++;
}

void cached_interp_LBU(void)
{
   r4300.pc = PC->addr;
   address = iimmediate + irs32;
   rdword = (unsigned long long int*)PC->f.i.rt;
   read_byte_in_memory();
   PC++;
}

void cached_interp_LHU(void)
{
   r4300.pc = PC->addr;
   address = iimmediate + irs32;
   rdword = (unsigned long long int*)PC->f.i.rt;
   read_hword_in_memory();
   PC++;
}

void cached_interp_LWU(void)
{
   r4300.pc = PC->addr;
   address = iimmediate + irs32;
   rdword = (unsigned long long int*)PC->f.i.rt;
   read_word_in_memory();
   PC++;
}

void cached_interp_SB(void)
{
   r4300.pc = PC->addr;
   address = iimmediate + irs32;
   byte = (unsigned char)(irt & 0xFF);
   write_byte_in_memory();
   PC++;
}

void cached_interp_SH(void)
{
   r4300.pc = PC->addr;
   address = iimmediate + irs32;
   hword = (unsigned short)(irt & 0xFFFF);
   write_hword_in_memory();
   PC++;
}

void cached_interp_SW(void)
{
   r4300.pc = PC->addr;
   address = iimmediate + irs32;
   word = (u32)(irt & 0xFFFFFFFF);
   write_word_in_memory();
   PC++;
}

void cached_interp_LWL(void)
{
   r4300.pc = PC->addr;
   u32 addr = iimmediate + irs32;
   unsigned long long int aligned_val = 0;
   address = addr & ~3;
   rdword = &aligned_val;
   read_word_in_memory();
   u32 val = (u32)(aligned_val & 0xFFFFFFFF);
   switch (addr & 3)
   {
      case 0: LOW_WORD(irt) = (LOW_WORD(irt) & 0x00FFFFFF) | (val << 24); break;
      case 1: LOW_WORD(irt) = (LOW_WORD(irt) & 0x0000FFFF) | (val << 16); break;
      case 2: LOW_WORD(irt) = (LOW_WORD(irt) & 0x000000FF) | (val << 8); break;
      case 3: LOW_WORD(irt) = val; break;
   }
   sign_extended(irt);
   PC++;
}

void cached_interp_LWR(void)
{
   r4300.pc = PC->addr;
   u32 addr = iimmediate + irs32;
   unsigned long long int aligned_val = 0;
   address = addr & ~3;
   rdword = &aligned_val;
   read_word_in_memory();
   u32 val = (u32)(aligned_val & 0xFFFFFFFF);
   switch (addr & 3)
   {
      case 0: LOW_WORD(irt) = val; break;
      case 1: LOW_WORD(irt) = (LOW_WORD(irt) & 0xFF000000) | (val >> 8); break;
      case 2: LOW_WORD(irt) = (LOW_WORD(irt) & 0xFFFF0000) | (val >> 16); break;
      case 3: LOW_WORD(irt) = (LOW_WORD(irt) & 0xFFFFFF00) | (val >> 24); break;
   }
   sign_extended(irt);
   PC++;
}

void cached_interp_SWL(void)
{
   r4300.pc = PC->addr;
   u32 addr = iimmediate + irs32;
   u32 val = (u32)(irt & 0xFFFFFFFF);
   unsigned long long int old_word = 0;
   address = addr & ~3;
   rdword = &old_word;
   read_word_in_memory();
   u32 mem = (u32)(old_word & 0xFFFFFFFF);
   switch (addr & 3)
   {
      case 0: val = (val >> 24) | (mem & 0xFFFFFF00); break;
      case 1: val = (val >> 16) | (mem & 0xFFFF0000); break;
      case 2: val = (val >> 8)  | (mem & 0xFF000000); break;
      case 3: break;
   }
   word = val;
   address = addr & ~3;
   write_word_in_memory();
   PC++;
}

void cached_interp_SWR(void)
{
   r4300.pc = PC->addr;
   u32 addr = iimmediate + irs32;
   u32 val = (u32)(irt & 0xFFFFFFFF);
   unsigned long long int old_word = 0;
   address = addr & ~3;
   rdword = &old_word;
   read_word_in_memory();
   u32 mem = (u32)(old_word & 0xFFFFFFFF);
   switch (addr & 3)
   {
      case 0: break;
      case 1: val = (val << 8)  | (mem & 0x000000FF); break;
      case 2: val = (val << 16) | (mem & 0x0000FFFF); break;
      case 3: val = (val << 24) | (mem & 0x00FFFFFF); break;
   }
   word = val;
   address = addr & ~3;
   write_word_in_memory();
   PC++;
}

void cached_interp_LDL(void)
{
   r4300.pc = PC->addr;
   u32 addr = iimmediate + irs32;
   unsigned long long int aligned_val = 0;
   address = addr & ~7;
   rdword = &aligned_val;
   read_dword_in_memory();
   u64 val = aligned_val;
   switch (addr & 7)
   {
      case 0: irt = (irt & 0x00FFFFFFFFFFFFFFULL) | (val << 56); break;
      case 1: irt = (irt & 0x0000FFFFFFFFFFFFULL) | (val << 48); break;
      case 2: irt = (irt & 0x000000FFFFFFFFFFULL) | (val << 40); break;
      case 3: irt = (irt & 0x00000000FFFFFFFFULL) | (val << 32); break;
      case 4: irt = (irt & 0x0000000000FFFFFFULL) | (val << 24); break;
      case 5: irt = (irt & 0x000000000000FFFFULL) | (val << 16); break;
      case 6: irt = (irt & 0x00000000000000FFULL) | (val << 8); break;
      case 7: irt = val; break;
   }
   PC++;
}

void cached_interp_LDR(void)
{
   r4300.pc = PC->addr;
   u32 addr = iimmediate + irs32;
   unsigned long long int aligned_val = 0;
   address = addr & ~7;
   rdword = &aligned_val;
   read_dword_in_memory();
   u64 val = aligned_val;
   switch (addr & 7)
   {
      case 0: irt = val; break;
      case 1: irt = (irt & 0xFF00000000000000ULL) | (val >> 8); break;
      case 2: irt = (irt & 0xFFFF000000000000ULL) | (val >> 16); break;
      case 3: irt = (irt & 0xFFFFFF0000000000ULL) | (val >> 24); break;
      case 4: irt = (irt & 0xFFFFFFFF00000000ULL) | (val >> 32); break;
      case 5: irt = (irt & 0xFFFFFFFFFF000000ULL) | (val >> 40); break;
      case 6: irt = (irt & 0xFFFFFFFFFFFF0000ULL) | (val >> 48); break;
      case 7: irt = (irt & 0xFFFFFFFFFFFFFF00ULL) | (val >> 56); break;
   }
   PC++;
}

void cached_interp_SDL(void)
{
   r4300.pc = PC->addr;
   u32 addr = iimmediate + irs32;
   unsigned long long int val = irt;
   unsigned long long int old_word = 0;
   u32 aligned = addr & ~7;
   address = aligned;
   rdword = &old_word;
   read_dword_in_memory();
   switch (addr & 7)
   {
      case 0: val = (val >> 56) | (old_word & 0x00FFFFFFFFFFFFFFULL); break;
      case 1: val = (val >> 48) | (old_word & 0x0000FFFFFFFFFFFFULL); break;
      case 2: val = (val >> 40) | (old_word & 0x000000FFFFFFFFFFULL); break;
      case 3: val = (val >> 32) | (old_word & 0x00000000FFFFFFFFULL); break;
      case 4: val = (val >> 24) | (old_word & 0x0000000000FFFFFFULL); break;
      case 5: val = (val >> 16) | (old_word & 0x000000000000FFFFULL); break;
      case 6: val = (val >> 8)  | (old_word & 0x00000000000000FFULL); break;
      case 7: break;
   }
   dword = val;
   address = aligned;
   write_dword_in_memory();
   PC++;
}

void cached_interp_SDR(void)
{
   r4300.pc = PC->addr;
   u32 addr = iimmediate + irs32;
   unsigned long long int val = irt;
   unsigned long long int old_word = 0;
   u32 aligned = addr & ~7;
   address = aligned;
   rdword = &old_word;
   read_dword_in_memory();
   switch (addr & 7)
   {
      case 0: break;
      case 1: val = (val << 8)  | (old_word & 0x00000000000000FFULL); break;
      case 2: val = (val << 16) | (old_word & 0x000000000000FFFFULL); break;
      case 3: val = (val << 24) | (old_word & 0x0000000000FFFFFFULL); break;
      case 4: val = (val << 32) | (old_word & 0x00000000FFFFFFFFULL); break;
      case 5: val = (val << 40) | (old_word & 0x000000FFFFFFFFFFULL); break;
      case 6: val = (val << 48) | (old_word & 0x0000FFFFFFFFFFFFULL); break;
      case 7: val = (val << 56) | (old_word & 0x00FFFFFFFFFFFFFFULL); break;
   }
   dword = val;
   address = aligned;
   write_dword_in_memory();
   PC++;
}

void cached_interp_LWC1(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   r4300.pc = PC->addr;
   address = lfoffset + r4300.gpr[lfbase];
   unsigned long long int temp;
   rdword = &temp;
   read_word_in_memory();
   *((s32*)r4300.fpr_single[lfft]) = (s32)(temp & 0xFFFFFFFF);
   PC++;
}

void cached_interp_SWC1(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   r4300.pc = PC->addr;
   address = lfoffset + r4300.gpr[lfbase];
   word = *((u32*)r4300.fpr_single[lfft]);
   write_word_in_memory();
   PC++;
}

void cached_interp_LDC1(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   r4300.pc = PC->addr;
   address = lfoffset + r4300.gpr[lfbase];
   rdword = (unsigned long long int*)r4300.fpr_double[lfft];
   read_dword_in_memory();
   PC++;
}

void cached_interp_SDC1(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   r4300.pc = PC->addr;
   address = lfoffset + r4300.gpr[lfbase];
   dword = *((u64*)r4300.fpr_double[lfft]);
   write_dword_in_memory();
   PC++;
}

void cached_interp_LQ(void)
{
   printf("LQ not implemented\n");
   r4300.stop = 1;
   PC++;
}

void cached_interp_SQ(void)
{
   printf("SQ not implemented\n");
   r4300.stop = 1;
   PC++;
}

void cached_interp_LL(void)
{
   r4300.pc = PC->addr;
   address = iimmediate + irs32;
   rdword = (unsigned long long int*)PC->f.i.rt;
   read_word_in_memory();
   sign_extended(*PC->f.i.rt);
   r4300.llbit = 1;
   PC++;
}

void cached_interp_SC(void)
{
   r4300.pc = PC->addr;
   if (r4300.llbit)
   {
      address = iimmediate + irs32;
      word = (u32)(irt & 0xFFFFFFFF);
      write_word_in_memory();
      r4300.llbit = 0;
      irt = 1;
   }
   else
      irt = 0;
   PC++;
}

void cached_interp_LLD(void)
{
   printf("LLD not implemented\n");
   r4300.stop = 1;
   PC++;
}

void cached_interp_SCD(void)
{
   printf("SCD not implemented\n");
   r4300.stop = 1;
   PC++;
}

void cached_interp_LD(void)
{
   r4300.pc = PC->addr;
   address = iimmediate + irs32;
   rdword = (unsigned long long int*)PC->f.i.rt;
   read_dword_in_memory();
   PC++;
}

void cached_interp_SD(void)
{
   r4300.pc = PC->addr;
   address = iimmediate + irs32;
   dword = irt;
   write_dword_in_memory();
   PC++;
}

void cached_interp_PREF(void)
{
   PC++;
}

void cached_interp_CACHE(void)
{
   PC++;
}

void cached_interp_ERET(void)
{
   update_count();
   if (Status & 0x4)
   {
      printf("error in ERET\n");
      r4300.stop = 1;
      PC++;
      return;
   }
   Status &= 0xFFFFFFFD;
   r4300.pc = EPC;
   r4300.llbit = 0;
   check_interrupt();
   if (r4300.next_interrupt <= Count) gen_interrupt();
   cached_interpreter_jump_to(r4300.pc);
}

void cached_interp_MFC0(void)
{
   u32 reg = PC->f.r.nrd;
   if (reg == 1)
   {
      printf("reading Random\n");
      r4300.stop = 1;
      PC++;
      return;
   }
   irt32 = r4300.reg_cop0[reg];
   sign_extended(irt);
   PC++;
}

void cached_interp_MTC0(void)
{
   u32 val = irt32 & 0xFFFFFFFF;
   u32 reg = PC->f.r.nrd;
   switch (reg)
   {
      case 0:
         Index = val & 0x8000003F;
         if ((Index & 0x3F) > 31)
         {
            printf("more than 32 TLB entries\n");
            r4300.stop = 1;
         }
         break;
      case 1:
         break;
      case 2:
         EntryLo0 = val & 0x3FFFFFFF;
         break;
      case 3:
         EntryLo1 = val & 0x3FFFFFFF;
         break;
      case 4:
         Context = (val & 0xFF800000) | (Context & 0x007FFFF0);
         break;
      case 5:
         PageMask = val & 0x01FFE000;
         break;
      case 6:
         Wired = val;
         Random = 31;
         break;
      case 8:
         break;
      case 9:
         update_count();
         if (r4300.next_interrupt <= Count) gen_interrupt();
         translate_event_queue(val & 0xFFFFFFFF);
         Count = val & 0xFFFFFFFF;
         break;
      case 10:
         EntryHi = val & 0xFFFFE0FF;
         break;
      case 11:
         update_count();
         remove_event(COMPARE_INT);
         add_interrupt_event_count(COMPARE_INT, val);
         Compare = val;
         Cause &= 0xFFFF7FFF;
         break;
      case 12:
         if ((val & 0x04000000) != (Status & 0x04000000))
         {
            shuffle_fpr_data(Status, val);
            set_fpr_pointers(val);
         }
         Status = val;
         check_interrupt();
         update_count();
         if (r4300.next_interrupt <= Count) gen_interrupt();
         break;
      case 14:
         EPC = val;
         break;
      case 16:
         Config = val;
         break;
      case 18:
         LLAddr = val;
         break;
      default:
         r4300.reg_cop0[reg] = val;
         break;
   }
   PC++;
}

void cached_interp_TLBR(void)
{
   int idx = Index & 0x1F;
   PageMask = tlb_e[idx].mask << 13;
   EntryHi = (tlb_e[idx].vpn2 << 13) | tlb_e[idx].asid;
   EntryLo0 = (tlb_e[idx].pfn_even << 6) | (tlb_e[idx].c_even << 3)
      | (tlb_e[idx].d_even << 2) | (tlb_e[idx].v_even << 1) | tlb_e[idx].g;
   EntryLo1 = (tlb_e[idx].pfn_odd << 6) | (tlb_e[idx].c_odd << 3)
      | (tlb_e[idx].d_odd << 2) | (tlb_e[idx].v_odd << 1) | tlb_e[idx].g;
   PC++;
}

void cached_interp_TLBWI(void)
{
   int idx = Index & 0x3F;
   tlb_unmap(idx);
   tlb_e[idx].g = (EntryLo0 & EntryLo1 & 1);
   tlb_e[idx].pfn_even = (EntryLo0 & 0x3FFFFFC0) >> 6;
   tlb_e[idx].pfn_odd = (EntryLo1 & 0x3FFFFFC0) >> 6;
   tlb_e[idx].c_even = (EntryLo0 & 0x38) >> 3;
   tlb_e[idx].c_odd = (EntryLo1 & 0x38) >> 3;
   tlb_e[idx].d_even = (EntryLo0 & 0x4) >> 2;
   tlb_e[idx].d_odd = (EntryLo1 & 0x4) >> 2;
   tlb_e[idx].v_even = (EntryLo0 & 0x2) >> 1;
   tlb_e[idx].v_odd = (EntryLo1 & 0x2) >> 1;
   tlb_e[idx].asid = (EntryHi & 0xFF);
   tlb_e[idx].vpn2 = (EntryHi & 0xFFFFE000) >> 13;
   tlb_e[idx].mask = (PageMask & 0x1FFE000) >> 13;
   tlb_e[idx].start_even = tlb_e[idx].vpn2 << 13;
   tlb_e[idx].end_even = tlb_e[idx].start_even + (tlb_e[idx].mask << 12) + 0xFFF;
   tlb_e[idx].phys_even = tlb_e[idx].pfn_even << 12;
   tlb_e[idx].start_odd = tlb_e[idx].end_even + 1;
   tlb_e[idx].end_odd = tlb_e[idx].start_odd + (tlb_e[idx].mask << 12) + 0xFFF;
   tlb_e[idx].phys_odd = tlb_e[idx].pfn_odd << 12;
   tlb_map(idx);
   invalidate_cached_code(tlb_e[idx].start_even, (tlb_e[idx].mask << 12) + 0x1000);
   invalidate_cached_code(tlb_e[idx].start_odd, (tlb_e[idx].mask << 12) + 0x1000);
   PC++;
}

void cached_interp_TLBWR(void)
{
   update_count();
   Random = (Count / 2 % (32 - Wired)) + Wired;
   int idx = Random;
   tlb_unmap(idx);
   tlb_e[idx].g = (EntryLo0 & EntryLo1 & 1);
   tlb_e[idx].pfn_even = (EntryLo0 & 0x3FFFFFC0) >> 6;
   tlb_e[idx].pfn_odd = (EntryLo1 & 0x3FFFFFC0) >> 6;
   tlb_e[idx].c_even = (EntryLo0 & 0x38) >> 3;
   tlb_e[idx].c_odd = (EntryLo1 & 0x38) >> 3;
   tlb_e[idx].d_even = (EntryLo0 & 0x4) >> 2;
   tlb_e[idx].d_odd = (EntryLo1 & 0x4) >> 2;
   tlb_e[idx].v_even = (EntryLo0 & 0x2) >> 1;
   tlb_e[idx].v_odd = (EntryLo1 & 0x2) >> 1;
   tlb_e[idx].asid = (EntryHi & 0xFF);
   tlb_e[idx].vpn2 = (EntryHi & 0xFFFFE000) >> 13;
   tlb_e[idx].mask = (PageMask & 0x1FFE000) >> 13;
   tlb_e[idx].start_even = tlb_e[idx].vpn2 << 13;
   tlb_e[idx].end_even = tlb_e[idx].start_even + (tlb_e[idx].mask << 12) + 0xFFF;
   tlb_e[idx].phys_even = tlb_e[idx].pfn_even << 12;
   tlb_e[idx].start_odd = tlb_e[idx].end_even + 1;
   tlb_e[idx].end_odd = tlb_e[idx].start_odd + (tlb_e[idx].mask << 12) + 0xFFF;
   tlb_e[idx].phys_odd = tlb_e[idx].pfn_odd << 12;
   tlb_map(idx);
   invalidate_cached_code(tlb_e[idx].start_even, (tlb_e[idx].mask << 12) + 0x1000);
   invalidate_cached_code(tlb_e[idx].start_odd, (tlb_e[idx].mask << 12) + 0x1000);
   PC++;
}

void cached_interp_TLBP(void)
{
   int i;
   Index |= 0x80000000;
   for (i = 0; i < 32; i++)
   {
      if (((tlb_e[i].vpn2 & (~tlb_e[i].mask)) ==
           (((EntryHi & 0xFFFFE000) >> 13) & (~tlb_e[i].mask))) &&
          ((tlb_e[i].g) || (tlb_e[i].asid == (EntryHi & 0xFF))))
      {
         Index = i;
         break;
      }
   }
   PC++;
}

void cached_interp_LWC2(void)
{
   printf("LWC2 not implemented\n");
   PC++;
}

void cached_interp_SWC2(void)
{
   printf("SWC2 not implemented\n");
   PC++;
}

void cached_interp_LDC2(void)
{
   printf("LDC2 not implemented\n");
   PC++;
}

void cached_interp_SDC2(void)
{
   printf("SDC2 not implemented\n");
   PC++;
}

static void cached_fpu_op_s(u32 funct)
{
   u32 ft = PC->f.cf.ft;
   u32 fs = PC->f.cf.fs;
   u32 fd = PC->f.cf.fd;

   switch (funct)
   {
      case 0x00: *r4300.fpr_single[fd] = *r4300.fpr_single[fs] + *r4300.fpr_single[ft]; break;
      case 0x01: *r4300.fpr_single[fd] = *r4300.fpr_single[fs] - *r4300.fpr_single[ft]; break;
      case 0x02: *r4300.fpr_single[fd] = *r4300.fpr_single[fs] * *r4300.fpr_single[ft]; break;
      case 0x03: *r4300.fpr_single[fd] = *r4300.fpr_single[fs] / *r4300.fpr_single[ft]; break;
      case 0x04: *r4300.fpr_single[fd] = (float)sqrt(*r4300.fpr_single[fs]); break;
      case 0x05: *r4300.fpr_single[fd] = (float)fabs(*r4300.fpr_single[fs]); break;
      case 0x06: *r4300.fpr_single[fd] = *r4300.fpr_single[fs]; break;
      case 0x07: *r4300.fpr_single[fd] = -*r4300.fpr_single[fs]; break;
      case 0x21:
         *r4300.fpr_double[fd] = (double)*r4300.fpr_single[fs];
         break;
      case 0x24:
         set_trunc();
         *r4300.fpr_single[fd] = (float)(s32)*r4300.fpr_double[fs];
         set_rounding();
         break;
      case 0x25:
         set_trunc();
         *r4300.fpr_single[fd] = (float)(s64)*r4300.fpr_double[fs];
         set_rounding();
         break;
      case 0x28:
         set_ceil();
         *r4300.fpr_single[fd] = (float)(s32)ceil(*r4300.fpr_double[fs]);
         set_rounding();
         break;
      case 0x29:
         set_ceil();
         *r4300.fpr_single[fd] = (float)(s64)ceil(*r4300.fpr_double[fs]);
         set_rounding();
         break;
      case 0x30:
         set_trunc();
         *r4300.fpr_single[fd] = (float)(s32)(*r4300.fpr_double[fs]);
         set_rounding();
         break;
      case 0x31:
         set_trunc();
         *r4300.fpr_single[fd] = (float)(s64)(*r4300.fpr_double[fs]);
         set_rounding();
         break;
      case 0x22:
         set_round();
         *r4300.fpr_single[fd] = (float)(s32)(*r4300.fpr_double[fs] + 0.5);
         set_rounding();
         break;
      case 0x34:
         set_floor();
         *r4300.fpr_single[fd] = (float)(s32)floor(*r4300.fpr_double[fs]);
         set_rounding();
         break;
      case 0x35:
         set_floor();
         *r4300.fpr_single[fd] = (float)(s64)floor(*r4300.fpr_double[fs]);
         set_rounding();
         break;
      case 0x20:
         *r4300.fpr_single[fd] = (float)*r4300.fpr_double[fs];
         break;
      default: printf("unimplemented FPU S op funct=0x%02x\n", funct); r4300.stop = 1; break;
   }
}

static void cached_fpu_op_d(u32 funct)
{
   u32 ft = PC->f.cf.ft;
   u32 fs = PC->f.cf.fs;
   u32 fd = PC->f.cf.fd;

   switch (funct)
   {
      case 0x00: *r4300.fpr_double[fd] = *r4300.fpr_double[fs] + *r4300.fpr_double[ft]; break;
      case 0x01: *r4300.fpr_double[fd] = *r4300.fpr_double[fs] - *r4300.fpr_double[ft]; break;
      case 0x02: *r4300.fpr_double[fd] = *r4300.fpr_double[fs] * *r4300.fpr_double[ft]; break;
      case 0x03: *r4300.fpr_double[fd] = *r4300.fpr_double[fs] / *r4300.fpr_double[ft]; break;
      case 0x04: *r4300.fpr_double[fd] = sqrt(*r4300.fpr_double[fs]); break;
      case 0x05: *r4300.fpr_double[fd] = fabs(*r4300.fpr_double[fs]); break;
      case 0x06: *r4300.fpr_double[fd] = *r4300.fpr_double[fs]; break;
      case 0x07: *r4300.fpr_double[fd] = -*r4300.fpr_double[fs]; break;
      case 0x21:
         *r4300.fpr_double[fd] = (double)(s32)r4300.fpr_data[fs];
         break;
      case 0x24:
         set_trunc();
         *r4300.fpr_double[fd] = (double)(s32)*r4300.fpr_double[fs];
         set_rounding();
         break;
      case 0x25:
         set_trunc();
         *r4300.fpr_double[fd] = (double)(s64)*r4300.fpr_double[fs];
         set_rounding();
         break;
      case 0x30:
         set_trunc();
         *r4300.fpr_double[fd] = (double)(s32)(*r4300.fpr_double[fs]);
         set_rounding();
         break;
      case 0x31:
         set_trunc();
         *r4300.fpr_double[fd] = (double)(s64)(*r4300.fpr_double[fs]);
         set_rounding();
         break;
      case 0x20:
         *r4300.fpr_double[fd] = (double)*r4300.fpr_single[fs];
         break;
      default: printf("unimplemented FPU D op funct=0x%02x\n", funct); r4300.stop = 1; break;
   }
}

void cached_interp_MFC1(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   u32 reg = PC->f.r.nrd;
   LOW_WORD(irt) = *((s32*)r4300.fpr_single[reg]);
   sign_extended(irt);
   PC++;
}

void cached_interp_DMFC1(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   u32 reg = PC->f.r.nrd;
   irt = *((s64*)r4300.fpr_double[reg]);
   PC++;
}

void cached_interp_CFC1(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   u32 reg = PC->f.r.nrd;
   if (reg == 31)
      irt32 = (s32)r4300.fcr31;
   else
      irt32 = r4300.fcr0;
   sign_extended(irt);
   PC++;
}

void cached_interp_MTC1(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   u32 reg = PC->f.r.nrd;
   *((s32*)r4300.fpr_single[reg]) = irt32;
   PC++;
}

void cached_interp_DMTC1(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   u32 reg = PC->f.r.nrd;
   *((s64*)r4300.fpr_double[reg]) = irt;
   PC++;
}

void cached_interp_CTC1(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   u32 reg = PC->f.r.nrd;
   if (reg == 31)
      r4300.fcr31 = irt32;
   PC++;
}

void cached_interp_BC1F(void)
{
   r4300.pc = PC->addr;
   if (!(r4300.fcr31 & 0x00800000))
      do_branch_taken(iimmediate << 2);
   else
      do_branch_not_taken();
   update_count();
   if (r4300.next_interrupt <= Count) gen_interrupt();
}

void cached_interp_BC1T(void)
{
   r4300.pc = PC->addr;
   if (r4300.fcr31 & 0x00800000)
      do_branch_taken(iimmediate << 2);
   else
      do_branch_not_taken();
   update_count();
   if (r4300.next_interrupt <= Count) gen_interrupt();
}

void cached_interp_BC1FL(void)
{
   r4300.pc = PC->addr;
   if (!(r4300.fcr31 & 0x00800000))
      do_branch_likely_taken(iimmediate << 2);
   else
      do_branch_likely_not_taken();
   update_count();
   if (r4300.next_interrupt <= Count) gen_interrupt();
}

void cached_interp_BC1TL(void)
{
   r4300.pc = PC->addr;
   if (r4300.fcr31 & 0x00800000)
      do_branch_likely_taken(iimmediate << 2);
   else
      do_branch_likely_not_taken();
   update_count();
   if (r4300.next_interrupt <= Count) gen_interrupt();
}

void cached_interp_ADD_S(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_s(0x00); PC++;
}

void cached_interp_SUB_S(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_s(0x01); PC++;
}

void cached_interp_MUL_S(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_s(0x02); PC++;
}

void cached_interp_DIV_S(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_s(0x03); PC++;
}

void cached_interp_SQRT_S(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_s(0x04); PC++;
}

void cached_interp_ABS_S(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_s(0x05); PC++;
}

void cached_interp_MOV_S(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_s(0x06); PC++;
}

void cached_interp_NEG_S(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_s(0x07); PC++;
}

void cached_interp_ADD_D(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_d(0x00); PC++;
}

void cached_interp_SUB_D(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_d(0x01); PC++;
}

void cached_interp_MUL_D(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_d(0x02); PC++;
}

void cached_interp_DIV_D(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_d(0x03); PC++;
}

void cached_interp_SQRT_D(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_d(0x04); PC++;
}

void cached_interp_ABS_D(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_d(0x05); PC++;
}

void cached_interp_MOV_D(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_d(0x06); PC++;
}

void cached_interp_NEG_D(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_d(0x07); PC++;
}

void cached_interp_CVT_D_S(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_s(0x21); PC++;
}

void cached_interp_CVT_D_W(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_d(0x21); PC++;
}

void cached_interp_CVT_W_S(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_s(0x24); PC++;
}

void cached_interp_CVT_W_D(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_d(0x24); PC++;
}

void cached_interp_CVT_L_S(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_s(0x25); PC++;
}

void cached_interp_CVT_L_D(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_d(0x25); PC++;
}

void cached_interp_CVT_S_D(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_d(0x20); PC++;
}

void cached_interp_CVT_S_W(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_s(0x20); PC++;
}

void cached_interp_CVT_S_L(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   r4300.stop = 1; PC++; printf("CVT_S_L not implemented\n");
}

void cached_interp_TRUNC_W_S(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_s(0x30); PC++;
}

void cached_interp_TRUNC_W_D(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_d(0x30); PC++;
}

void cached_interp_TRUNC_L_S(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_s(0x31); PC++;
}

void cached_interp_TRUNC_L_D(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_d(0x31); PC++;
}

void cached_interp_CEIL_W_S(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_s(0x28); PC++;
}

void cached_interp_CEIL_W_D(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_d(0x28); PC++;
}

void cached_interp_CEIL_L_S(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_s(0x29); PC++;
}

void cached_interp_CEIL_L_D(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_d(0x29); PC++;
}

void cached_interp_FLOOR_W_S(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_s(0x34); PC++;
}

void cached_interp_FLOOR_W_D(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_d(0x34); PC++;
}

void cached_interp_FLOOR_L_S(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_s(0x35); PC++;
}

void cached_interp_FLOOR_L_D(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_d(0x35); PC++;
}

void cached_interp_ROUND_W_S(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_s(0x22); PC++;
}

void cached_interp_ROUND_W_D(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_d(0x22); PC++;
}

void cached_interp_ROUND_L_S(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_s(0x23); PC++;
}

void cached_interp_ROUND_L_D(void)
{
   if (check_cop1_unusable()) { PC++; return; }
   cached_fpu_op_d(0x23); PC++;
}

void cached_interp_CMP_COND_S(u32 cond)
{
   if (check_cop1_unusable()) { PC++; return; }
   float a = *r4300.fpr_single[PC->f.cf.fs];
   float b = *r4300.fpr_single[PC->f.cf.ft];
   int c = 0;
   switch (cond)
   {
      case 0: c = 0; break;
      case 1: c = (a != a || b != b || a != b); break;
      case 2: c = (a == b); break;
      case 3: c = (a != a || b != b || a == b); break;
      case 4: c = (a < b); break;
      case 5: c = (a != a || b != b || a < b); break;
      case 6: c = (a <= b); break;
      case 7: c = (a != a || b != b || a <= b); break;
   }
   if (c)
      r4300.fcr31 |= 0x00800000;
   else
      r4300.fcr31 &= ~0x00800000;
   PC++;
}

void cached_interp_CMP_COND_D(u32 cond)
{
   if (check_cop1_unusable()) { PC++; return; }
   double a = *r4300.fpr_double[PC->f.cf.fs];
   double b = *r4300.fpr_double[PC->f.cf.ft];
   int c = 0;
   switch (cond)
   {
      case 0: c = 0; break;
      case 1: c = (a != a || b != b || a != b); break;
      case 2: c = (a == b); break;
      case 3: c = (a != a || b != b || a == b); break;
      case 4: c = (a < b); break;
      case 5: c = (a != a || b != b || a < b); break;
      case 6: c = (a <= b); break;
      case 7: c = (a != a || b != b || a <= b); break;
   }
   if (c)
      r4300.fcr31 |= 0x00800000;
   else
      r4300.fcr31 &= ~0x00800000;
   PC++;
}

void cached_interp_C_F_S(void) { cached_interp_CMP_COND_S(0); }
void cached_interp_C_UN_S(void) { cached_interp_CMP_COND_S(1); }
void cached_interp_C_EQ_S(void) { cached_interp_CMP_COND_S(2); }
void cached_interp_C_UEQ_S(void) { cached_interp_CMP_COND_S(3); }
void cached_interp_C_OLT_S(void) { cached_interp_CMP_COND_S(4); }
void cached_interp_C_ULT_S(void) { cached_interp_CMP_COND_S(5); }
void cached_interp_C_OLE_S(void) { cached_interp_CMP_COND_S(6); }
void cached_interp_C_ULE_S(void) { cached_interp_CMP_COND_S(7); }

void cached_interp_C_F_D(void) { cached_interp_CMP_COND_D(0); }
void cached_interp_C_UN_D(void) { cached_interp_CMP_COND_D(1); }
void cached_interp_C_EQ_D(void) { cached_interp_CMP_COND_D(2); }
void cached_interp_C_UEQ_D(void) { cached_interp_CMP_COND_D(3); }
void cached_interp_C_OLT_D(void) { cached_interp_CMP_COND_D(4); }
void cached_interp_C_ULT_D(void) { cached_interp_CMP_COND_D(5); }
void cached_interp_C_OLE_D(void) { cached_interp_CMP_COND_D(6); }
void cached_interp_C_ULE_D(void) { cached_interp_CMP_COND_D(7); }
