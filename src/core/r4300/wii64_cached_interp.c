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

extern u32 op;
extern tlb tlb_e[32];
extern u32 *tlb_LUT_r;

#define RDRAM_WORDS 0x100000

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
void cached_interp_CVT_D_S(void); void cached_interp_CVT_D_W(void);
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
      if (!b) return;
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
      if (!b->block) return;
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
               switch (rt)
               {
                  case 0x00: inst->ops = cached_interp_BC1F; break;
                  case 0x02: inst->ops = cached_interp_BC1T; break;
                  case 0x04: inst->ops = cached_interp_BC1FL; break;
                  case 0x06: inst->ops = cached_interp_BC1TL; break;
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
                  case 0x21: inst->ops = (fmt == 16) ? cached_interp_CVT_D_S : cached_interp_CVT_D_W; break;
                  case 0x24: inst->ops = (fmt == 16) ? cached_interp_CVT_W_S : cached_interp_CVT_W_D; break;
                  case 0x25: inst->ops = (fmt == 16) ? cached_interp_CVT_L_S : cached_interp_CVT_L_D; break;
                  case 0x20: inst->ops = (fmt == 17) ? cached_interp_CVT_S_D : cached_interp_NI; break;
                  case 0x32: inst->ops = (fmt == 16) ? cached_interp_CVT_S_W : cached_interp_NI; break;
                  case 0x33: inst->ops = (fmt == 16) ? cached_interp_CVT_S_L : cached_interp_NI; break;
                  case 0x30: inst->ops = (fmt == 16) ? cached_interp_TRUNC_W_S : cached_interp_TRUNC_W_D; break;
                  case 0x31: inst->ops = (fmt == 16) ? cached_interp_TRUNC_L_S : cached_interp_TRUNC_L_D; break;
                  case 0x28: inst->ops = (fmt == 16) ? cached_interp_CEIL_W_S : cached_interp_CEIL_W_D; break;
                  case 0x29: inst->ops = (fmt == 16) ? cached_interp_CEIL_L_S : cached_interp_CEIL_L_D; break;
                  case 0x34: inst->ops = (fmt == 16) ? cached_interp_FLOOR_W_S : cached_interp_FLOOR_W_D; break;
                  case 0x35: inst->ops = (fmt == 16) ? cached_interp_FLOOR_L_S : cached_interp_FLOOR_L_D; break;
                  case 0x22: inst->ops = (fmt == 16) ? cached_interp_ROUND_W_S : cached_interp_ROUND_W_D; break;
                  case 0x23: inst->ops = (fmt == 16) ? cached_interp_ROUND_L_S : cached_interp_ROUND_L_D; break;
                  case 0x38:
                  case 0x39:
                  case 0x3A:
                  case 0x3B:
                  case 0x3C:
                  case 0x3D:
                  case 0x3E:
                  case 0x3F:
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
                     inst->ops = (fmt == 16) ? c_cmp[funct & 7] : c_cmp_d[funct & 7];
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

void run_cached_interpreter(void)
{
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
      if (!PC) { r4300.stop = 1; break; }
      if (ci.actual && ci.actual->block)
      {
         u32 length = (ci.actual->end - ci.actual->start) / 4;
         precomp_instr* block_end = ci.actual->block + length + 1 + (length >> 2);
         if (PC < ci.actual->block || PC >= block_end)
         {
            r4300.stop = 1;
            break;
         }
      }
      if (!PC->ops)
      {
         r4300.stop = 1;
         break;
      }
      r4300.pc = PC->addr;
      PC->ops();
      if (r4300.next_interrupt <= Count)
      {
         u32 pc_before_int = r4300.pc;
         gen_interrupt();
         if (r4300.pc != pc_before_int)
            cached_interpreter_jump_to(r4300.pc);
      }
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
      printf("TLB_REFILL stuck at 0x%08x (vector 0x%08x)\n", vaddr, r4300.pc);
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
      printf("FIN_BLOCK stuck at 0x%08x\n", addr);
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
      printf("NOTCOMPILED stuck at 0x%08x\n", addr);
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
   printf("NI at 0x%08x (iw=0x%08x op=%d rs=%d rt=%d rd=%d funct=0x%02x)\n",
          PC->addr, iw, (iw >> 26) & 0x3F, (iw >> 21) & 0x1F,
          (iw >> 16) & 0x1F, (iw >> 11) & 0x1F, iw & 0x3F);
   r4300.stop = 1;
   PC++;
}

void cached_interp_NOP(void)
{
   PC++;
}