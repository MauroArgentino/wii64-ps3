#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ppu-types.h>
#include "r4300.h"
#include "wii64_cached_interp.h"
#include "exception.h"
#include "../n64_memory/memory.h"
#include "macros.h"
#include "interrupt.h"
#include "../../debug.h"

extern u32 op;
extern tlb tlb_e[32];
extern u32 *tlb_LUT_r;

extern int dbg_vi_count;
extern void dbg_dump_queue(void);
#ifdef PS3
extern int ps3_pad_exit_combo_pressed(void);
extern void controller_PS3_poll_pad(void);
#endif

#define RDRAM_WORDS 0x100000

/* Define CACHED_DEBUG to re-enable the per-instruction debug hooks in run_cached_interpreter(). */
/* #define CACHED_DEBUG */

// op function declarations (defined in wii64_cached_ops.c)
void cached_interp_SLL(void); void cached_interp_SRL(void); void cached_interp_SRA(void);
void cached_interp_SLLV(void); void cached_interp_SRLV(void); void cached_interp_SRAV(void);
void cached_interp_MFHI(void); void cached_interp_MTHI(void);
void cached_interp_MFLO(void); void cached_interp_MTLO(void);
void cached_interp_DSLLV(void); void cached_interp_DSRLV(void); void cached_interp_DSRAV(void);
void cached_interp_MULT(void); void cached_interp_MULTU(void);
void cached_interp_DIV(void); void cached_interp_DIVU(void);
void cached_interp_DMULT(void); void cached_interp_DMULTU(void);
void cached_interp_DDIV(void); void cached_interp_DDIVU(void);
void cached_interp_ADD(void); void cached_interp_ADDU(void);
void cached_interp_SUB(void); void cached_interp_SUBU(void);
void cached_interp_AND(void); void cached_interp_OR(void);
void cached_interp_XOR(void); void cached_interp_NOR(void);
void cached_interp_SLT(void); void cached_interp_SLTU(void);
void cached_interp_DADD(void); void cached_interp_DADDU(void);
void cached_interp_DSUB(void); void cached_interp_DSUBU(void);
void cached_interp_TEQ(void); void cached_interp_TGE(void); void cached_interp_TGEU(void);
void cached_interp_TLT(void); void cached_interp_TLTU(void); void cached_interp_TNE(void);
void cached_interp_DSLL(void); void cached_interp_DSRL(void); void cached_interp_DSRA(void);
void cached_interp_DSLL32(void); void cached_interp_DSRL32(void); void cached_interp_DSRA32(void);
void cached_interp_JR(void); void cached_interp_JALR(void);
void cached_interp_SYSCALL(void); void cached_interp_BREAK(void); void cached_interp_SYNC(void);
void cached_interp_BLTZ(void); void cached_interp_BGEZ(void);
void cached_interp_BLTZL(void); void cached_interp_BGEZL(void);
void cached_interp_BLTZAL(void); void cached_interp_BGEZAL(void);
void cached_interp_BLTZALL(void); void cached_interp_BGEZALL(void);
void cached_interp_TGEI(void); void cached_interp_TGEIU(void);
void cached_interp_TLTI(void); void cached_interp_TLTIU(void);
void cached_interp_TEQI(void); void cached_interp_TNEI(void);
void cached_interp_J(void); void cached_interp_JAL(void);
void cached_interp_BEQ(void); void cached_interp_BNE(void);
void cached_interp_BLEZ(void); void cached_interp_BGTZ(void);
void cached_interp_ADDI(void); void cached_interp_ADDIU(void);
void cached_interp_SLTI(void); void cached_interp_SLTIU(void);
void cached_interp_ANDI(void); void cached_interp_ORI(void);
void cached_interp_XORI(void); void cached_interp_LUI(void);
void cached_interp_DADDI(void); void cached_interp_DADDIU(void);
void cached_interp_BEQL(void); void cached_interp_BNEL(void);
void cached_interp_BLEZL(void); void cached_interp_BGTZL(void);
void cached_interp_LDL(void); void cached_interp_LDR(void);
void cached_interp_LQ(void); void cached_interp_SQ(void);
void cached_interp_LB(void); void cached_interp_LH(void);
void cached_interp_LWL(void); void cached_interp_LW(void);
void cached_interp_LBU(void); void cached_interp_LHU(void);
void cached_interp_LWR(void); void cached_interp_LWU(void);
void cached_interp_SB(void); void cached_interp_SH(void);
void cached_interp_SWL(void); void cached_interp_SW(void);
void cached_interp_SDL(void); void cached_interp_SDR(void);
void cached_interp_SWR(void); void cached_interp_CACHE(void);
void cached_interp_LL(void); void cached_interp_LWC1(void);
void cached_interp_LWC2(void); void cached_interp_PREF(void);
void cached_interp_LLD(void); void cached_interp_LDC1(void);
void cached_interp_LDC2(void); void cached_interp_LD(void);
void cached_interp_SC(void); void cached_interp_SWC1(void);
void cached_interp_SWC2(void); void cached_interp_SCD(void);
void cached_interp_SDC1(void); void cached_interp_SDC2(void); void cached_interp_SD(void);
void cached_interp_NI(void); void cached_interp_NOP(void);
void cached_interp_FIN_BLOCK(void); void cached_interp_NOTCOMPILED(void);
void cached_interp_NOTCOMPILED2(void);
void cached_interp_ERET(void);
void cached_interp_TLB_REFILL(void);
void cached_interp_MFC0(void); void cached_interp_MTC0(void);
void cached_interp_TLBR(void); void cached_interp_TLBWI(void);
void cached_interp_TLBWR(void); void cached_interp_TLBP(void);
void cached_interp_MFC1(void); void cached_interp_DMFC1(void); void cached_interp_CFC1(void);
void cached_interp_MTC1(void); void cached_interp_DMTC1(void); void cached_interp_CTC1(void);
void cached_interp_BC1F(void); void cached_interp_BC1T(void);
void cached_interp_BC1FL(void); void cached_interp_BC1TL(void);
void cached_interp_ADD_S(void); void cached_interp_SUB_S(void);
void cached_interp_MUL_S(void); void cached_interp_DIV_S(void);
void cached_interp_SQRT_S(void); void cached_interp_ABS_S(void);
void cached_interp_MOV_S(void); void cached_interp_NEG_S(void);
void cached_interp_ADD_D(void); void cached_interp_SUB_D(void);
void cached_interp_MUL_D(void); void cached_interp_DIV_D(void);
void cached_interp_SQRT_D(void); void cached_interp_ABS_D(void);
void cached_interp_MOV_D(void); void cached_interp_NEG_D(void);
void cached_interp_CVT_D_S(void); void cached_interp_CVT_D_W(void); void cached_interp_CVT_D_L(void);
void cached_interp_CVT_W_S(void); void cached_interp_CVT_W_D(void);
void cached_interp_CVT_L_S(void); void cached_interp_CVT_L_D(void);
void cached_interp_CVT_S_D(void); void cached_interp_CVT_S_W(void); void cached_interp_CVT_S_L(void);
void cached_interp_TRUNC_W_S(void); void cached_interp_TRUNC_W_D(void);
void cached_interp_TRUNC_L_S(void); void cached_interp_TRUNC_L_D(void);
void cached_interp_CEIL_W_S(void); void cached_interp_CEIL_W_D(void);
void cached_interp_CEIL_L_S(void); void cached_interp_CEIL_L_D(void);
void cached_interp_FLOOR_W_S(void); void cached_interp_FLOOR_W_D(void);
void cached_interp_FLOOR_L_S(void); void cached_interp_FLOOR_L_D(void);
void cached_interp_ROUND_W_S(void); void cached_interp_ROUND_W_D(void);
void cached_interp_ROUND_L_S(void); void cached_interp_ROUND_L_D(void);
void cached_interp_C_F_S(void); void cached_interp_C_UN_S(void);
void cached_interp_C_EQ_S(void); void cached_interp_C_UEQ_S(void);
void cached_interp_C_OLT_S(void); void cached_interp_C_ULT_S(void);
void cached_interp_C_OLE_S(void); void cached_interp_C_ULE_S(void);
void cached_interp_C_F_D(void); void cached_interp_C_UN_D(void);
void cached_interp_C_EQ_D(void); void cached_interp_C_UEQ_D(void);
void cached_interp_C_OLT_D(void); void cached_interp_C_ULT_D(void);
void cached_interp_C_OLE_D(void); void cached_interp_C_ULE_D(void);

struct cached_interp ci;

u32 read_inst(u32 addr)
{
   if (addr >= 0x80000000 && addr < 0x80800000)
   {
      u32 index = (addr & 0xFFFFFF) / 4;
      if (index < RDRAM_WORDS)
         return rdram[index];
      return 0;
   }
   else if (addr >= 0xa4000000 && addr < 0xa4001000)
       return SP_DMEM[(addr & 0xFFF) / 4];
   else if (addr >= 0x1fc00000 && addr < 0x1fc01000)
      return ((u32*)ROM_HEADER)[(addr & 0xFFF) / 4];
   else if (tlb_LUT_r)
   {
      u32 paddr = tlb_LUT_r[addr >> 12];
      if (paddr)
      {
         paddr = (paddr & 0xFFFFF000) | (addr & 0xFFF);
         if (paddr >= 0x80000000 && paddr < 0x80800000)
         {
            u32 index = (paddr & 0xFFFFFF) / 4;
            if (index < RDRAM_WORDS)
               return rdram[index];
         }
      }
   }
   return 0;
}

/*static u32 calc_start_address(u32 address)
{
   int i;
   for (i = 0; i < 32; i++)
   {
      if (address >= tlb_e[i].start_even && address <= tlb_e[i].end_even)
      {
         return address;
      }
      if (address >= tlb_e[i].start_odd && address <= tlb_e[i].end_odd)
      {
         return address;
      }
   }
   return address;
}*/

void init_cached_blocks(void)
{
   int i;
   ci.invalid_code = (char*)malloc(0x100000);
   ci.blocks = (struct precomp_block**)malloc(0x100000 * sizeof(struct precomp_block*));
   ci.actual = NULL;
   for (i = 0; i < 0x100000; i++)
   {
      ci.invalid_code[i] = 1;
      ci.blocks[i] = NULL;
   }
}

