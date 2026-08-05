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
#include "../../main/ROM-Cache.h"

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
void cached_interp_CVT_D_L(void);
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
                  case 0x21:
                     inst->ops = (fmt == 16) ? cached_interp_CVT_D_S :
                                 (fmt == 20) ? cached_interp_CVT_D_W :
                                 (fmt == 21) ? cached_interp_CVT_D_L : cached_interp_NI;
                     break;
                  case 0x24: inst->ops = (fmt == 16) ? cached_interp_CVT_W_S : cached_interp_CVT_W_D; break;
                  case 0x25: inst->ops = (fmt == 16) ? cached_interp_CVT_L_S : cached_interp_CVT_L_D; break;
                  case 0x20:
                     inst->ops = (fmt == 17) ? cached_interp_CVT_S_D :
                                 (fmt == 20) ? cached_interp_CVT_S_W :
                                 (fmt == 21) ? cached_interp_CVT_S_L : cached_interp_NI;
                     break;
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
      r4300.last_pc = r4300.pc;
      {
        static int dbgTraceOn = 0; static int dbgTraceCnt = 0;
        static int decompRaw = 0, decompRepeat = 0, decompTotalBytes = 0;
        static int tokLogDone = 0;
        static int reloadLogCnt = 0;
        static int firstEntryCnt = 0;
        if (r4300.pc == 0x8027F500 && firstEntryCnt < 10) {
          firstEntryCnt++;
          u32 fa0=(u32)r4300.gpr[4], fa1=(u32)r4300.gpr[5], fa2=(u32)r4300.gpr[6];
          u32 fa3=(u32)r4300.gpr[7], ft0=(u32)r4300.gpr[8], ft8=(u32)r4300.gpr[24];
          u32 ft9=(u32)r4300.gpr[25];
          printf("[FIRST#%d] a0=%08X a1=%08X a2=%08X a3=%08X t0=%08X t8=%08X t9=%08X\n",
            firstEntryCnt, fa0, fa1, fa2, fa3, ft0, ft8, ft9);
          printf("[FIRST#%d] output range: %08X to %08X (%d bytes)\n",
            firstEntryCnt, fa1, ft8, (int)(ft8 - fa1));
          printf("[FIRST#%d] bitfield@%08X: ", firstEntryCnt, fa0);
          { int i; for (i=0; i<4; i++) {
            u32 addr=fa0+i*4;
            u32 v=(rdramb[(addr&0xFFFFFF)]<<24)|(rdramb[(addr&0xFFFFFF)+1]<<16)|(rdramb[(addr&0xFFFFFF)+2]<<8)|rdramb[(addr&0xFFFFFF)+3];
            printf("%08X ", v);
          } printf("\n"); }
          printf("[FIRST#%d] tokens@%08X: ", firstEntryCnt, fa3);
          { int i; for (i=0; i<16; i++) {
            u32 addr=fa3+i*2;
            printf("%04X ", (rdramb[(addr&0xFFFFFF)]<<8)|rdramb[(addr&0xFFFFFF)+1]);
          } printf("\n"); }
        }
        if (r4300.pc >= 0x8027F500 && r4300.pc <= 0x8027F57F) {
          if (r4300.pc == 0x8027F528) { decompRaw++; decompTotalBytes++; }
          if (r4300.pc == 0x8027F55C) { decompRepeat++; decompTotalBytes++; }
          if (r4300.pc == 0x8027F508 && (u32)r4300.gpr[5] > (u32)r4300.gpr[24] - 0x40 && reloadLogCnt < 20) {
            u32 a0val = (u32)r4300.gpr[4];
            u32 rWord = (rdramb[(a0val&0xFFFFFF)]<<24)|(rdramb[(a0val&0xFFFFFF)+1]<<16)|(rdramb[(a0val&0xFFFFFF)+2]<<8)|rdramb[(a0val&0xFFFFFF)+3];
            printf("[RELOAD] PRE-lw: a0=%08X word@mem=%08X a2=%d a1=%08X t0=%08X\n", a0val, rWord, (u32)r4300.gpr[6]&0xFF, (u32)r4300.gpr[5], (u32)r4300.gpr[8]);
          }
          if (r4300.pc == 0x8027F50C && (u32)r4300.gpr[5] > (u32)r4300.gpr[24] - 0x40 && reloadLogCnt < 20) {
            reloadLogCnt++;
            printf("[RELOAD] POST-lw: t0=%08X a0=%08X a2=%d a1=%08X\n", (u32)r4300.gpr[8], (u32)r4300.gpr[4], (u32)r4300.gpr[6]&0xFF, (u32)r4300.gpr[5]);
          }
          if (!dbgTraceOn && (u32)r4300.gpr[5] > (u32)r4300.gpr[24] - 0x40) { dbgTraceOn = 1; printf("[TRACE] near end a1=%08X t8=%08X a3=%08X t9=%08X a0=%08X a2=%08X t0=%08X\n", (u32)r4300.gpr[5], (u32)r4300.gpr[24], (u32)r4300.gpr[7], (u32)r4300.gpr[25], (u32)r4300.gpr[4], (u32)r4300.gpr[6], (u32)r4300.gpr[8]); }
          if (dbgTraceOn && dbgTraceCnt < 1500) { dbgTraceCnt++; printf("[TR] pc=%08X t0=%08X a2=%02X a1=%08X a3=%08X t9=%08X t1=%08X t2=%08X t3=%08X\n", r4300.pc, (u32)r4300.gpr[8], (u32)r4300.gpr[6]&0xFF, (u32)r4300.gpr[5], (u32)r4300.gpr[7], (u32)r4300.gpr[25], (u32)r4300.gpr[9], (u32)r4300.gpr[10], (u32)r4300.gpr[11]); }
          if (r4300.pc == 0x8027F574 && (u32)r4300.gpr[5] > (u32)r4300.gpr[24] - 0x40) {
            printf("[BYTECOUNT] raw=%d repeat=%d total=%d a1=%08X t8=%08X delta=%d\n", decompRaw, decompRepeat, decompTotalBytes, (u32)r4300.gpr[5], (u32)r4300.gpr[24], (u32)r4300.gpr[5] - (0x80064F80));
          }
          if (!tokLogDone && r4300.pc == 0x8027F500 && (u32)r4300.gpr[5] > (u32)r4300.gpr[24] - 0x40 && (u32)r4300.gpr[5] < (u32)r4300.gpr[24]) {
            tokLogDone = 1;
            u32 a3start = (u32)r4300.gpr[7];
            u32 a0val = (u32)r4300.gpr[4];
            int i;
            printf("[TOKDUMP] a1=%08X t8=%08X a3=%08X a0=%08X t0=%08X a2=%d\n", (u32)r4300.gpr[5], (u32)r4300.gpr[24], a3start, a0val, (u32)r4300.gpr[8], (u32)r4300.gpr[6]&0xFF);
            printf("[TOKDUMP] bitfield word at a0:\n");
            for (i = 0; i < 4; i++) {
              u32 addr = a0val + i*4;
              u32 val = (rdramb[(addr&0xFFFFFF)]<<24)|(rdramb[(addr&0xFFFFFF)+1]<<16)|(rdramb[(addr&0xFFFFFF)+2]<<8)|rdramb[(addr&0xFFFFFF)+3];
              printf("[TOKDUMP]   %08X: %08X\n", addr, val);
            }
            printf("[TOKDUMP] token stream at a3 (32 halfwords):\n");
            for (i = 0; i < 32; i++) {
              u32 addr = a3start + i*2;
              u16 tok = (rdramb[(addr&0xFFFFFF)]<<8)|rdramb[(addr&0xFFFFFF)+1];
              printf("[TOKDUMP]   %08X: %04X (cnt=%d dist=%d)\n", addr, tok, (tok>>12)+3, tok&0xFFF);
            }
          }
        }
        if (r4300.pc == 0x8027F574 && (u32)r4300.gpr[5] > (u32)r4300.gpr[24]) {
          printf("[OVERSHOOT] a1=%08X t8=%08X raw=%d repeat=%d total=%d\n", (u32)r4300.gpr[5], (u32)r4300.gpr[24], decompRaw, decompRepeat, decompTotalBytes);
        }
        static int dbgInsns = 0; static int dbgDumpDone = 0;
        if (dbgInsns < 20000000 && (dbgInsns++ % 100000) == 0) {
          if ((r4300.pc >= 0x8027F500 && r4300.pc <= 0x8027F580) || (r4300.pc >= 0x80327650 && r4300.pc <= 0x80327700)) {
            u32 ctxEp = 0x803673CC & 0xFFFFFF;
            u32 savedEpc = (rdramb[ctxEp]<<24)|(rdramb[ctxEp+1]<<16)|(rdramb[ctxEp+2]<<8)|rdramb[ctxEp+3];
            printf("[CIP] pc=%08X instr=%08X a1=%08X t8=%08X a3=%08X t1=%08X t0=%08X EPCret=%08X Count=%08X next=%08X\n", r4300.pc, read_inst(r4300.pc), (u32)r4300.gpr[5], (u32)r4300.gpr[24], (u32)r4300.gpr[7], (u32)r4300.gpr[9], (u32)r4300.gpr[8], savedEpc, (unsigned int)Count, r4300.next_interrupt);
          }
          if (!dbgDumpDone && r4300.pc >= 0x8027F500 && r4300.pc <= 0x8027F580 && (u32)r4300.gpr[5] > (u32)r4300.gpr[24]) {
            dbgDumpDone = 1; int i;
            printf("[DUMP] a1=%08X t8=%08X src=%08X\n", (u32)r4300.gpr[5], (u32)r4300.gpr[24], (u32)r4300.gpr[7]);
            u8* bp = (u8*)(&rdramb[((r4300.gpr[7] & 0xFFFFFF) - 0x40)]);
            for (i = 0; i < 96; i++) printf("%02X%s", bp[i], ((i+1)%16==0)?"\n":" ");
            printf("[DUMP] tokenbuf start 0x801B6870:\n"); bp = (u8*)(&rdramb[0x1B6870]);
            for (i = 0; i < 64; i++) printf("%02X%s", bp[i], ((i+1)%16==0)?"\n":" ");
            printf("[DUMP] code 0x8027F4F0:\n"); { u8* cp = (u8*)(&rdramb[0x27F4F0 & 0xFFFFFF]); int j;
              for (j = 0; j < 48; j++) printf("%08X%s", ((cp[j*4]<<24)|(cp[j*4+1]<<16)|(cp[j*4+2]<<8)|cp[j*4+3]), ((j+1)%4==0)?"\n":" "); }
            printf("[DUMP] tokens@0x801DD000 (near end a3):\n"); { u8* bp2 = (u8*)(&rdramb[0x1DD000]); int j; for (j = 0; j < 64; j++) printf("%02X%s", bp2[j], ((j+1)%16==0)?"\n":" "); }
            printf("[DUMP] tokens@0x801BDD00 (mid stream):\n"); { u8* bp3 = (u8*)(&rdramb[0x1BDD00]); int j; for (j = 0; j < 32; j++) printf("%02X%s", bp3[j], ((j+1)%16==0)?"\n":" "); }
          }
        }
        /* Coarse PC sampler: log every 200000 insns, max 80 samples (BUILD 00138, moved outside dbgInsns block) */
        {
          static int pcSamplerInsns = 0;
          static int pcSamplerCnt = 0;
          if (pcSamplerInsns++ % 200000 == 0 && pcSamplerCnt < 80) {
            pcSamplerCnt++;
            printf("[PCS] pc=%08X instr=%08X sp=%08X a0=%08X a1=%08X ra=%08X Count=%08X\n",
              r4300.pc, read_inst(r4300.pc), (u32)r4300.gpr[29], (u32)r4300.gpr[4], (u32)r4300.gpr[5],
              (u32)r4300.gpr[31], (unsigned int)Count);
          }
        }
      }
      /* === BUILD 00140: relocate __osIntOffTable/__osIntTable from VMA 0x80339980
         (rom 0xF4980, where the 1MB cart DMA put them) to 0x80349980, the address
         the exception handler dispatch reads (lui at,0x8034 + lbu 0x9980 / lw 0x99A0).
         The ESP translation's data segment sits 0x10000 below its text expectations. === */
      {
        static int intTableRelocated = 0;
        if (!intTableRelocated && rdramb && r4300.pc >= 0x80327640 && r4300.pc <= 0x80327EC0) {
          intTableRelocated = 1;
          ROMCache_read(&rdramb[0x349980], 0xF4980, 0x44);
          printf("[INTFIX] __osIntOffTable/__osIntTable: rom 0xF4980 -> RDRAM 0x80349980 (0x44 bytes)\n");
          printf("[INTFIX] intTable targets: %08X %08X %08X %08X %08X %08X %08X %08X %08X\n",
            ((u32)rdramb[0x3499A0]<<24)|(rdramb[0x3499A1]<<16)|(rdramb[0x3499A2]<<8)|rdramb[0x3499A3],
            ((u32)rdramb[0x3499A4]<<24)|(rdramb[0x3499A5]<<16)|(rdramb[0x3499A6]<<8)|rdramb[0x3499A7],
            ((u32)rdramb[0x3499A8]<<24)|(rdramb[0x3499A9]<<16)|(rdramb[0x3499AA]<<8)|rdramb[0x3499AB],
            ((u32)rdramb[0x3499AC]<<24)|(rdramb[0x3499AD]<<16)|(rdramb[0x3499AE]<<8)|rdramb[0x3499AF],
            ((u32)rdramb[0x3499B0]<<24)|(rdramb[0x3499B1]<<16)|(rdramb[0x3499B2]<<8)|rdramb[0x3499B3],
            ((u32)rdramb[0x3499B4]<<24)|(rdramb[0x3499B5]<<16)|(rdramb[0x3499B6]<<8)|rdramb[0x3499B7],
            ((u32)rdramb[0x3499B8]<<24)|(rdramb[0x3499B9]<<16)|(rdramb[0x3499BA]<<8)|rdramb[0x3499BB],
            ((u32)rdramb[0x3499BC]<<24)|(rdramb[0x3499BD]<<16)|(rdramb[0x3499BE]<<8)|rdramb[0x3499BF],
            ((u32)rdramb[0x3499C0]<<24)|(rdramb[0x3499C1]<<16)|(rdramb[0x3499C2]<<8)|rdramb[0x3499C3]);
        }
      }
      /* === BUILD 00129: SP corruption detector + interrupt trace + vector dump === */
      {
        static int bgeuHackApplied = 0;
        static int codeHexDumpDone = 0;

        /* --- Dump exception vector at 0x80000180 and delay slot at 0x80246DDC --- */
        static int vectorDumpDone = 0;
        if (!vectorDumpDone && r4300.pc >= 0x80246000 && r4300.pc <= 0x80247000) {
          vectorDumpDone = 1;
          int i;
          printf("[VECTORDUMP] Exception vector at 0x80000180 (8 words):\n");
          for (i = 0; i < 8; i++) {
            u32 addr = 0x80000180 + i * 4;
            u32 insn = read_inst(addr);
            printf("[VECTORDUMP]   %08X: %08X\n", addr, insn);
          }
          printf("[VECTORDUMP] Main region 0x80246000-0x80247000 (1024 words):\n");
          for (i = 0; i < 1024; i++) {
            u32 addr = 0x80246000 + i * 4;
            u32 insn = read_inst(addr);
            printf("[VECTORDUMP]   %08X: %08X\n", addr, insn);
          }
          /* Also dump what's at 0x80327640-0x80327A40 (handler area, BUILD 00132: extended) */
          printf("[VECTORDUMP] Handler area 0x80327640-0x80327A40 (256 words):\n");
          for (i = 0; i < 256; i++) {
            u32 addr = 0x80327640 + i * 4;
            u32 insn = read_inst(addr);
            printf("[VECTORDUMP]   %08X: %08X\n", addr, insn);
          }
          /* Dump the OSTask struct in SP DMEM 0xFC0 (BUILD 00135: RSP task probe) */
          printf("[VECTORDUMP] OSTask @ SP_DMEM 0xFC0 (32 words):\n");
          for (i = 0; i < 32; i++) {
            u32 addr = 0xFC0 + i * 4;
            u32 val = SP_DMEM[addr / 4];
            printf("[VECTORDUMP]   %08X: %08X\n", addr, val);
          }
        }

        /* === BUILD 00136: identify 0x803236F0 (idle pre-spin call) + main entry === */
        {
          static int libDumpDone = 0;
          if (!libDumpDone && r4300.pc >= 0x80246000 && r4300.pc <= 0x80247000) {
            libDumpDone = 1;
            int i;
            printf("[VECTORDUMP] libultra 0x80323500-0x80323800 (192 words):\n");
            for (i = 0; i < 192; i++) {
              u32 addr = 0x80323500 + i * 4;
              u32 insn = read_inst(addr);
              printf("[VECTORDUMP]   %08X: %08X\n", addr, insn);
            }
            /* BUILD 00138/00146: dump the exception dispatch tables. CORRECTION (00146):
               the handler reads them at 0x80339980/0x803399A0 (lui at,0x8034 + 0xFFFF9980/99A0),
               NOT 0x80349980 where INTFIX (00140) wrote. */
            printf("[VECTORDUMP] __osIntOffTable @ 0x80339980 (32 bytes):\n");
            for (i = 0; i < 32; i++) {
              u32 addr = 0x80339980 + i;
              u8 b = rdramb[addr & 0xFFFFFF];
              printf("[VECTORDUMP]   %08X: %02X\n", addr, b);
            }
            printf("[VECTORDUMP] __osIntTable @ 0x803399A0 (18 words):\n");
            for (i = 0; i < 18; i++) {
              u32 addr = 0x803399A0 + i * 4;
              u32 v = (rdramb[(addr)&0xFFFFFF]<<24)|(rdramb[(addr+1)&0xFFFFFF]<<16)|(rdramb[(addr+2)&0xFFFFFF]<<8)|rdramb[(addr+3)&0xFFFFFF];
              printf("[VECTORDUMP]   %08X: %08X\n", addr, v);
            }
            {
              u8 off10 = rdramb[(0x80339980+0x10) & 0xFFFFFF];
              u32 addr = 0x803399A0 + off10;
              u32 tgt = (rdramb[(addr)&0xFFFFFF]<<24)|(rdramb[(addr+1)&0xFFFFFF]<<16)|(rdramb[(addr+2)&0xFFFFFF]<<8)|rdramb[(addr+3)&0xFFFFFF];
              printf("[VECTORDUMP] index0x10: off=%02X target=%08X (resume-check=80327B18, resched=80327D68, suspend=80327B68)\n", off10, tgt);
            }
            printf("[VECTORDUMP] INTFIX copy @ 0x80349980 (17 words):\n");
            for (i = 0; i < 17; i++) {
              u32 addr = 0x80349980 + i * 4;
              u32 v = (rdramb[(addr)&0xFFFFFF]<<24)|(rdramb[(addr+1)&0xFFFFFF]<<16)|(rdramb[(addr+2)&0xFFFFFF]<<8)|rdramb[(addr+3)&0xFFFFFF];
              printf("[VECTORDUMP]   %08X: %08X\n", addr, v);
            }
            printf("[VECTORDUMP] handler dispatch 0x803278A0-0x80327A40 (96 words):\n");
            for (i = 0; i < 96; i++) {
              u32 addr = 0x803278A0 + i * 4;
              u32 insn = read_inst(addr);
              printf("[VECTORDUMP]   %08X: %08X\n", addr, insn);
            }
            printf("[VECTORDUMP] handler redispatch 0x80327A40-0x80327EC0 (160 words):\n");
            for (i = 0; i < 160; i++) {
              u32 addr = 0x80327A40 + i * 4;
              u32 insn = read_inst(addr);
              printf("[VECTORDUMP]   %08X: %08X\n", addr, insn);
            }
            printf("[VECTORDUMP] flag area 0x80335A30-0x80335B60 (76 words):\n");
            for (i = 0; i < 76; i++) {
              u32 addr = 0x80335A30 + i * 4;
              u32 v = (rdramb[(addr)&0xFFFFFF]<<24)|(rdramb[(addr+1)&0xFFFFFF]<<16)|(rdramb[(addr+2)&0xFFFFFF]<<8)|rdramb[(addr+3)&0xFFFFFF];
              printf("[VECTORDUMP]   %08X: %08X\n", addr, v);
            }
          }

          /* Dynamic trace of 0x803236F0: log first 120 instructions once entered */
          {
            static int idleFnTrace = 0;
            static int idleFnLog = 0;
            if (idleFnTrace == 0 && r4300.pc == 0x803236F0) {
              idleFnTrace = 1;
              printf("[IDLEFN] entered 0x803236F0 a0=%08X a1=%08X a2=%08X a3=%08X ra=%08X sp=%08X Count=%08X\n",
                (u32)r4300.gpr[4], (u32)r4300.gpr[5], (u32)r4300.gpr[6], (u32)r4300.gpr[7],
                (u32)r4300.gpr[31], (u32)r4300.gpr[29], (unsigned int)Count);
            }
            if (idleFnTrace == 1 && idleFnLog < 120) {
              idleFnLog++;
              printf("[IDLEFN] pc=%08X instr=%08X a0=%08X a1=%08X t0=%08X t1=%08X t2=%08X t3=%08X ra=%08X sp=%08X Count=%08X\n",
                r4300.pc, read_inst(r4300.pc),
                (u32)r4300.gpr[4], (u32)r4300.gpr[5],
                (u32)r4300.gpr[8], (u32)r4300.gpr[9], (u32)r4300.gpr[10], (u32)r4300.gpr[11],
                (u32)r4300.gpr[31], (u32)r4300.gpr[29], (unsigned int)Count);
            }
            if (idleFnLog >= 120) idleFnTrace = 2;
          }

          /* Log when main thread entry (0x802469B8) first executes */
          {
            static int mainEntryLogged = 0;
            if (!mainEntryLogged && r4300.pc == 0x802469B8) {
              mainEntryLogged = 1;
              printf("[MAINTHREAD] main thread entered! sp=%08X ra=%08X a0=%08X a1=%08X t0=%08X Count=%08X\n",
                (u32)r4300.gpr[29], (u32)r4300.gpr[31], (u32)r4300.gpr[4], (u32)r4300.gpr[5],
                (u32)r4300.gpr[8], (unsigned int)Count);
            }
          }

          /* Log osStartThread (0x80322DF0) entry with its thread pointer */
          {
            static int startThreadCnt = 0;
            if (startThreadCnt < 8 && r4300.pc == 0x80322DF0) {
              startThreadCnt++;
              printf("[STHREAD#%d] osStartThread tcb=%08X a1=%08X Count=%08X\n",
                startThreadCnt, (u32)r4300.gpr[4], (u32)r4300.gpr[5], (unsigned int)Count);
            }
          }

          /* Log real osCreateThread (0x803226B0) entry with its args */
          {
            static int createThreadCnt = 0;
            if (createThreadCnt < 8 && r4300.pc == 0x803226B0) {
              createThreadCnt++;
              u32 spb = ((u32)r4300.gpr[29]) & 0xFFFFFF;
              u32 stacktop = (rdramb[spb+16]<<24)|(rdramb[spb+17]<<16)|(rdramb[spb+18]<<8)|rdramb[spb+19];
              u32 pri      = (rdramb[spb+20]<<24)|(rdramb[spb+21]<<16)|(rdramb[spb+22]<<8)|rdramb[spb+23];
              printf("[CTHREAD#%d] osCreateThread tcb=%08X id=%08X entry=%08X arg=%08X stacktop=%08X pri=%08X Count=%08X\n",
                createThreadCnt,
                (u32)r4300.gpr[4], (u32)r4300.gpr[5], (u32)r4300.gpr[6], (u32)r4300.gpr[7],
                stacktop, pri, (unsigned int)Count);
            }
          }

          /* BUILD 00138: exception dispatch trace at handler JR t2 (0x803278DC) */
          {
            static int dispCnt = 0;
            if (dispCnt < 40 && r4300.pc == 0x803278DC) {
              dispCnt++;
              u32 qhead = (rdramb[0x3359A8]<<24)|(rdramb[0x3359A9]<<16)|(rdramb[0x3359AA]<<8)|rdramb[0x3359AB];
              u32 runth = (rdramb[0x3359B0]<<24)|(rdramb[0x3359B1]<<16)|(rdramb[0x3359B2]<<8)|rdramb[0x3359B3];
              printf("[DISP#%d] s0=%08X t1=%08X t2(target)=%08X Status=%08X Cause=%08X mi_intr=%08X mi_mask=%08X qhead=%08X run=%08X Count=%08X\n",
                dispCnt,
                (u32)r4300.gpr[16], (u32)r4300.gpr[9], (u32)r4300.gpr[10],
                (u32)Status, (u32)Cause,
                MI_register.mi_intr_reg, MI_register.mi_intr_mask_reg,
                qhead, runth, (unsigned int)Count);
            }
          }

          /* BUILD 00145: scheduler state at exception vector entry (0x80327650) */
          {
            static int excCnt = 0;
            if (excCnt < 100 && r4300.pc == 0x80327650) {
              excCnt++;
              u32 qhead = (rdramb[0x3359A8]<<24)|(rdramb[0x3359A9]<<16)|(rdramb[0x3359AA]<<8)|rdramb[0x3359AB];
              u32 runth = (rdramb[0x3359B0]<<24)|(rdramb[0x3359B1]<<16)|(rdramb[0x3359B2]<<8)|rdramb[0x3359B3];
              u32 tssp  = (rdramb[0x372F0]<<24)|(rdramb[0x372F1]<<16)|(rdramb[0x372F2]<<8)|rdramb[0x372F3];
              printf("[EXC#%d] entry Status=%08X Cause=%08X EPC=%08X run=%08X qhead=%08X tsSP=%08X mi_intr=%08X Count=%08X\n",
                excCnt,
                (u32)Status, (u32)Cause, (u32)EPC,
                runth, qhead, tssp,
                MI_register.mi_intr_reg, (unsigned int)Count);
              /* BUILD 00146: one-time dump of the REAL int tables at first exception */
              if (excCnt == 1) {
                int ti;
                u8 off10;
                u32 addr, tgt;
                printf("[EXCTABLE] __osIntOffTable @ 0x80339980 (0x20 bytes):\n");
                for (ti = 0; ti < 0x20; ti++)
                  printf("[EXCTABLE]   %08X: %02X\n", 0x80339980 + ti, rdramb[(0x80339980+ti) & 0xFFFFFF]);
                printf("[EXCTABLE] __osIntTable @ 0x803399A0 (0x1C words):\n");
                for (ti = 0; ti < 0x1C; ti++) {
                  addr = 0x803399A0 + ti * 4;
                  tgt = (rdramb[(addr)&0xFFFFFF]<<24)|(rdramb[(addr+1)&0xFFFFFF]<<16)|(rdramb[(addr+2)&0xFFFFFF]<<8)|rdramb[(addr+3)&0xFFFFFF];
                  printf("[EXCTABLE]   %08X: %08X\n", addr, tgt);
                }
                off10 = rdramb[(0x80339980+0x10) & 0xFFFFFF];
                addr = 0x803399A0 + off10;
                tgt = (rdramb[(addr)&0xFFFFFF]<<24)|(rdramb[(addr+1)&0xFFFFFF]<<16)|(rdramb[(addr+2)&0xFFFFFF]<<8)|rdramb[(addr+3)&0xFFFFFF];
                printf("[EXCTABLE] index0x10: off=%02X target=%08X\n", off10, tgt);
              }
            }
          }

          /* BUILD 00146: redispatch entry + pop/run-store trace */
          {
            static int redispCnt = 0;
            if (redispCnt < 60 && r4300.pc == 0x80327B18) {
              redispCnt++;
              u32 k0v = (u32)r4300.gpr[26];
              u32 pk0 = 0, pqh = 0;
              if (k0v >= 0x80000000 && k0v < 0x80800000)
                pk0 = (rdramb[(k0v&0xFFFFFF)+4]<<24)|(rdramb[(k0v&0xFFFFFF)+5]<<16)|(rdramb[(k0v&0xFFFFFF)+6]<<8)|rdramb[(k0v&0xFFFFFF)+7];
              u32 qhead = (rdramb[0x3359A8]<<24)|(rdramb[0x3359A9]<<16)|(rdramb[0x3359AA]<<8)|rdramb[0x3359AB];
              if (qhead >= 0x80000000 && qhead < 0x80800000)
                pqh = (rdramb[(qhead&0xFFFFFF)+4]<<24)|(rdramb[(qhead&0xFFFFFF)+5]<<16)|(rdramb[(qhead&0xFFFFFF)+6]<<8)|rdramb[(qhead&0xFFFFFF)+7];
              printf("[REDISP#%d] B18 k0=%08X prioK0=%08X qhead=%08X prioQH=%08X Count=%08X\n",
                redispCnt, k0v, pk0, qhead, pqh, (unsigned int)Count);
            }
            static int b68Cnt = 0;
            if (b68Cnt < 20 && r4300.pc == 0x80327B68) {
              b68Cnt++;
              printf("[B68#%d] suspend path k0=%08X Count=%08X\n", b68Cnt, (u32)r4300.gpr[26], (unsigned int)Count);
            }
            static int runStoreCnt = 0;
            if (runStoreCnt < 60 && r4300.pc == 0x80327D78) {
              runStoreCnt++;
              u32 nrun = (u32)r4300.gpr[2];
              u32 qhead = (rdramb[0x3359A8]<<24)|(rdramb[0x3359A9]<<16)|(rdramb[0x3359AA]<<8)|rdramb[0x3359AB];
              printf("[RUN#%d] newrun=%08X%s qhead=%08X k0=%08X Count=%08X\n",
                runStoreCnt, nrun, (nrun==0x803359A0)?" (SENTINEL!)":"", qhead,
                (u32)r4300.gpr[26], (unsigned int)Count);
            }
          }

          /* BUILD 00148: scheduler run-queue timeline. ENQ = the JAL insert in
             __osEnqueueAndYield (0x80327D00); SKIP = its a0==0 skip check
             (0x80327CF8, skips the enqueue); POP = the unguarded run-queue pop
             (0x80327D58, run queue only); RESCH = reschedule entry (0x80327D68). */
          {
            static int enqCnt = 0;
            if (enqCnt < 80 && r4300.pc == 0x80327D00) {
              enqCnt++;
              u32 qh = (rdramb[0x3359A8]<<24)|(rdramb[0x3359A9]<<16)|(rdramb[0x3359AA]<<8)|rdramb[0x3359AB];
              printf("[ENQ#%d] a0(queue)=%08X a1(thread)=%08X qhead=%08X sp=%08X ra=%08X Count=%08X\n",
                enqCnt, (u32)r4300.gpr[4], (u32)r4300.gpr[5], qh,
                (u32)r4300.gpr[29], (u32)r4300.gpr[31], (unsigned int)Count);
            }
            static int skipCnt = 0;
            if (skipCnt < 40 && r4300.pc == 0x80327CF8) {
              skipCnt++;
              printf("[SKIP#%d] a0=%08X (skip enqueue if 0) sp=%08X ra=%08X Count=%08X\n",
                skipCnt, (u32)r4300.gpr[4], (u32)r4300.gpr[29], (u32)r4300.gpr[31],
                (unsigned int)Count);
            }
            static int popCnt = 0;
            if (popCnt < 80 && r4300.pc == 0x80327D58 && (u32)r4300.gpr[4] == 0x803359A8) {
              popCnt++;
              u32 head = (rdramb[0x3359A8]<<24)|(rdramb[0x3359A9]<<16)|(rdramb[0x3359AA]<<8)|rdramb[0x3359AB];
              u32 headNext = 0;
              if (head >= 0x80000000 && head < 0x80800000)
                headNext = (rdramb[(head&0xFFFFFF)]<<24)|(rdramb[(head&0xFFFFFF)+1]<<16)|(rdramb[(head&0xFFFFFF)+2]<<8)|rdramb[(head&0xFFFFFF)+3];
              printf("[POP#%d] popped=%08X%s next=%08X sp=%08X ra=%08X Count=%08X\n",
                popCnt, head, (head==0x803359A0)?" (SENTINEL!)":"", headNext,
                (u32)r4300.gpr[29], (u32)r4300.gpr[31], (unsigned int)Count);
            }
            static int reschCnt = 0;
            if (reschCnt < 40 && r4300.pc == 0x80327D68) {
              reschCnt++;
              u32 qh = (rdramb[0x3359A8]<<24)|(rdramb[0x3359A9]<<16)|(rdramb[0x3359AA]<<8)|rdramb[0x3359AB];
              printf("[RESCH#%d] qhead=%08X%s k0=%08X sp=%08X ra=%08X Count=%08X\n",
                reschCnt, qh, (qh==0x803359A0)?" (SENTINEL!)":"", (u32)r4300.gpr[26],
                (u32)r4300.gpr[29], (u32)r4300.gpr[31], (unsigned int)Count);
            }
          }

          /* BUILD 00145: log when non-flag path reads __osRunningThread (0x803276E8) */
          {
            static int nfpCnt = 0;
            if (nfpCnt < 40 && r4300.pc == 0x803276E8) {
              nfpCnt++;
              u32 runth = (rdramb[0x3359B0]<<24)|(rdramb[0x3359B1]<<16)|(rdramb[0x3359B2]<<8)|rdramb[0x3359B3];
              u32 qhead = (rdramb[0x3359A8]<<24)|(rdramb[0x3359A9]<<16)|(rdramb[0x3359AA]<<8)|rdramb[0x3359AB];
              printf("[NFP#%d] run=%08X qhead=%08X t0(tsbase)=%08X Status=%08X Cause=%08X EPC=%08X Count=%08X\n",
                nfpCnt, runth, qhead, (u32)r4300.gpr[8],
                (u32)Status, (u32)Cause, (u32)EPC, (unsigned int)Count);
            }
          }

          /* BUILD 00139: handler flag-path trace at 0x80327808 (reads [0x80335A44]) */
          {
            static int flagCnt = 0;
            if (flagCnt < 30 && r4300.pc == 0x80327808) {
              flagCnt++;
              u32 fl44 = (rdramb[0x335A44]<<24)|(rdramb[0x335A45]<<16)|(rdramb[0x335A46]<<8)|rdramb[0x335A47];
              u32 fl48 = (rdramb[0x335A48]<<24)|(rdramb[0x335A49]<<16)|(rdramb[0x335A4A]<<8)|rdramb[0x335A4B];
              printf("[FLAG#%d] fl44=%08X fl48=%08X t0(cause)=%08X k1(status)=%08X sp=%08X Count=%08X mi_intr=%08X\n",
                flagCnt, fl44, fl48, (u32)r4300.gpr[8], (u32)r4300.gpr[27],
                (u32)r4300.gpr[29], (unsigned int)Count, MI_register.mi_intr_reg);
            }
          }

          /* BUILD 00149: settle fast-path vs generic. FASTVI = flag44 nonzero
             (jump target 0x80327D88 mid-restore, no reschedule); FASTVI2 = flag48
             nonzero (same target); GEN = both flags zero (table dispatch @0x803278DC). */
          {
            static int fastviCnt = 0;
            if (fastviCnt < 10 && r4300.pc == 0x80327814) {
              fastviCnt++;
              u32 fl44 = (rdramb[0x335A44]<<24)|(rdramb[0x335A45]<<16)|(rdramb[0x335A46]<<8)|rdramb[0x335A47];
              printf("[FASTVI#%d] flag44 path taken fl44=%08X t0=%08X k0=%08X Count=%08X mi_intr=%08X\n",
                fastviCnt, fl44, (u32)r4300.gpr[8], (u32)r4300.gpr[26],
                (unsigned int)Count, MI_register.mi_intr_reg);
            }
            static int fastvi2Cnt = 0;
            if (fastvi2Cnt < 10 && r4300.pc == 0x80327844) {
              fastvi2Cnt++;
              u32 fl48 = (rdramb[0x335A48]<<24)|(rdramb[0x335A49]<<16)|(rdramb[0x335A4A]<<8)|rdramb[0x335A4B];
              printf("[FASTVI2#%d] flag48 path taken fl48=%08X t0=%08X k0=%08X Count=%08X mi_intr=%08X\n",
                fastvi2Cnt, fl48, (u32)r4300.gpr[8], (u32)r4300.gpr[26],
                (unsigned int)Count, MI_register.mi_intr_reg);
            }
            static int genCnt = 0;
            if (genCnt < 10 && r4300.pc == 0x80327880) {
              genCnt++;
              u32 qhead = (rdramb[0x3359A8]<<24)|(rdramb[0x3359A9]<<16)|(rdramb[0x3359AA]<<8)|rdramb[0x3359AB];
              printf("[GEN#%d] generic path (both flags 0) t0=%08X qhead=%08X Count=%08X mi_intr=%08X\n",
                genCnt, (u32)r4300.gpr[8], qhead,
                (unsigned int)Count, MI_register.mi_intr_reg);
            }
          }

          /* BUILD 00149: confirm the exit-thunk -> __osCleanupThread path of the crash.
             EXITTHUNK = 0x80327EA8 (only way to reach __osCleanupThread, no JAL callers);
             CLEANUP = __osCleanupThread entry (ra = caller). */
          {
            static int exitThunkCnt = 0;
            if (exitThunkCnt < 10 && r4300.pc == 0x80327EA8) {
              exitThunkCnt++;
              u32 runth = (rdramb[0x3359B0]<<24)|(rdramb[0x3359B1]<<16)|(rdramb[0x3359B2]<<8)|rdramb[0x3359B3];
              printf("[EXITTHUNK#%d] at 0x80327EA8 run=%08X k0=%08X sp=%08X ra=%08X Count=%08X\n",
                exitThunkCnt, runth, (u32)r4300.gpr[26],
                (u32)r4300.gpr[29], (u32)r4300.gpr[31], (unsigned int)Count);
            }
            static int cleanupCnt = 0;
            if (cleanupCnt < 10 && r4300.pc == 0x8032AE70) {
              cleanupCnt++;
              u32 runth = (rdramb[0x3359B0]<<24)|(rdramb[0x3359B1]<<16)|(rdramb[0x3359B2]<<8)|rdramb[0x3359B3];
              printf("[CLEANUP#%d] __osCleanupThread a0=%08X run=%08X sp=%08X ra=%08X Count=%08X\n",
                cleanupCnt, (u32)r4300.gpr[4], runth,
                (u32)r4300.gpr[29], (u32)r4300.gpr[31], (unsigned int)Count);
            }
          }
        }

        /* --- SP corruption detector: log when gpr[29] transitions to 0 --- */
        {
          static u32 prev_sp = 0xDEADBEEF;
          static int spCorruptCnt = 0;
          u32 cur_sp = (u32)r4300.gpr[29];
          if (prev_sp != 0 && cur_sp == 0 && spCorruptCnt < 20) {
            spCorruptCnt++;
            printf("[SP_CORRUPT#%d] SP went to 0! pc=%08X instr=%08X prev_sp=%08X\n",
              spCorruptCnt, r4300.pc, read_inst(r4300.pc), prev_sp);
            printf("[SP_CORRUPT#%d] All regs: a0=%08X a1=%08X v0=%08X v1=%08X\n",
              spCorruptCnt,
              (u32)r4300.gpr[4], (u32)r4300.gpr[5],
              (u32)r4300.gpr[2], (u32)r4300.gpr[3]);
            printf("[SP_CORRUPT#%d] t0-t7: %08X %08X %08X %08X %08X %08X %08X %08X\n",
              spCorruptCnt,
              (u32)r4300.gpr[8], (u32)r4300.gpr[9], (u32)r4300.gpr[10], (u32)r4300.gpr[11],
              (u32)r4300.gpr[12], (u32)r4300.gpr[13], (u32)r4300.gpr[14], (u32)r4300.gpr[15]);
            printf("[SP_CORRUPT#%d] s0-s7: %08X %08X %08X %08X %08X %08X %08X %08X\n",
              spCorruptCnt,
              (u32)r4300.gpr[16], (u32)r4300.gpr[17], (u32)r4300.gpr[18], (u32)r4300.gpr[19],
              (u32)r4300.gpr[20], (u32)r4300.gpr[21], (u32)r4300.gpr[22], (u32)r4300.gpr[23]);
            printf("[SP_CORRUPT#%d] t8-t9 k0-k1 ra: %08X %08X %08X %08X %08X\n",
              spCorruptCnt,
              (u32)r4300.gpr[24], (u32)r4300.gpr[25],
              (u32)r4300.gpr[26], (u32)r4300.gpr[27],
              (u32)r4300.gpr[31]);
            printf("[SP_CORRUPT#%d] Status=%08X Cause=%08X EPC=%08X Count=%08X\n",
              spCorruptCnt,
              (u32)Status, (u32)Cause, (u32)EPC, (unsigned int)Count);
            {
              u32 qhead = (rdramb[0x3359A8]<<24)|(rdramb[0x3359A9]<<16)|(rdramb[0x3359AA]<<8)|rdramb[0x3359AB];
              u32 runth = (rdramb[0x3359B0]<<24)|(rdramb[0x3359B1]<<16)|(rdramb[0x3359B2]<<8)|rdramb[0x3359B3];
              u32 qhead2 = (rdramb[0x3359AC]<<24)|(rdramb[0x3359AD]<<16)|(rdramb[0x3359AE]<<8)|rdramb[0x3359AF];
              printf("[SP_CORRUPT#%d] qhead=%08X qhead2=%08X run=%08X Count=%08X\n",
                spCorruptCnt, qhead, qhead2, runth, (unsigned int)Count);
              printf("[SP_CORRUPT#%d] TCB 0x803359A0 (k0) context dump:\n", spCorruptCnt);
              {
                int si;
                u32 k0t = (u32)r4300.gpr[26];
                if (k0t >= 0x80000000 && k0t < 0x80800000) {
                  u8* cb = &rdramb[k0t & 0xFFFFFF];
                  for (si = 0; si < 0x80; si += 16)
                    printf("  %08X: %08X %08X %08X %08X\n", k0t + si,
                      (cb[si]<<24)|(cb[si+1]<<16)|(cb[si+2]<<8)|cb[si+3],
                      (cb[si+4]<<24)|(cb[si+5]<<16)|(cb[si+6]<<8)|cb[si+7],
                      (cb[si+8]<<24)|(cb[si+9]<<16)|(cb[si+10]<<8)|cb[si+11],
                      (cb[si+12]<<24)|(cb[si+13]<<16)|(cb[si+14]<<8)|cb[si+15]);
                }
              }
            }
            /* Dump 8 words from old_sp (the stack before corruption) */
            if (prev_sp >= 0x80000000 && prev_sp < 0x80800000) {
              int si;
              u8* spBase = &rdramb[prev_sp & 0xFFFFFF];
              printf("[SP_CORRUPT#%d] Stack dump @ prev_sp=%08X:\n", spCorruptCnt, prev_sp);
              for (si = 0; si < 32; si += 4)
                printf("  %08X: %08X %08X %08X %08X\n", prev_sp + si,
                  (spBase[si]<<24)|(spBase[si+1]<<16)|(spBase[si+2]<<8)|spBase[si+3],
                  (spBase[si+4]<<24)|(spBase[si+5]<<16)|(spBase[si+6]<<8)|spBase[si+7],
                  (spBase[si+8]<<24)|(spBase[si+9]<<16)|(spBase[si+10]<<8)|spBase[si+11],
                  (spBase[si+12]<<24)|(spBase[si+13]<<16)|(spBase[si+14]<<8)|spBase[si+15]);
            }
          }
          prev_sp = cur_sp;
        }

        /* --- Track every SP register change (debug: only log large changes) --- */
        {
          static u32 lastLoggedSp = 0;
          static int spChangeCnt = 0;
          u32 curSp2 = (u32)r4300.gpr[29];
          if (spChangeCnt < 500 && curSp2 != lastLoggedSp) {
            u32 diff = (curSp2 > lastLoggedSp) ? (curSp2 - lastLoggedSp) : (lastLoggedSp - curSp2);
            if (diff > 0x1000 || curSp2 == 0 || lastLoggedSp == 0) {
              spChangeCnt++;
              printf("[SP_CHG#%d] SP: %08X -> %08X pc=%08X instr=%08X Count=%08X\n",
                spChangeCnt, lastLoggedSp, curSp2, r4300.pc, read_inst(r4300.pc), (unsigned int)Count);
            }
          }
          lastLoggedSp = curSp2;
        }

        /* --- Post-hack trace: log every 200th instruction for first 200K after hack --- */
        static int postHackCnt = 0;
        static int postHackLogCnt = 0;
        if (bgeuHackApplied && postHackCnt < 200000) {
          postHackCnt++;
          if ((postHackCnt % 200) == 0 && postHackLogCnt < 1000) {
            postHackLogCnt++;
            printf("[PH#%d] pc=%08X instr=%08X a0=%08X a1=%08X t0=%08X t8=%08X ra=%08X sp=%08X Count=%08X\n",
              postHackLogCnt, r4300.pc, read_inst(r4300.pc),
              (u32)r4300.gpr[4], (u32)r4300.gpr[5],
              (u32)r4300.gpr[8], (u32)r4300.gpr[24],
              (u32)r4300.gpr[31], (u32)r4300.gpr[29], (unsigned int)Count);
          }
          if (postHackCnt == 200000)
            printf("[POSTHACK] trace complete (200K insns, %d logged)\n", postHackLogCnt);
        }

        /* One-time: dump raw instruction hex from RDRAM for disassembly verification */
        if (!codeHexDumpDone && r4300.pc >= 0x8027F500 && r4300.pc <= 0x8027F580) {
          codeHexDumpDone = 1;
          int i;
          printf("[CODEHEX] Decompressor code at 0x8027F4F0 (36 words):\n");
          for (i = 0; i < 36; i++) {
            u32 addr = 0x8027F4F0 + i * 4;
            u8 *cp = &rdramb[addr & 0xFFFFFF];
            u32 insn = (cp[0] << 24) | (cp[1] << 16) | (cp[2] << 8) | cp[3];
            printf("[CODEHEX]   %08X: %08X\n", addr, insn);
          }
        }

        /* Hack: at BNE a1,t8 (0x8027F574), if a1 overshot t8, force a1=t8 so loop exits */
        if (!bgeuHackApplied && r4300.pc == 0x8027F574
            && (u32)r4300.gpr[5] > (u32)r4300.gpr[24]) {
          bgeuHackApplied = 1;
          printf("[BGEU_HACK] Forcing a1=t8: a1=%08X -> %08X (was %08X)\n",
            (u32)r4300.gpr[24], (u32)r4300.gpr[24],
            (u32)r4300.gpr[5]);
          r4300.gpr[5] = r4300.gpr[24]; /* force a1 = t8 */
        }
      }

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