void free_cached_blocks(void)
{
   int i;
   for (i = 0; i < 0x100000; i++)
   {
      if (ci.blocks[i])
      {
         if (ci.blocks[i]->block)
            free(ci.blocks[i]->block);
         free(ci.blocks[i]);
         ci.blocks[i] = NULL;
      }
   }
   if (ci.invalid_code)
   {
      free(ci.invalid_code);
      ci.invalid_code = NULL;
   }
   if (ci.blocks)
   {
      free(ci.blocks);
      ci.blocks = NULL;
   }
   ci.actual = NULL;
}

/*static void update_invalid_addr(u32 addr)
{
   if (addr >= 0x80000000 && addr < 0xc0000000)
   {
      if (ci.invalid_code[addr >> 12])
         ci.invalid_code[(addr ^ 0x20000000) >> 12] = 1;
      if (ci.invalid_code[(addr ^ 0x20000000) >> 12])
         ci.invalid_code[addr >> 12] = 1;
   }
   else
   {
      u32 paddr = virtual_to_physical_address(addr, 2);
      u32 beg_paddr;
      if (!paddr || !rdram) return;
      beg_paddr = paddr - (addr - (addr & ~0xfff));
      update_invalid_addr(paddr);
      if (ci.invalid_code[(beg_paddr + 0x000) >> 12])
         ci.invalid_code[addr >> 12] = 1;
      if (ci.invalid_code[(beg_paddr + 0xffc) >> 12])
         ci.invalid_code[addr >> 12] = 1;
      if (ci.invalid_code[addr >> 12])
         ci.invalid_code[(beg_paddr + 0x000) >> 12] = 1;
      if (ci.invalid_code[addr >> 12])
         ci.invalid_code[(beg_paddr + 0xffc) >> 12] = 1;
   }
}*/

void cached_interp_init_block(u32 address)
{
   int i, length;
   int block_idx = address >> 12;
   struct precomp_block* b = ci.blocks[block_idx];

   if (b == NULL)
   {
      b = (struct precomp_block*)malloc(sizeof(struct precomp_block));
      if (!b) { DBG_LOG("[BALLOC_FAIL] precomp_block at %08X Count=%08X\n", address, (unsigned int)Count); fflush(stdout); return; }
      b->block = NULL;
      b->start = address & ~0xFFF;
      b->end = (address & ~0xFFF) + 0x1000;
      ci.blocks[block_idx] = b;
   }

   length = (b->end - b->start) / 4;

   if (!b->block)
   {
      int memsize = (length + 1 + (length >> 2)) * sizeof(precomp_instr);
      b->block = (precomp_instr*)malloc(memsize);
      if (!b->block) { DBG_LOG("[BBLOCK_FAIL] addr=%08X memsize=%d Count=%08X\n", address, memsize, (unsigned int)Count); fflush(stdout); return; }
      memset(b->block, 0, memsize);
   }

   for (i = 0; i < length; i++)
   {
      b->block[i].addr = b->start + 4 * i;
      b->block[i].ops = cached_interp_NOTCOMPILED;
   }

   ci.invalid_code[b->start >> 12] = 0;

   if (b->start >= 0x80000000 && b->end < 0xc0000000)
   {
      u32 alt = b->start ^ 0x20000000;
      if (ci.invalid_code[alt >> 12])
         cached_interp_init_block(alt);
   }
   else
   {
      u32 paddr = tlb_LUT_r ? tlb_LUT_r[b->start >> 12] : 0;
      if (paddr)
      {
         paddr = virtual_to_physical_address(b->start, 2);
         cached_interp_init_block(paddr);
         paddr += b->end - b->start - 4;
         cached_interp_init_block(paddr);
      }
   }
}

static void r4300_decode(precomp_instr* inst, u32 iw, u32 next_iw, struct precomp_block* block)
{
   u32 opcode = (iw >> 26) & 0x3F;
   u32 rs = (iw >> 21) & 0x1F;
   u32 rt = (iw >> 16) & 0x1F;
   u32 rd = (iw >> 11) & 0x1F;
   u32 sa = (iw >> 6) & 0x1F;
   u32 funct = iw & 0x3F;
   s32 imm = (s32)(s16)(iw & 0xFFFF);
   u32 target = iw & 0x3FFFFFF;

   inst->addr = inst->addr;

   if (opcode == 0x00)
   {
      inst->f.r.rs = r4300.gpr + rs;
      inst->f.r.rt = r4300.gpr + rt;
      inst->f.r.rd = r4300.gpr + rd;
      inst->f.r.sa = sa;
      inst->f.r.nrd = rd;

      switch (funct)
      {
         case 0x00: inst->ops = cached_interp_SLL; break;
         case 0x02: inst->ops = cached_interp_SRL; break;
         case 0x03: inst->ops = cached_interp_SRA; break;
         case 0x04: inst->ops = cached_interp_SLLV; break;
         case 0x06: inst->ops = cached_interp_SRLV; break;
         case 0x07: inst->ops = cached_interp_SRAV; break;
         case 0x08: inst->ops = cached_interp_JR; break;
          case 0x09: inst->ops = cached_interp_JALR; break;
          case 0x0C: inst->ops = cached_interp_SYSCALL; break;
          case 0x0D: inst->ops = cached_interp_BREAK; break;
         case 0x0F: inst->ops = cached_interp_SYNC; break;
         case 0x10: inst->ops = cached_interp_MFHI; break;
         case 0x11: inst->ops = cached_interp_MTHI; break;
         case 0x12: inst->ops = cached_interp_MFLO; break;
         case 0x13: inst->ops = cached_interp_MTLO; break;
         case 0x14: inst->ops = cached_interp_DSLLV; break;
         case 0x16: inst->ops = cached_interp_DSRLV; break;
         case 0x17: inst->ops = cached_interp_DSRAV; break;
         case 0x18: inst->ops = cached_interp_MULT; break;
         case 0x19: inst->ops = cached_interp_MULTU; break;
         case 0x1A: inst->ops = cached_interp_DIV; break;
         case 0x1B: inst->ops = cached_interp_DIVU; break;
         case 0x1C: inst->ops = cached_interp_DMULT; break;
         case 0x1D: inst->ops = cached_interp_DMULTU; break;
         case 0x1E: inst->ops = cached_interp_DDIV; break;
         case 0x1F: inst->ops = cached_interp_DDIVU; break;
         case 0x20: inst->ops = cached_interp_ADD; break;
         case 0x21: inst->ops = cached_interp_ADDU; break;
         case 0x22: inst->ops = cached_interp_SUB; break;
         case 0x23: inst->ops = cached_interp_SUBU; break;
         case 0x24: inst->ops = cached_interp_AND; break;
         case 0x25: inst->ops = cached_interp_OR; break;
         case 0x26: inst->ops = cached_interp_XOR; break;
         case 0x27: inst->ops = cached_interp_NOR; break;
         case 0x28: inst->ops = cached_interp_NI; break;
         case 0x2A: inst->ops = cached_interp_SLT; break;
         case 0x2B: inst->ops = cached_interp_SLTU; break;
         case 0x2C: inst->ops = cached_interp_DADD; break;
         case 0x2D: inst->ops = cached_interp_DADDU; break;
         case 0x2E: inst->ops = cached_interp_DSUB; break;
         case 0x2F: inst->ops = cached_interp_DSUBU; break;
         case 0x30: inst->ops = cached_interp_TGE; break;
         case 0x31: inst->ops = cached_interp_TGEU; break;
         case 0x32: inst->ops = cached_interp_TLT; break;
         case 0x33: inst->ops = cached_interp_TLTU; break;
         case 0x34: inst->ops = cached_interp_TEQ; break;
         case 0x36: inst->ops = cached_interp_TNE; break;
         case 0x38: inst->ops = cached_interp_DSLL; break;
         case 0x3A: inst->ops = cached_interp_DSRL; break;
         case 0x3B: inst->ops = cached_interp_DSRA; break;
         case 0x3C: inst->ops = cached_interp_DSLL32; break;
         case 0x3E: inst->ops = cached_interp_DSRL32; break;
         case 0x3F: inst->ops = cached_interp_DSRA32; break;
         default: inst->ops = cached_interp_NI; break;
      }
      if (inst->f.r.nrd == 0 && funct != 0x08 && funct != 0x09
          && funct != 0x0C && funct != 0x0D && funct != 0x0F
          && funct != 0x11 && funct != 0x13
          && funct != 0x18 && funct != 0x19 && funct != 0x1A && funct != 0x1B
          && funct != 0x1C && funct != 0x1D && funct != 0x1E && funct != 0x1F
          && funct != 0x30 && funct != 0x31 && funct != 0x32 && funct != 0x33
          && funct != 0x34 && funct != 0x36)
         inst->ops = cached_interp_NOP;
   }
   else if (opcode == 0x01)
   {
      inst->f.i.rs = r4300.gpr + rs;
      inst->f.i.rt = r4300.gpr + rt;
      inst->f.i.immediate = imm;

      switch (rt)
      {
         case 0x00: inst->ops = cached_interp_BLTZ; break;
         case 0x01: inst->ops = cached_interp_BGEZ; break;
         case 0x02: inst->ops = cached_interp_BLTZL; break;
         case 0x03: inst->ops = cached_interp_BGEZL; break;
         case 0x10: inst->ops = cached_interp_BLTZAL; break;
         case 0x11: inst->ops = cached_interp_BGEZAL; break;
         case 0x12: inst->ops = cached_interp_BLTZALL; break;
         case 0x13: inst->ops = cached_interp_BGEZALL; break;
          case 0x08: inst->ops = cached_interp_TGEI; break;
          case 0x09: inst->ops = cached_interp_TGEIU; break;
          case 0x0A: inst->ops = cached_interp_TLTI; break;
          case 0x0B: inst->ops = cached_interp_TLTIU; break;
          case 0x0C: inst->ops = cached_interp_TEQI; break;
          case 0x0E: inst->ops = cached_interp_TNEI; break;
         default: inst->ops = cached_interp_NI; break;
      }
   }
   else if (opcode >= 0x02 && opcode <= 0x03)
   {
      inst->f.j.inst_index = target;

      if (opcode == 0x02) inst->ops = cached_interp_J;
      else inst->ops = cached_interp_JAL;
   }
   else if (opcode >= 0x04 && opcode <= 0x07)
   {
      inst->f.i.rs = r4300.gpr + rs;
      inst->f.i.rt = r4300.gpr + rt;
      inst->f.i.immediate = imm;

      switch (opcode)
      {
         case 0x04: inst->ops = cached_interp_BEQ; break;
         case 0x05: inst->ops = cached_interp_BNE; break;
         case 0x06: inst->ops = cached_interp_BLEZ; break;
         case 0x07: inst->ops = cached_interp_BGTZ; break;
      }
   }
   else if (opcode >= 0x08 && opcode <= 0x0F)
   {
      inst->f.i.rs = r4300.gpr + rs;
      inst->f.i.rt = r4300.gpr + rt;
      inst->f.i.immediate = imm;

      if (rd == 0 && rt == 0) { inst->ops = cached_interp_NOP; return; }

      switch (opcode)
      {
         case 0x08: inst->ops = cached_interp_ADDI; break;
         case 0x09: inst->ops = cached_interp_ADDIU; break;
         case 0x0A: inst->ops = cached_interp_SLTI; break;
         case 0x0B: inst->ops = cached_interp_SLTIU; break;
         case 0x0C: inst->ops = cached_interp_ANDI; break;
         case 0x0D: inst->ops = cached_interp_ORI; break;
         case 0x0E: inst->ops = cached_interp_XORI; break;
         case 0x0F: inst->ops = cached_interp_LUI; break;
      }
   }
   else if (opcode >= 0x10 && opcode <= 0x13)
   {
      inst->f.r.rs = r4300.gpr + rs;
      inst->f.r.rt = r4300.gpr + rt;
      inst->f.r.rd = r4300.gpr + rd;
      inst->f.r.nrd = rd;
      inst->f.r.sa = sa;

      switch (opcode)
      {
          case 0x10:
          {
             switch (rs)
             {
                case 0x00: inst->ops = cached_interp_MFC0; break;
                case 0x04: inst->ops = cached_interp_MTC0; break;
                case 0x10:
                   switch (funct)
                   {
                      case 0x01: inst->ops = cached_interp_TLBR; break;
                      case 0x02: inst->ops = cached_interp_TLBWI; break;
                      case 0x06: inst->ops = cached_interp_TLBWR; break;
                      case 0x08: inst->ops = cached_interp_TLBP; break;
                      case 0x18: inst->ops = cached_interp_ERET; break;
                      default: inst->ops = cached_interp_NI; break;
                   }
                   break;
                default: inst->ops = cached_interp_NI; break;
             }
             break;
          }
         case 0x11:
         {
            if (rs == 0x00)
               inst->ops = cached_interp_MFC1;
            else if (rs == 0x01)
               inst->ops = cached_interp_DMFC1;
            else if (rs == 0x02)
               inst->ops = cached_interp_CFC1;
            else if (rs == 0x04)
               inst->ops = cached_interp_MTC1;
            else if (rs == 0x05)
               inst->ops = cached_interp_DMTC1;
            else if (rs == 0x06)
               inst->ops = cached_interp_CTC1;
            else if (rs == 0x08)
            {
               inst->f.i.immediate = imm;
               switch (rt & 3)
               {
                  case 0: inst->ops = cached_interp_BC1F; break;
                  case 1: inst->ops = cached_interp_BC1T; break;
                  case 2: inst->ops = cached_interp_BC1FL; break;
                  case 3: inst->ops = cached_interp_BC1TL; break;
                  default: inst->ops = cached_interp_NI; break;
               }
            }
            else
            {
               u32 fmt = (iw >> 21) & 0x1F;
               u32 ft = (iw >> 16) & 0x1F;
               u32 fs = (iw >> 11) & 0x1F;
               u32 fd = (iw >> 6) & 0x1F;
               inst->f.cf.ft = ft;
               inst->f.cf.fs = fs;
               inst->f.cf.fd = fd;
               inst->f.r.nrd = funct;

               switch (funct)
               {
                  case 0x00: inst->ops = (fmt == 16) ? cached_interp_ADD_S : cached_interp_ADD_D; break;
                  case 0x01: inst->ops = (fmt == 16) ? cached_interp_SUB_S : cached_interp_SUB_D; break;
                  case 0x02: inst->ops = (fmt == 16) ? cached_interp_MUL_S : cached_interp_MUL_D; break;
                  case 0x03: inst->ops = (fmt == 16) ? cached_interp_DIV_S : cached_interp_DIV_D; break;
                  case 0x04: inst->ops = (fmt == 16) ? cached_interp_SQRT_S : cached_interp_SQRT_D; break;
                  case 0x05: inst->ops = (fmt == 16) ? cached_interp_ABS_S : cached_interp_ABS_D; break;
                  case 0x06: inst->ops = (fmt == 16) ? cached_interp_MOV_S : cached_interp_MOV_D; break;
                  case 0x07: inst->ops = (fmt == 16) ? cached_interp_NEG_S : cached_interp_NEG_D; break;
                  case 0x20:
                     if (fmt == 17) inst->ops = cached_interp_CVT_S_D;
                     else if (fmt == 20) inst->ops = cached_interp_CVT_S_W;
                     else if (fmt == 21) inst->ops = cached_interp_CVT_S_L;
                     else inst->ops = cached_interp_NI;
                     break;
                  case 0x21:
                     if (fmt == 16) inst->ops = cached_interp_CVT_D_S;
                     else if (fmt == 20) inst->ops = cached_interp_CVT_D_W;
                     else if (fmt == 21) inst->ops = cached_interp_CVT_D_L;
                     else inst->ops = cached_interp_NI;
                     break;
                  case 0x24:
                     if (fmt == 16) inst->ops = cached_interp_CVT_W_S;
                     else if (fmt == 17) inst->ops = cached_interp_CVT_W_D;
                     else inst->ops = cached_interp_NI;
                     break;
                  case 0x25:
                     if (fmt == 16) inst->ops = cached_interp_CVT_L_S;
                     else if (fmt == 17) inst->ops = cached_interp_CVT_L_D;
                     else inst->ops = cached_interp_NI;
                     break;
                   case 0x08: inst->ops = (fmt == 16) ? cached_interp_ROUND_L_S : cached_interp_ROUND_L_D; break;
                   case 0x09: inst->ops = (fmt == 16) ? cached_interp_TRUNC_L_S : cached_interp_TRUNC_L_D; break;
                   case 0x0A: inst->ops = (fmt == 16) ? cached_interp_CEIL_L_S : cached_interp_CEIL_L_D; break;
                   case 0x0B: inst->ops = (fmt == 16) ? cached_interp_FLOOR_L_S : cached_interp_FLOOR_L_D; break;
                   case 0x0C: inst->ops = (fmt == 16) ? cached_interp_ROUND_W_S : cached_interp_ROUND_W_D; break;
                   case 0x0D: inst->ops = (fmt == 16) ? cached_interp_TRUNC_W_S : cached_interp_TRUNC_W_D; break;
                   case 0x0E: inst->ops = (fmt == 16) ? cached_interp_CEIL_W_S : cached_interp_CEIL_W_D; break;
                   case 0x0F: inst->ops = (fmt == 16) ? cached_interp_FLOOR_W_S : cached_interp_FLOOR_W_D; break;
                   case 0x30:
                   case 0x31:
                   case 0x32:
                   case 0x33:
                   case 0x34:
                   case 0x35:
                   case 0x36:
                   case 0x37:
                   {
                      static void (*c_cmp[8])(void) = {
                         cached_interp_C_F_S, cached_interp_C_UN_S, cached_interp_C_EQ_S,
                         cached_interp_C_UEQ_S, cached_interp_C_OLT_S, cached_interp_C_ULT_S,
                         cached_interp_C_OLE_S, cached_interp_C_ULE_S
                      };
                      static void (*c_cmp_d[8])(void) = {
                         cached_interp_C_F_D, cached_interp_C_UN_D, cached_interp_C_EQ_D,
                         cached_interp_C_UEQ_D, cached_interp_C_OLT_D, cached_interp_C_ULT_D,
                         cached_interp_C_OLE_D, cached_interp_C_ULE_D
                      };
                      inst->ops = (fmt == 16) ? c_cmp[funct - 0x30] : c_cmp_d[funct - 0x30];
                      break;
                   }
                   case 0x38:
                   case 0x39:
                   case 0x3A:
                   case 0x3B:
                   case 0x3C:
                   case 0x3D:
                   case 0x3E:
                   case 0x3F:
                   {
                      static void (*c_cmp2[8])(void) = {
                         cached_interp_C_F_S, cached_interp_C_F_S, cached_interp_C_EQ_S,
                         cached_interp_C_EQ_S, cached_interp_C_OLT_S, cached_interp_C_OLT_S,
                         cached_interp_C_OLE_S, cached_interp_C_OLE_S
                      };
                      static void (*c_cmp2_d[8])(void) = {
                         cached_interp_C_F_D, cached_interp_C_F_D, cached_interp_C_EQ_D,
                         cached_interp_C_EQ_D, cached_interp_C_OLT_D, cached_interp_C_OLT_D,
                         cached_interp_C_OLE_D, cached_interp_C_OLE_D
                      };
                      inst->ops = (fmt == 16) ? c_cmp2[funct - 0x38] : c_cmp2_d[funct - 0x38];
                      break;
                   }
                  default: inst->ops = cached_interp_NI; break;
               }
            }
            break;
         }
         default: inst->ops = cached_interp_NI; break;
      }
   }
   else if (opcode >= 0x14 && opcode <= 0x1F)
   {
      inst->f.i.rs = r4300.gpr + rs;
      inst->f.i.rt = r4300.gpr + rt;
      inst->f.i.immediate = imm;

      switch (opcode)
      {
         case 0x14: inst->ops = cached_interp_BEQL; break;
         case 0x15: inst->ops = cached_interp_BNEL; break;
         case 0x16: inst->ops = cached_interp_BLEZL; break;
         case 0x17: inst->ops = cached_interp_BGTZL; break;
         case 0x18:
         case 0x19:
         case 0x1A:
         case 0x1B:
         case 0x1C:
         case 0x1D:
         {
            switch (opcode)
            {
               case 0x18: inst->ops = cached_interp_DADDI; break;
               case 0x19: inst->ops = cached_interp_DADDIU; break;
               case 0x1A: inst->ops = cached_interp_LDL; break;
               case 0x1B: inst->ops = cached_interp_LDR; break;
               case 0x1C: inst->ops = cached_interp_NI; break;
               case 0x1D: inst->ops = cached_interp_NI; break;
            }
            break;
         }
         case 0x1E: inst->ops = cached_interp_LQ; break;
         case 0x1F: inst->ops = cached_interp_SQ; break;
      }
   }
   else if (opcode >= 0x20 && opcode <= 0x2F)
   {
      inst->f.i.rs = r4300.gpr + rs;
      inst->f.i.rt = r4300.gpr + rt;
      inst->f.i.immediate = imm;

       if (rt == 0 && opcode <= 0x27) { inst->ops = cached_interp_NOP; return; }

       switch (opcode)
       {
          case 0x20: inst->ops = cached_interp_LB; break;
         case 0x21: inst->ops = cached_interp_LH; break;
         case 0x22: inst->ops = cached_interp_LWL; break;
         case 0x23: inst->ops = cached_interp_LW; break;
         case 0x24: inst->ops = cached_interp_LBU; break;
         case 0x25: inst->ops = cached_interp_LHU; break;
         case 0x26: inst->ops = cached_interp_LWR; break;
         case 0x27: inst->ops = cached_interp_LWU; break;
         case 0x28: inst->ops = cached_interp_SB; break;
         case 0x29: inst->ops = cached_interp_SH; break;
         case 0x2A: inst->ops = cached_interp_SWL; break;
         case 0x2B: inst->ops = cached_interp_SW; break;
         case 0x2C: inst->ops = cached_interp_SDL; break;
         case 0x2D: inst->ops = cached_interp_SDR; break;
         case 0x2E: inst->ops = cached_interp_SWR; break;
         case 0x2F: inst->ops = cached_interp_CACHE; break;
      }
   }
   else if (opcode >= 0x30 && opcode <= 0x3F)
   {
      inst->f.i.rs = r4300.gpr + rs;
      inst->f.i.rt = r4300.gpr + rt;
      inst->f.i.immediate = imm;

      switch (opcode)
      {
         case 0x30: inst->ops = cached_interp_LL; break;
         case 0x31:
            inst->f.lf.base = rs;
            inst->f.lf.ft = rt;
            inst->f.lf.offset = imm;
            inst->ops = cached_interp_LWC1;
            break;
         case 0x32: inst->ops = cached_interp_LWC2; break;
         case 0x33: inst->ops = cached_interp_PREF; break;
         case 0x34: inst->ops = cached_interp_LLD; break;
         case 0x35:
            inst->f.lf.base = rs;
            inst->f.lf.ft = rt;
            inst->f.lf.offset = imm;
            inst->ops = cached_interp_LDC1;
            break;
         case 0x36: inst->ops = cached_interp_LDC2; break;
         case 0x37: inst->ops = cached_interp_LD; break;
         case 0x38: inst->ops = cached_interp_SC; break;
         case 0x39:
            inst->f.lf.base = rs;
            inst->f.lf.ft = rt;
            inst->f.lf.offset = imm;
            inst->ops = cached_interp_SWC1;
            break;
         case 0x3A: inst->ops = cached_interp_SWC2; break;
         case 0x3B: inst->ops = cached_interp_NI; break;
         case 0x3C: inst->ops = cached_interp_SCD; break;
         case 0x3D:
            inst->f.lf.base = rs;
            inst->f.lf.ft = rt;
            inst->f.lf.offset = imm;
            inst->ops = cached_interp_SDC1;
            break;
         case 0x3E: inst->ops = cached_interp_SDC2; break;
         case 0x3F: inst->ops = cached_interp_SD; break;
      }
   }
   else if (opcode < 0x3F)
   {
      inst->f.i.rs = r4300.gpr + rs;
      inst->f.i.rt = r4300.gpr + rt;
      inst->f.i.immediate = imm;
      inst->ops = cached_interp_NI;
   }
}

void cached_interp_recompile_block(u32 address)
{
   int i, length, length2, finished;
   precomp_instr* inst;
   struct precomp_block* block = ci.blocks[address >> 12];
   u32 iw[0x402];

   if (!block) return;
   if (!block->block) return;

   if (block->start >= 0xc0000000 || block->end < 0x80000000)
   {
      u32 entry = (address - block->start) >> 2;
      u32 paddr = tlb_LUT_r ? tlb_LUT_r[block->start >> 12] : 0;
      if (!paddr && entry < (block->end - block->start) / 4)
      {
         block->block[entry].addr = address;
         block->block[entry].ops = cached_interp_TLB_REFILL;
         block->block[entry + 1].addr = address + 4;
         block->block[entry + 1].ops = cached_interp_FIN_BLOCK;
         return;
      }
   }

   length = (block->end - block->start) / 4;
   length2 = length - 2 + (length >> 2);

   for (i = 0; i < length + 2; i++)
      iw[i] = read_inst(block->start + i * 4);

   block->xxhash = 0;

   for (i = (address & 0xFFF) / 4, finished = 0; finished != 2; i++)
   {
      if (i >= length + 2) break;
      inst = block->block + i;
      inst->addr = block->start + i * 4;

      r4300_decode(inst, iw[i], iw[i + 1], block);

      if (i >= length2) finished = 2;
      if (i >= (length - 1) && (block->start >= 0xc0000000 || block->end < 0x80000000))
         finished = 2;
      if (inst->ops == cached_interp_ERET || finished == 1)
         finished = 2;
      if ((inst->ops == cached_interp_J || inst->ops == cached_interp_JR) &&
          !(i >= (length - 1) && block->end < 0x80000000))
         finished = 1;
   }

   if (i >= length)
   {
      inst = block->block + i;
      inst->addr = block->start + i * 4;
      inst->ops = cached_interp_FIN_BLOCK;
      i++;
      if (i <= length2)
      {
         inst = block->block + i;
         inst->addr = block->start + i * 4;
          inst->ops = cached_interp_FIN_BLOCK;
       }
   }
}

void cached_interpreter_jump_to(u32 address)
{
   if (r4300.skip_jump) return;
   if (ci.invalid_code[address >> 12])
      cached_interp_init_block(address);
   ci.actual = ci.blocks[address >> 12];
   if (!ci.actual || !ci.actual->block) return;
   PC = ci.actual->block + ((address - ci.actual->start) >> 2);
}

void cached_interp_take_exception(void)
{
   u32 target = r4300.skip_jump ? r4300.skip_jump : r4300.pc;
   r4300.pc = target;
   if (ci.invalid_code[target >> 12])
      cached_interp_init_block(target);
   ci.actual = ci.blocks[target >> 12];
   if (!ci.actual || !ci.actual->block) { r4300.stop = 1; return; }
   PC = ci.actual->block + ((target - ci.actual->start) >> 2);
}

static void generic_jump_to(u32 address)
{
   if (r4300.skip_jump) return;
   if (ci.blocks[address >> 12] == NULL || ci.invalid_code[address >> 12])
   {
      cached_interp_init_block(address);
      cached_interp_recompile_block(address);
   }
   ci.actual = ci.blocks[address >> 12];
   if (!ci.actual || !ci.actual->block) return;
   PC = ci.actual->block + ((address - ci.actual->start) >> 2);
}

void invalidate_cached_code(u32 address, u32 size)
{
   u32 addr, addr_max;
   int i;

   if (size == 0)
   {
      memset(ci.invalid_code, 1, 0x100000);
      return;
   }

   addr_max = address + size;
   for (addr = address; addr < addr_max; addr += 4)
   {
      i = addr >> 12;
      if (ci.invalid_code[i] == 0)
      {
          if (ci.blocks[i] == NULL || ci.blocks[i]->block == NULL ||
              ci.blocks[i]->block[(addr & 0xfff) / 4].ops != cached_interp_NOTCOMPILED)
         {
            ci.invalid_code[i] = 1;
            addr = (addr & ~0xfff) | 0xffc;
         }
      }
      else
         addr = (addr & ~0xfff) | 0xffc;
   }
}

#ifdef CACHED_DEBUG
static int dumped_spin_code = 0;
static int dumped_final_code = 0;
static int gfx_alloc_cnt = 0;
static int dumped_fmt = 0;
static int dumped_exit_entry = 0;
static int dumped_frk_code = 0;
static int dumped_mod_code = 0;
static int gd_fail_cnt = 0;
static unsigned long long ci_trace_cnt = 0;
static u32 gw_cnt = 0;
static int gw_fatal = 0;
static u32 fr_cnt = 0;
static u32 frk_cnt = 0;
static u32 du_cnt = 0;
static u32 spf_dumped = 0;
static u32 md_cnt = 0;
static u32 md_arg_lo = 0, md_arg_hi = 0, md_base = 0;
static int md_capture = 0;
static int frk_sprintf_active = 0;
static int dm2_cnt = 0;
static int dm2_active = 0;
static int dumped_spin2_code = 0;
static int dumped_reset_code = 0;
static int t5create_cnt = 0;
static int t5ent_cnt = 0;
static int idle_dispatch_cnt = 0;
static int dumped_t5create_code = 0;
static int t5_trace_cnt = 0;
static int t5_was_in = 0;
static int t5block_cnt = 0;
static int t5pi_trace = 0;
static int t5load_trace = 0;
static u32 triadd_cnt = 0;
static u32 trireset_cnt = 0;
static u32 append_hit_cnt = 0;
static u32 append_new_cnt = 0;
static u32 flushcall_cnt = 0;
static u32 dedup_cmp_cnt = 0;
#endif

void run_cached_interpreter(void)
{
#ifdef CACHED_DEBUG
   u32 spin_cnt = 0;
#endif
   static s64 chb_last_us = 0;
   extern s64 sysGetSystemTime(void);
   r4300.stop = 0;
   r4300.last_pc = r4300.pc;

   cached_interp_init_block(r4300.pc);
   cached_interp_recompile_block(r4300.pc);

   ci.actual = ci.blocks[r4300.pc >> 12];
   if (!ci.actual || !ci.actual->block) return;
   PC = ci.actual->block + ((r4300.pc - ci.actual->start) >> 2);

while (!r4300.stop)
    {
       Count += 2;
#ifdef PS3
       {
          s64 now_us = sysGetSystemTime();
          s64 dt_us = now_us - chb_last_us;
          if (dt_us > 2000000)
          {
             chb_last_us = now_us;
             DBG_LOG("[CHB2] pc=%08X Count=%08X ni=%08X dt=%lldus\n",
                (unsigned int)r4300.pc, (unsigned int)Count,
                (unsigned int)r4300.next_interrupt, (long long)dt_us);
             fflush(stdout);
          }
       }
       if ((Count & 0x1FFF) == 0)
       {
          /* Poll pads periodically. Low frequency avoids draining
           * pad data before _GetKeys reads from the shared cache. */
          controller_PS3_poll_pad();
          if (ps3_pad_exit_combo_pressed())
          {
#ifdef DEBUG
             DBG_LOG("[EXIT] Combo Square+Triangle -> r4300.stop=1\n");
#endif
             r4300.stop = 1;
             break;
          }
       }
#endif
       if (!PC) { DBG_LOG("[STOP] PC=NULL Count=%08X\n", (unsigned int)Count); fflush(stdout); r4300.stop = 1; break; }
      if (ci.actual && ci.actual->block)
      {
         u32 length = (ci.actual->end - ci.actual->start) / 4;
         precomp_instr* block_end = ci.actual->block + length + 1 + (length >> 2);
         if (PC < ci.actual->block || PC >= block_end)
         {
            DBG_LOG("[STOP] PC OOB pc=%p block=[%p,%p) Count=%08X\n", (void*)PC, (void*)ci.actual->block, (void*)block_end, (unsigned int)Count); fflush(stdout);
            r4300.stop = 1;
            break;
         }
      }
      if (!PC->ops)
      {
         DBG_LOG("[STOP] PC->ops=NULL pc=%08X Count=%08X\n", (unsigned int)PC->addr, (unsigned int)Count); fflush(stdout);
         r4300.stop = 1;
         break;
      }
      r4300.pc = PC->addr;
#ifdef CACHED_DEBUG
      ci_trace_cnt++;
      if ((ci_trace_cnt & 0xFFFFF) == 0)
         DBG_LOG("[TC] %08llX %08X v0=%08X a0=%08X sp=%08X\n", ci_trace_cnt,
            r4300.pc, (u32)r4300.gpr[2], (u32)r4300.gpr[4], (u32)r4300.gpr[29]);
      if (r4300.pc == 0x8019BBD8 && gd_fail_cnt < 8)
      {
         u32 gs = (u32)r4300.gpr[8];
         u32 gp = rdram ? rdram[(0x801A882C & 0xFFFFFF) / 4] : 0;
         u32 ge = rdram ? rdram[(0x801A8828 & 0xFFFFFF) / 4] : 0;
         u32 cra = 0;
         if (rdram && r4300.gpr[29])
            cra = (unsigned int)rdram[(((u32)r4300.gpr[29] + 0x14) & 0xFFFFFF) / 4];
         DBG_LOG("[GDFAIL#%d] size=%u (0x%X) a1=%u sp=%08X caller_ra=%08X poolPtr=%08X poolEnd=%08X remain=%d\n",
            gd_fail_cnt, gs, gs, (u32)r4300.gpr[5], (u32)r4300.gpr[29], cra, gp, ge, (int)(ge - gp));
         gd_fail_cnt++;
      }
       if (r4300.pc == 0x80317934)
       {
          if ((spin_cnt & 0xFFFF) == 0)
             DBG_LOG("[SPINVAL] a0=%08X val=%08X\n", (u32)r4300.gpr[4],
                rdram ? rdram[(0x80226B80 & 0xFFFFFF) / 4] : 0xDEADBEEF);
       }
       if (r4300.pc == 0x80188B80 && gw_cnt < 100)
       {
          u32 gwptr = rdram ? rdram[(0x801A83E4 & 0xFFFFFF) / 4] : 0;
          u32 gwtag = (gwptr && rdram) ? rdram[((gwptr + 12) & 0xFFFFFF) / 4] : 0;
          u32 gwname = rdram ? rdram[(0x801BA0C0 & 0xFFFFFF) / 4] : 0;
          DBG_LOG("[GW#%u] ptr=%08X tag=%08X name=%08X a0=%08X sp=%08X Count=%08X\n",
             gw_cnt, gwptr, gwtag, gwname, (u32)r4300.gpr[4], (u32)r4300.gpr[29],
             (unsigned int)Count);
          gw_cnt++;
       }
       if (r4300.pc == 0x801891B0 && !gw_fatal)
       {
          u32 gwptr = rdram ? rdram[(0x801A83E4 & 0xFFFFFF) / 4] : 0;
          u32 gwtag = (gwptr && rdram) ? rdram[((gwptr + 12) & 0xFFFFFF) / 4] : 0;
          u32 gwname = rdram ? rdram[(0x801BA0C0 & 0xFFFFFF) / 4] : 0;
          int oi;
          gw_fatal = 1;
          DBG_LOG("[GWFATAL] pc=801891B0 ptr=%08X tag=%08X name=%08X a0=%08X sp=%08X ra=%08X Count=%08X ni=%08X\n",
             gwptr, gwtag, gwname, (u32)r4300.gpr[4], (u32)r4300.gpr[29], (u32)r4300.gpr[31],
             (unsigned int)Count, r4300.next_interrupt);
          if (gwptr && (gwptr & 0xFFFFFF) + 64 < 0xFFFFFF)
          {
             DBG_LOG("[GWFATAL] obj dump @%08X:\n", gwptr);
             for (oi = 0; oi < 16; oi++)
                printf("  +0x%02X: %08X\n", oi * 4, rdram[((gwptr + oi * 4) & 0xFFFFFF) / 4]);
          }
       }
        if (r4300.pc == 0x80184510 && fr_cnt < 200)
        {
           u32 frbase = rdram ? rdram[(0x801A83E0 & 0xFFFFFF) / 4] : 0;
           u32 frcnt = rdram ? rdram[(0x801BA0BC & 0xFFFFFF) / 4] : 0;
           DBG_LOG("[FR#%u] a0=%08X base=%08X cnt=%u ra=%08X Count=%08X\n",
              fr_cnt, (u32)r4300.gpr[4], frbase, frcnt, (u32)r4300.gpr[31], (unsigned int)Count);
           fr_cnt++;
        }
        if (r4300.pc == 0x80186C00 && du_cnt < 120)
        {
           DBG_LOG("[DU#%u] a0=%08X ra=%08X Count=%08X\n",
              du_cnt, (u32)r4300.gpr[4], (u32)r4300.gpr[31], (unsigned int)Count);
           du_cnt++;
        }
         if (r4300.pc == 0x80184558)
         {
            u32 fmtaddr = (u32)r4300.gpr[5];
            int fi;
            frk_sprintf_active = 1;
            DBG_LOG("[FRKSPF] sprintf entry a0=%08X a1=%08X a2=%08X a3=%08X Count=%08X fmt=\"",
               (u32)r4300.gpr[4], (u32)r4300.gpr[5], (u32)r4300.gpr[6], (u32)r4300.gpr[7],
               (unsigned int)Count);
            if (rdram && fmtaddr)
            {
               for (fi = 0; fi < 24; fi++)
               {
                  u32 w = rdram[((fmtaddr + fi) & 0xFFFFFF) / 4];
                  char c1 = (char)((w >> 24) & 0xFF);
                  char c2 = (char)((w >> 16) & 0xFF);
                  char c3 = (char)((w >> 8) & 0xFF);
                  char c4 = (char)(w & 0xFF);
                  if (c1) printf("%c", c1); else break;
                  if (c2) printf("%c", c2); else break;
                  if (c3) printf("%c", c3); else break;
                  if (c4) printf("%c", c4); else break;
               }
            }
            printf("\"\n");
         }
         if (r4300.pc == 0x80184560)
            frk_sprintf_active = 0;
          if (r4300.pc == 0x8032411C && md_cnt < 400 && frk_sprintf_active)
         {
            md_cnt++;
            md_capture = 1;
            md_arg_lo = (u32)r4300.gpr[4];
            md_arg_hi = (u32)r4300.gpr[5];
            md_base = (u32)r4300.gpr[7];
            DBG_LOG("[MD#%u] MOD  in arg=%08X%08X base=%u ra=%08X Count=%08X\n",
               md_cnt, md_arg_hi, md_arg_lo, md_base, (u32)r4300.gpr[31], (unsigned int)Count);
            if (!dumped_mod_code && rdram)
            {
               int di;
               dumped_mod_code = 1;
               DBG_LOG("[MODCODEDUMP] around 0x8032411C:\n");
               for (di = 0; di < 164; di++)
                  printf("  0x%08X: %08X\n", 0x803240E0 + di * 4,
                     rdram[((0x803240E0 + di * 4) & 0xFFFFFF) / 4]);
               DBG_LOG("[DIGHANDLERDUMP] around 0x80329790:\n");
               for (di = 0; di < 160; di++)
                  printf("  0x%08X: %08X\n", 0x80329760 + di * 4,
                     rdram[((0x80329760 + di * 4) & 0xFFFFFF) / 4]);
               DBG_LOG("[DIVMODHELPER] around 0x8032B060:\n");
               for (di = 0; di < 112; di++)
                  printf("  0x%08X: %08X\n", 0x8032B060 + di * 4,
                     rdram[((0x8032B060 + di * 4) & 0xFFFFFF) / 4]);
            }
         }
         if (r4300.pc == 0x80324148 && md_capture && md_cnt < 400)
        {
           md_capture = 0;
           DBG_LOG("[MD#%u] MOD  out v0=%016llX (arg=%08X%08X base=%u)\n",
              md_cnt, (unsigned long long)r4300.gpr[2], md_arg_hi, md_arg_lo, md_base);
        }
         if (r4300.pc == 0x80324158 && md_cnt < 400 && frk_sprintf_active)
        {
           md_cnt++;
           md_capture = 1;
           md_arg_lo = (u32)r4300.gpr[4];
           md_arg_hi = (u32)r4300.gpr[5];
           md_base = (u32)r4300.gpr[7];
           DBG_LOG("[MD#%u] DIV  in arg=%08X%08X base=%u ra=%08X Count=%08X\n",
              md_cnt, md_arg_hi, md_arg_lo, md_base, (u32)r4300.gpr[31], (unsigned int)Count);
        }
          if (r4300.pc == 0x80324184 && md_capture && md_cnt < 400)
         {
            md_capture = 0;
            DBG_LOG("[MD#%u] DIV  out v0=%016llX (arg=%08X%08X base=%u)\n",
               md_cnt, (unsigned long long)r4300.gpr[2], md_arg_hi, md_arg_lo, md_base);
         }
          if (r4300.pc == 0x8032B060 && dm2_cnt < 60)
         {
            dm2_active = 1;
            dm2_cnt++;
            DBG_LOG("[B0ENT] a0=%08X a2=%08X a3=%08X ra=%08X base=%08X\n",
               (u32)r4300.gpr[4], (u32)r4300.gpr[6], (u32)r4300.gpr[7],
               (u32)r4300.gpr[31],
               rdram ? rdram[(((u32)r4300.gpr[29] + 0x44) & 0xFFFFFF) / 4] : 0);
         }
         if (r4300.pc == 0x80324214 && dm2_active)
         {
            DBG_LOG("[BDDIV] t6=%016llX t7=%016llX\n",
               (unsigned long long)r4300.gpr[14], (unsigned long long)r4300.gpr[15]);
         }
         if (r4300.pc == 0x8032421C && dm2_active)
         {
            DBG_LOG("[BDDIVRES] lo=%016llX hi=%016llX\n",
               (unsigned long long)r4300.lo, (unsigned long long)r4300.hi);
         }
         if (r4300.pc == 0x80324248 && dm2_active)
         {
            DBG_LOG("[BDIVRES] v0=%016llX\n", (unsigned long long)r4300.gpr[2]);
         }
         if (r4300.pc == 0x80324270 && dm2_active)
         {
            DBG_LOG("[BMUL] t6=%016llX t7=%016llX\n",
               (unsigned long long)r4300.gpr[14], (unsigned long long)r4300.gpr[15]);
         }
         if (r4300.pc == 0x80324274 && dm2_active)
         {
            DBG_LOG("[BMULRES2] lo=%016llX hi=%016llX\n",
               (unsigned long long)r4300.lo, (unsigned long long)r4300.hi);
         }
         if (r4300.pc == 0x80324278 && dm2_active)
         {
            DBG_LOG("[BMULRES] v0=%016llX\n", (unsigned long long)r4300.gpr[2]);
         }
         if (r4300.pc == 0x8032B0C8 && dm2_active)
         {
            DBG_LOG("[BB] t8=%08X v0=%016llX\n", (u32)r4300.gpr[24], (unsigned long long)r4300.gpr[2]);
         }
         if (r4300.pc == 0x8032B12C && dm2_active)
         {
            DBG_LOG("[BRET] rem_lo=%08X rem_hi=%08X\n",
               rdram ? rdram[(((u32)r4300.gpr[29] + 0x20) & 0xFFFFFF) / 4] : 0,
               rdram ? rdram[(((u32)r4300.gpr[29] + 0x24) & 0xFFFFFF) / 4] : 0);
         }
          if (r4300.pc == 0x8032B158)
             dm2_active = 0;
          if (r4300.pc == 0x8000008C && !dumped_reset_code)
          {
             int di;
             dumped_reset_code = 1;
             DBG_LOG("[RESETCODE] around 0x8000008C:\n");
             for (di = 0; di < 48; di++)
                printf("  0x%08X: %08X\n", 0x80000060 + di * 4,
                   rdram ? rdram[((0x80000060 + di * 4) & 0xFFFFFF) / 4] : 0);
             DBG_LOG("[RESETCODE] sp=%08X ra=%08X a0=%08X a1=%08X a2=%08X v0=%08X Count=%08X\n",
                (u32)r4300.gpr[29], (u32)r4300.gpr[31], (u32)r4300.gpr[4],
                (u32)r4300.gpr[5], (u32)r4300.gpr[6], (u32)r4300.gpr[2],
                (unsigned int)Count);
              DBG_LOG("[RESETCODE] precompiled block @ pc 0x8000008C: block_start=%08X pc_index=%u\n",
                 ci.actual ? ci.actual->start : 0,
                 ci.actual ? (unsigned int)((0x8000008C - ci.actual->start) / 4) : 0);
              if (PC)
              {
                 DBG_LOG("[RESETCODE] PC->addr=%08X PC->ops=%p\n",
                    PC->addr, (void*)PC->ops);
                 DBG_LOG("[RESETCODE] rdram[8C]=%08X rdram[88]=%08X rdram[90]=%08X\n",
                    rdram ? rdram[0x8C / 4] : 0,
                    rdram ? rdram[0x88 / 4] : 0,
                    rdram ? rdram[0x90 / 4] : 0);
              }
            }
          if (r4300.pc == 0x80246380 && t5create_cnt < 20)
          {
             int di;
             t5create_cnt++;
             DBG_LOG("[T5CREATE#%d] pc=80246380 a0=%08X a1=%08X a2=%08X a3=%08X sp=%08X ra=%08X Count=%08X\n",
                t5create_cnt, (u32)r4300.gpr[4], (u32)r4300.gpr[5], (u32)r4300.gpr[6],
                (u32)r4300.gpr[7], (u32)r4300.gpr[29], (u32)r4300.gpr[31], (unsigned int)Count);
             if (!dumped_t5create_code)
             {
                dumped_t5create_code = 1;
                DBG_LOG("[T5CREATE] thread5_create body 0x80246380:\n");
                for (di = 0; di < 48; di++)
                   printf("  0x%08X: %08X\n", 0x80246380 + di * 4,
                      rdram ? rdram[((0x80246380 + di * 4) & 0xFFFFFF) / 4] : 0);
             }
          }
          if (r4300.pc == 0x802469B8 && t5ent_cnt < 5)
          {
             int di;
             t5ent_cnt++;
             DBG_LOG("[T5ENT#%d] thread5 ENTRY RAN pc=802469B8 sp=%08X ra=%08X a0=%08X Count=%08X\n",
                t5ent_cnt, (u32)r4300.gpr[29], (u32)r4300.gpr[31], (u32)r4300.gpr[4], (unsigned int)Count);
             if (t5ent_cnt == 1)
             {
                DBG_LOG("[T5ENT] thread5 entry code 0x802469B8:\n");
                for (di = 0; di < 64; di++)
                   printf("  0x%08X: %08X\n", 0x802469B8 + di * 4,
                      rdram ? rdram[((0x802469B8 + di * 4) & 0xFFFFFF) / 4] : 0);
             }
          }
          if (r4300.pc == 0x80278974 && t5block_cnt < 8)
          {
             int di;
             t5block_cnt++;
             DBG_LOG("[T5BLOCK#%d] pc=80278974 sp=%08X ra=%08X a0=%08X a1=%08X Count=%08X\n",
                t5block_cnt, (u32)r4300.gpr[29], (u32)r4300.gpr[31], (u32)r4300.gpr[4],
                (u32)r4300.gpr[5], (unsigned int)Count);
             if (t5block_cnt == 1)
             {
                DBG_LOG("[T5BLOCK] code 0x80278974:\n");
                for (di = 0; di < 64; di++)
                   printf("  0x%08X: %08X\n", 0x80278974 + di * 4,
                      rdram ? rdram[((0x80278974 + di * 4) & 0xFFFFFF) / 4] : 0);
             }
          }
          if (r4300.pc >= 0x80278974 && r4300.pc < 0x80278A10 && t5pi_trace < 120)
          {
             t5pi_trace++;
             DBG_LOG("[T5PI] pc=%08X sp=%08X ra=%08X a0=%08X a1=%08X v0=%08X Count=%08X\n",
                r4300.pc, (u32)r4300.gpr[29], (u32)r4300.gpr[31], (u32)r4300.gpr[4],
                (u32)r4300.gpr[5], (u32)r4300.gpr[2], (unsigned int)Count);
          }
          if (r4300.pc >= 0x80278504 && r4300.pc < 0x80278650 && t5load_trace < 200)
          {
             int di;
             t5load_trace++;
             DBG_LOG("[T5LOAD] pc=%08X sp=%08X ra=%08X a0=%08X a1=%08X a2=%08X Count=%08X\n",
                r4300.pc, (u32)r4300.gpr[29], (u32)r4300.gpr[31], (u32)r4300.gpr[4],
                (u32)r4300.gpr[5], (u32)r4300.gpr[6], (unsigned int)Count);
             if (t5load_trace == 1)
             {
                DBG_LOG("[T5LOAD] code 0x80278504:\n");
                for (di = 0; di < 80; di++)
                   printf("  0x%08X: %08X\n", 0x80278504 + di * 4,
                      rdram ? rdram[((0x80278504 + di * 4) & 0xFFFFFF) / 4] : 0);
             }
          }
          if (r4300.pc >= 0x802469B8 && r4300.pc < 0x80247000 && t5_trace_cnt < 60000)
          {
             t5_was_in = 1;
             if ((t5_trace_cnt & 0xFFF) == 0)
                DBG_LOG("[T5TRACE] pc=%08X sp=%08X ra=%08X v0=%08X Count=%08X\n",
                   r4300.pc, (u32)r4300.gpr[29], (u32)r4300.gpr[31], (u32)r4300.gpr[2], (unsigned int)Count);
             t5_trace_cnt++;
          }
          else if (t5_was_in)
          {
             t5_was_in = 0;
             DBG_LOG("[T5EXIT] pc=%08X sp=%08X ra=%08X Count=%08X\n",
                r4300.pc, (u32)r4300.gpr[29], (u32)r4300.gpr[31], (unsigned int)Count);
          }
          if (r4300.pc == 0x80246CF0 && idle_dispatch_cnt < 10)
          {
             idle_dispatch_cnt++;
             DBG_LOG("[IDLEDISP#%d] pc=80246CF0 sp=%08X ra=%08X Count=%08X\n",
                idle_dispatch_cnt, (u32)r4300.gpr[29], (u32)r4300.gpr[31], (unsigned int)Count);
          }
          if (r4300.pc == 0x80246DD8 && !dumped_spin2_code)
          {
             int di;
             u32 fi;
             dumped_spin2_code = 1;
             DBG_LOG("[SPIN2CODE] around 0x80246DD8:\n");             for (di = 0; di < 144; di++)
                printf("  0x%08X: %08X\n", 0x80246C40 + di * 4,
                   rdram ? rdram[((0x80246C40 + di * 4) & 0xFFFFFF) / 4] : 0);
             DBG_LOG("[SPIN2CODE] fmt@0x802469B8 = \"");
             if (rdram)
             {
                for (fi = 0; fi < 128; fi++)
                {
                   u32 w = rdram[((0x802469B8 + fi) & 0xFFFFFF) / 4];
                   char c1 = (char)((w >> 24) & 0xFF);
                   char c2 = (char)((w >> 16) & 0xFF);
                   char c3 = (char)((w >> 8) & 0xFF);
                   char c4 = (char)(w & 0xFF);
                   if (c1) printf("%c", c1); else break;
                   if (c2) printf("%c", c2); else break;
                   if (c3) printf("%c", c3); else break;
                   if (c4) printf("%c", c4); else break;
                }
             }
             printf("\"\n");
             DBG_LOG("[SPIN2CODE] buf@0x8034A8E0 = \"");
             if (rdram)
             {
                for (fi = 0; fi < 128; fi++)
                {
                   u32 w = rdram[((0x8034A8E0 + fi) & 0xFFFFFF) / 4];
                   char c1 = (char)((w >> 24) & 0xFF);
                   char c2 = (char)((w >> 16) & 0xFF);
                   char c3 = (char)((w >> 8) & 0xFF);
                   char c4 = (char)(w & 0xFF);
                   if (c1) printf("%c", c1); else break;
                   if (c2) printf("%c", c2); else break;
                   if (c3) printf("%c", c3); else break;
                   if (c4) printf("%c", c4); else break;
                }
             }
             printf("\"\n");
          }
         if (r4300.pc == 0x80184588 && !dumped_frk_code)
         {
            int di;
            u32 base = 0x801844C0 & 0xFFFFFF;
            dumped_frk_code = 1;
            DBG_LOG("[FRKCODEDUMP] around 0x80184588:\n");
            for (di = 0; di < 64; di++)
            {
               printf("  0x%08X: %08X\n", 0x801844C0 + di * 4,
                  rdram ? rdram[(base + di * 4) / 4] : 0);
            }
         }
         if (r4300.pc == 0x80184588 && frk_cnt < 250)
         {
            u32 keyaddr = (u32)r4300.gpr[29] + 0x1C;
           u32 modeflag = rdram ? rdram[(0x801A8400 & 0xFFFFFF) / 4] : 0;
           char ks[25];
           int ki;
           u32 w;
           for (ki = 0; ki < 24; ki++)
           {
              w = rdram ? rdram[((keyaddr + ki) & 0xFFFFFF) / 4] : 0;
              ks[ki] = (char)((w >> ((3 - (ki & 3)) * 8)) & 0xFF);
              if (!ks[ki]) break;
           }
           ks[ki] = 0;
           DBG_LOG("[FRK#%u] key=\"%s\" mode=%08X a0=%08X ra=%08X Count=%08X\n",
              frk_cnt, ks, modeflag, rdram ? rdram[(((u32)r4300.gpr[29] + 0x120) & 0xFFFFFF) / 4] : 0, rdram ? rdram[(((u32)r4300.gpr[29] + 0x14) & 0xFFFFFF) / 4] : 0, (unsigned int)Count);
           frk_cnt++;
        }
       if (r4300.pc == 0x80317934 && !dumped_spin_code)
      {
         int di;
         u32 base = 0x80317900 & 0xFFFFFF;
         dumped_spin_code = 1;
         DBG_LOG("[CODEDUMP] around 0x80317934:\n");
         for (di = 0; di < 16; di++)
         {
            printf("  0x%08X: %08X\n", 0x80317900 + di * 4,
               rdram ? rdram[(base + di * 4) / 4] : 0);
         }
      }
      if (r4300.pc == 0x8018D540 && !dumped_fmt)
      {
         u32 fi, fmt;
         dumped_fmt = 1;
         DBG_LOG("[PRINTFEXIT] sp=%08X fmt_slot=%08X\n",
            (u32)r4300.gpr[29], (u32)(r4300.gpr[29] + 0x38));
         if (rdram && r4300.gpr[29])
         {
            fmt = (u32)rdram[(((u32)r4300.gpr[29] + 0x38) & 0xFFFFFF) / 4];
            DBG_LOG("[PRINTFEXIT] fmt=%08X text=\"", fmt);
            for (fi = 0; fi < 160; fi++)
            {
               u32 w = rdram[((fmt + fi) & 0xFFFFFF) / 4];
               char c1 = (char)((w >> 24) & 0xFF);
               char c2 = (char)((w >> 16) & 0xFF);
               char c3 = (char)((w >> 8) & 0xFF);
               char c4 = (char)(w & 0xFF);
               if (c1) printf("%c", c1); else break;
               if (c2) printf("%c", c2); else break;
               if (c3) printf("%c", c3); else break;
               if (c4) printf("%c", c4); else break;
            }
            printf("\"\n");
            for (fi = 0; fi < 12; fi++)
            {
               DBG_LOG("[PRINTFEXIT] arg%u = %08X\n", fi,
                  (unsigned int)rdram[(((u32)r4300.gpr[29] + 0x3C + fi * 4) & 0xFFFFFF) / 4]);
            }
         }
      }
      if (r4300.pc == 0x8019BB28 && !dumped_final_code)
      {
         int di;
         u32 base = 0x8019BB00 & 0xFFFFFF;
         dumped_final_code = 1;
         DBG_LOG("[CODEDUMP] around 0x8019BB28:\n");
         for (di = 0; di < 32; di++)
         {
            printf("  0x%08X: %08X\n", 0x8019BB00 + di * 4,
               rdram ? rdram[(base + di * 4) / 4] : 0);
         }
      }
       if (r4300.pc == 0x8019BB0C && !dumped_exit_entry)
       {
          u32 fi, fmt;
          dumped_exit_entry = 1;
          DBG_LOG("[EXITENTRY] pc=8019BB0C sp=%08X ra=%08X a0=%08X [sp+0x14]=%08X\n",
             (u32)r4300.gpr[29], (u32)r4300.gpr[31], (u32)r4300.gpr[4],
             rdram ? (u32)rdram[(((u32)r4300.gpr[29] + 0x14) & 0xFFFFFF) / 4] : 0);
          if (rdram)
          {
             int ci2;
             DBG_LOG("[CALLERDUMP] around 0x8018C000 (callsite 0x8018D540/0x8018C3D4):\n");
             for (ci2 = 0; ci2 < 1536; ci2++)
             {
                printf("  0x%08X: %08X\n", 0x8018C000 + ci2 * 4,
                   rdram[((0x8018C000 + ci2 * 4) & 0xFFFFFF) / 4]);
             }
             DBG_LOG("[CALLERREGS] v0=%08X v1=%08X a0=%08X a1=%08X a2=%08X a3=%08X t0=%08X t1=%08X\n",
                (u32)r4300.gpr[2], (u32)r4300.gpr[3], (u32)r4300.gpr[4],
                (u32)r4300.gpr[5], (u32)r4300.gpr[6], (u32)r4300.gpr[7],
                (u32)r4300.gpr[8], (u32)r4300.gpr[9]);
             DBG_LOG("[CALLERREGS] s0=%08X s1=%08X s2=%08X s3=%08X s4=%08X s5=%08X s6=%08X s7=%08X\n",
                (u32)r4300.gpr[16], (u32)r4300.gpr[17], (u32)r4300.gpr[18],
                (u32)r4300.gpr[19], (u32)r4300.gpr[20], (u32)r4300.gpr[21],
                (u32)r4300.gpr[22], (u32)r4300.gpr[23]);
          }
         if (rdram && r4300.gpr[29])
         {
            fmt = (u32)rdram[(((u32)r4300.gpr[29] + 0x38) & 0xFFFFFF) / 4];
            DBG_LOG("[EXITENTRY] fmt@[sp+0x38]=%08X text=\"", fmt);
            for (fi = 0; fi < 160; fi++)
            {
               u32 w = rdram[((fmt + fi) & 0xFFFFFF) / 4];
               char c1 = (char)((w >> 24) & 0xFF);
               char c2 = (char)((w >> 16) & 0xFF);
               char c3 = (char)((w >> 8) & 0xFF);
               char c4 = (char)(w & 0xFF);
               if (c1) printf("%c", c1); else break;
               if (c2) printf("%c", c2); else break;
               if (c3) printf("%c", c3); else break;
               if (c4) printf("%c", c4); else break;
            }
            printf("\"\n");
            for (fi = 0; fi < 12; fi++)
            {
               DBG_LOG("[EXITENTRY] arg%u = %08X\n", fi,
                  (unsigned int)rdram[(((u32)r4300.gpr[29] + 0x3C + fi * 4) & 0xFFFFFF) / 4]);
            }
          }
       }
       if (r4300.pc == 0x8019B158 && rdram && gfx_alloc_cnt < 64)
       {
          u32 gstruct = rdram[(0x801A888C & 0xFFFFFF) / 4];
          if (gstruct && (gstruct & 0xFFFFFF) + 0x30 < 0xFFFFFF)
          {
             u32 gcnt = rdram[((gstruct + 0x24) & 0xFFFFFF) / 4];
             u32 gmax = rdram[((gstruct + 0x28) & 0xFFFFFF) / 4];
             if (gcnt >= gmax - 24)
             {
                DBG_LOG("[GFXHW#%u] cnt=%u/%u ra=%08X sp=%08X v0=%08X a0=%08X Count=%08X triCnt=%08X triTab=%08X\n",
                   gfx_alloc_cnt, gcnt, gmax, (u32)r4300.gpr[31], (u32)r4300.gpr[29],
                   (u32)r4300.gpr[2], (u32)r4300.gpr[4], (unsigned int)Count,
                   (unsigned int)rdram[(0x801BB254 & 0xFFFFFF) / 4],
                   (unsigned int)rdram[(0x801BB25C & 0xFFFFFF) / 4]);
                gfx_alloc_cnt++;
             }
          }
       }
        if (r4300.pc == 0x801A0178 && rdram && trireset_cnt < 40)
        {
           trireset_cnt++;
           DBG_LOG("[TRIRESET#%u] triCnt=%08X vtxCnt=%08X triTab=%08X Count=%08X\n",
              trireset_cnt,
              (unsigned int)rdram[(0x801BB254 & 0xFFFFFF) / 4],
              (unsigned int)rdram[(0x801BB24C & 0xFFFFFF) / 4],
              (unsigned int)rdram[(0x801BB25C & 0xFFFFFF) / 4],
              (unsigned int)Count);
        }
        if (r4300.pc == 0x801A0030 && rdram && triadd_cnt < 60)
        {
           triadd_cnt++;
           DBG_LOG("[TRIADD#%u] triCnt=%08X vtxCnt=%08X Count=%08X\n",
              triadd_cnt,
              (unsigned int)rdram[(0x801BB254 & 0xFFFFFF) / 4],
              (unsigned int)rdram[(0x801BB24C & 0xFFFFFF) / 4],
              (unsigned int)Count);
        }
         if (r4300.pc == 0x801A0064 && rdram)
         {
            flushcall_cnt++;
            if (flushcall_cnt < 200)
               DBG_LOG("[FLUSHCALL#%u] triCnt=%08X vtxCnt=%08X Count=%08X\n",
                  flushcall_cnt,
                  (unsigned int)rdram[(0x801BB254 & 0xFFFFFF) / 4],
                  (unsigned int)rdram[(0x801BB24C & 0xFFFFFF) / 4],
                  (unsigned int)Count);
         }
         if (r4300.pc == 0x8019FC24 && rdram)
         {
            dedup_cmp_cnt++;
            if (dedup_cmp_cnt < 40 || (dedup_cmp_cnt % 2000) == 0)
               DBG_LOG("[DEDUPCMP#%u] stored=%04X cur=%04X vtxCnt=%08X triCnt=%08X Count=%08X\n",
                  dedup_cmp_cnt, (u32)(r4300.gpr[12] & 0xFFFF),
                  (u32)(r4300.gpr[8] & 0xFFFF),
                  (unsigned int)rdram[(0x801BB24C & 0xFFFFFF) / 4],
                  (unsigned int)rdram[(0x801BB254 & 0xFFFFFF) / 4],
                  (unsigned int)Count);
         }
         if (r4300.pc == 0x8019FCC4 && rdram)
        {
           append_hit_cnt++;
           if (append_hit_cnt < 30)
              DBG_LOG("[APPENDHIT#%u] vtxCnt=%08X triCnt=%08X Count=%08X\n",
                 append_hit_cnt,
                 (unsigned int)rdram[(0x801BB24C & 0xFFFFFF) / 4],
                 (unsigned int)rdram[(0x801BB254 & 0xFFFFFF) / 4],
                 (unsigned int)Count);
        }
         if (r4300.pc == 0x8019FD0C && rdram)
         {
            append_new_cnt++;
            if (append_new_cnt < 30)
               DBG_LOG("[APPENDNEW#%u] vtxCnt=%08X triCnt=%08X Count=%08X\n",
                  append_new_cnt,
                  (unsigned int)rdram[(0x801BB24C & 0xFFFFFF) / 4],
                  (unsigned int)rdram[(0x801BB254 & 0xFFFFFF) / 4],
                  (unsigned int)Count);
         }
#endif
         PC->ops();
      if ((s32)(r4300.next_interrupt - Count) <= 0)
      {
         u32 pc_before_int = r4300.pc;
         r4300.pc = PC->addr;
         gen_interrupt();
         if (r4300.pc != pc_before_int)
            cached_interpreter_jump_to(r4300.pc);
      }
#ifdef CACHED_DEBUG
      if ((++spin_cnt & 0x7FFFFF) == 0)
         DBG_LOG("[SPIN] pc=%08X Count=%08X ni=%08X vi=%d mi=%08X Status=%08X Cause=%08X\n",
            r4300.pc, (unsigned int)Count, r4300.next_interrupt, dbg_vi_count,
            MI_register.mi_intr_reg, (u32)Status, (u32)Cause);
      if (!dumped_final_code && (r4300.pc == 0x8019BB24 || r4300.pc == 0x8019BB28))
      {
         int di;
         u32 base = 0x8019BB00 & 0xFFFFFF;
         dumped_final_code = 1;
         DBG_LOG("[CODEDUMP2] around 0x8019BB24/0x8019BB28 (exit spin):\n");
            for (di = 0; di < 32; di++)
            {
               printf("  0x%08X: %08X\n", 0x8019BB00 + di * 4,
                  rdram ? rdram[(base + di * 4) / 4] : 0);
            }
            DBG_LOG("[ASSERTSTR] 0x801B8ED8: ");
            if (rdram)
            {
               u32 ai;
               for (ai = 0; ai < 128; ai++)
               {
                  u32 w = rdram[((0x801B8ED8 + ai) & 0xFFFFFF) / 4];
                  char c1 = (char)((w >> 24) & 0xFF);
                  char c2 = (char)((w >> 16) & 0xFF);
                  char c3 = (char)((w >> 8) & 0xFF);
                  char c4 = (char)(w & 0xFF);
                  if (c1) printf("%c", c1); else break;
                  if (c2) printf("%c", c2); else break;
                  if (c3) printf("%c", c3); else break;
                  if (c4) printf("%c", c4); else break;
               }
            }
            printf("\n[ASSERTCTX] sp=%08X ra=%08X a0=%08X a1=%08X a2=%08X v0=%08X\n",
               (u32)r4300.gpr[29], (u32)r4300.gpr[31],
               (u32)r4300.gpr[4], (u32)r4300.gpr[5], (u32)r4300.gpr[6], (u32)r4300.gpr[2]);
            if (rdram)
               DBG_LOG("[GFPOOL] ptr=%08X end=%08X remain=%d\n",
                  (unsigned int)rdram[(0x801A882C & 0xFFFFFF) / 4],
                  (unsigned int)rdram[(0x801A8828 & 0xFFFFFF) / 4],
                  (int)((unsigned int)rdram[(0x801A8828 & 0xFFFFFF) / 4] -
                        (unsigned int)rdram[(0x801A882C & 0xFFFFFF) / 4]));
            if (rdram && r4300.gpr[29])
            {
               u32 si, saddr = (u32)r4300.gpr[29] & 0xFFFFFF;
               DBG_LOG("[STACKDUMP] 0x%08X (saved ra at sp+0x14 = 0x%08X):",
                  (u32)r4300.gpr[29], (u32)(r4300.gpr[29] + 0x14));
               for (si = 0; si < 32; si++)
               {
                  if ((si & 7) == 0) printf("\n  ");
                  printf("%08X ", (unsigned int)rdram[(saddr + si * 4) / 4]);
               }
               printf("\n");
            }
            if (rdram)
            {
               u32 si2;
               DBG_LOG("[PFRAME] printf frame 0x80206940..0x80206C20 (caller_ra@0x80206974 fmt@0x80206990 vargs@0x80206984):");
               for (si2 = 0; si2 < 184; si2++)
               {
                  if ((si2 & 7) == 0) printf("\n  ");
                  printf("%08X ", (unsigned int)rdram[((0x80206940 + si2 * 4) & 0xFFFFFF) / 4]);
               }
               printf("\n");
            }
            if (rdram)
            {
               u32 rr;
               u32 regions[2] = { 0x801B8ED8, 0x801C8ED8 };
               for (rr = 0; rr < 2; rr++)
               {
                  u32 bi, baddr = regions[rr];
                  DBG_LOG("[HEXDUMP] 0x%08X:", baddr);
                  for (bi = 0; bi < 64; bi++)
                  {
                     if ((bi & 15) == 0) printf("\n  ");
                     printf("%02X ", (unsigned int)(rdram[((baddr + bi) & 0xFFFFFF) / 4] >> (24 - 8 * (bi & 3))) & 0xFF);
                  }
                   printf("\n");
                }
             }
             if (rdram)
             {
                u32 bi, baddr = 0x801B70E0;
                DBG_LOG("[RAMDUMP] gd strings 0x801B70E0..0x801B72A0:");
                for (bi = 0; bi < 448; bi++)
                {
                   if ((bi & 15) == 0) printf("\n  %08X: ", baddr + bi);
                   printf("%02X ", (unsigned int)(rdram[((baddr + bi) & 0xFFFFFF) / 4] >> (24 - 8 * (bi & 3))) & 0xFF);
                }
                printf("\n");
             }
             if (rdram)
             {
                u32 bi, baddr = 0x80206BA0;
                DBG_LOG("[EXFRAME] exit frame 0x80206BA0..0x80206C80:");
                for (bi = 0; bi < 224; bi++)
                {
                   if ((bi & 7) == 0) printf("\n  ");
                   printf("%08X ", (unsigned int)rdram[((baddr + bi) & 0xFFFFFF) / 4]);
                }
                printf("\n");
             }
             if (rdram)
             {
                u32 bi, baddr = 0x80000A80;
                DBG_LOG("[ARG1TGT] 0x80000A80..0x80000B00 (arg1=0x80000A84):");
                for (bi = 0; bi < 128; bi++)
                {
                   if ((bi & 15) == 0) printf("\n  ");
                   printf("%02X ", (unsigned int)(rdram[((baddr + bi) & 0xFFFFFF) / 4] >> (24 - 8 * (bi & 3))) & 0xFF);
                }
                 printf("\n");
              }
           if ((spin_cnt & 0xFFFFFF) == 0)
              dbg_dump_queue();
            DBG_LOG("[FINALSTOP] pc=%08X Count=%08X ni=%08X\n", r4300.pc,
               (unsigned int)Count, r4300.next_interrupt);
            r4300.stop = 1;
         }
#endif
     }
  }

void cached_interp_TLB_REFILL(void)
{
   u32 vaddr = PC->addr;
   TLB_refill_exception(vaddr, 2);
   if (ci.invalid_code[r4300.pc >> 12])
      cached_interp_init_block(r4300.pc);
   ci.actual = ci.blocks[r4300.pc >> 12];
   if (!ci.actual || !ci.actual->block)
   {
      DBG_LOG("[TLBREF_STUCK] vaddr=%08X vector=%08X Count=%08X\n", vaddr, (unsigned int)r4300.pc, (unsigned int)Count); fflush(stdout);
      DBG_LOG("TLB_REFILL stuck at 0x%08x (vector 0x%08x)\n", vaddr, r4300.pc);
      r4300.stop = 1;
      return;
   }
   PC = ci.actual->block + ((r4300.pc - ci.actual->start) >> 2);
   PC->ops();
}

void cached_interp_FIN_BLOCK(void)
{
   u32 addr = (PC - 1)->addr + 4;
   precomp_instr* old = PC;
   generic_jump_to(addr);
   if (PC == old)
   {
      DBG_LOG("[FINBLOCK_STUCK] addr=%08X Count=%08X\n", addr, (unsigned int)Count); fflush(stdout);
      DBG_LOG("FIN_BLOCK stuck at 0x%08x\n", addr);
      r4300.stop = 1;
      return;
   }
   PC->ops();
}

void cached_interp_NOTCOMPILED(void)
{
   u32 addr = PC->addr;
   cached_interp_recompile_block(addr);
   if (PC->ops == cached_interp_NOTCOMPILED)
   {
      DBG_LOG("[NOTCOMPILED_STUCK] addr=%08X Count=%08X\n", addr, (unsigned int)Count); fflush(stdout);
      DBG_LOG("NOTCOMPILED stuck at 0x%08x\n", addr);
      r4300.stop = 1;
      return;
   }
   PC->ops();
}

void cached_interp_NOTCOMPILED2(void)
{
   cached_interp_NOTCOMPILED();
}

void cached_interp_NI(void)
{
   u32 iw = read_inst(PC->addr);
   DBG_LOG("[NI] at 0x%08x (iw=0x%08x op=%d funct=0x%02x) Count=%08X\n",
          PC->addr, iw, (iw >> 26) & 0x3F, iw & 0x3F, (unsigned int)Count);
   fflush(stdout);
   r4300.stop = 1;
   PC++;
}

void cached_interp_NOP(void)
{
   PC++;
}