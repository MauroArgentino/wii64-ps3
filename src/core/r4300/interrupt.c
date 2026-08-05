/**
 * Mupen64 - interrupt.c
 * Copyright (C) 2002 Hacktarux
 *               2010 emu_kidid
 *
 * Mupen64 homepage: http://mupen64.emulation64.com
 * email address: hacktarux@yahoo.fr
 *                emukidid@gmail.com
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

#include <stdio.h>
#include <stdlib.h>
#include "r4300.h"
#include "macros.h"
#include "interrupt.h"
#include "exception.h"
#include "../../config.h"
#include "../../main/plugin.h"
#include "../../main/guifuncs.h"
#include "../../main/savestates.h"
#include "../n64_memory/memory.h"
#include <ppu-types.h>

static int SPECIAL_done = 0;
int vi_field            = 0;
u32 next_vi   = 0;
static interrupt_queue *q = NULL;

void clear_queue()
{
  while(q != NULL) {
    interrupt_queue *aux = q->next;
    free(q);
    q = aux;
  }
}

void print_queue()
{
  interrupt_queue *aux;
  printf("------------------ %x\n", (unsigned int)Count);
  aux = q;
  while (aux != NULL) {
    printf("Count:%x, %x\n", (unsigned int)aux->count, aux->type);
    aux = aux->next;
  }
  printf("------------------\n");
}

int before_event(u32 evt1, u32 evt2, int type2)
{
  if(evt1 - Count < 0x80000000) {
    if(evt2 - Count < 0x80000000) {
      if((evt1 - Count) < (evt2 - Count)) {
        return 1;
      }
      else {
        return 0;
      }
    }
    else {
      if((Count - evt2) < 0x10000000) {
        if((type2 == SPECIAL_INT) && (SPECIAL_done)) {
          return 1;
        }
        else {
          return 0;
        }     
      }
      else {
        return 1;
      }
    }
  }
  else {
    return 0;
  }
}

void add_interrupt_event(int type, u32 delay)
{
  u32 count = Count + delay;
  int special = 0;
  
  if(type == SPECIAL_INT) {
    special = 1;
  }
  if(Count > 0x80000000) {
    SPECIAL_done = 0;
  }
     
  if (get_event(type)) {
    //printf("two events of type %x in queue\n", type);
  }
   
  interrupt_queue *aux = q;
  if (q == NULL) {
    q = malloc(sizeof(interrupt_queue));
    q->next = NULL;
    q->count = count;
    q->type = type;
    r4300.next_interrupt = q->count;
    return;
  }
   
  if(before_event(count, q->count, q->type) && !special) {
    q = malloc(sizeof(interrupt_queue));
    q->next = aux;
    q->count = count;
    q->type = type;
    r4300.next_interrupt = q->count;
    return;
  }

  while (aux->next != NULL && (!before_event(count, aux->next->count, aux->next->type) || special)) {
    aux = aux->next;
  }

  if (aux->next == NULL) {
    aux->next = malloc(sizeof(interrupt_queue));
    aux = aux->next;
    aux->next = NULL;
    aux->count = count;
    aux->type = type;
  }
  else {
    interrupt_queue *aux2;
    if (type != SPECIAL_INT) {
      while(aux->next != NULL && aux->next->count == count) {
        aux = aux->next;
      }
    }
    aux2 = aux->next;
    aux->next = malloc(sizeof(interrupt_queue));
    aux = aux->next;
    aux->next = aux2;
    aux->count = count;
    aux->type = type;
  }
}

void add_interrupt_event_count(int type, u32 count)
{
  add_interrupt_event(type, (count - Count));
}

void remove_interrupt_event()
{
  interrupt_queue *aux = q->next;
  if(q->type == SPECIAL_INT) {
    SPECIAL_done = 1;
  }
  free(q);
  q = aux;
  if (q != NULL && (q->count > Count || (Count - q->count) < 0x80000000)) {
    r4300.next_interrupt = q->count;
  }
  else {
    r4300.next_interrupt = 0;
  }
}

u32 get_event(int type)
{
  interrupt_queue *aux = q;
  if (q == NULL) {
    return 0;
  }
  if (q->type == type) {
    return q->count;
  }
  while (aux->next != NULL && aux->next->type != type) {
    aux = aux->next;
  }
  if (aux->next != NULL) {
    return aux->next->count;
  }
  return 0;
}

void remove_event(int type)
{
  interrupt_queue *aux = q;
  if (q == NULL) return;
  if (q->type == type) {
    aux = aux->next;
    free(q);
    q = aux;
    return;
  }
  while (aux->next != NULL && aux->next->type != type) {
    aux = aux->next;
  }
  if (aux->next != NULL) { // it's a type int
    interrupt_queue *aux2 = aux->next->next;
    free(aux->next);
    aux->next = aux2;
  }
}

void translate_event_queue(u32 base)
{
  interrupt_queue *aux;
  remove_event(COMPARE_INT);
  remove_event(SPECIAL_INT);
  aux=q;
  while (aux != NULL) {
    aux->count = (aux->count - Count)+base;
    aux = aux->next;
  }
  add_interrupt_event_count(COMPARE_INT, Compare);
  add_interrupt_event_count(SPECIAL_INT, 0);
}

// save the queue (for save states)
int save_eventqueue_infos(char *buf)
{
  int len = 0;
  interrupt_queue *aux = q;
  if (q == NULL) {
    *((u32*)&buf[0]) = 0xFFFFFFFF;
    return 4;
  }
  while (aux != NULL) {
    memcpy(buf+len  , &aux->type , 4);
    memcpy(buf+len+4, &aux->count, 4);
    len += 8;
    aux = aux->next;
  }
  *((u32*)&buf[len]) = 0xFFFFFFFF;
  return len+4;
}

// load the queue (for save states)
void load_eventqueue_infos(char *buf)
{
  unsigned int len = 0, type = 0, count = 0;
  clear_queue();
  while (*((u32*)&buf[len]) != 0xFFFFFFFF) {
    type  = *((u32*)&buf[len]);
    count = *((u32*)&buf[len+4]);
    add_interrupt_event_count(type, count);
    len += 8;
  }
}

void init_interrupt()
{
  SPECIAL_done = 1;
  next_vi = r4300.next_interrupt = 5000;
  vi_register.vi_delay = next_vi;
  vi_field = 0;
  clear_queue();
  add_interrupt_event_count(VI_INT, next_vi);
  add_interrupt_event_count(SPECIAL_INT, 0);
}

// Map the pending MI interrupt sources into the correct Cause.IP bits.
// Real N64 hardware: SP->IP2(0x400), SI->IP3(0x800), AI->IP4(0x1000),
// VI->IP5(0x2000), PI->IP6(0x4000), DP->IP7(0x8000).
// IP7 (0x8000) is preserved because the COMPARE interrupt also uses it.
static void update_cause()
{
  u32 intr = MI_register.mi_intr_reg & MI_register.mi_intr_mask_reg;

  Cause &= 0xFFFFFF83;   // clear exception code field (bits 2-6)
  Cause &= ~0x7C00;      // clear IP2-IP6 (bits 10-14), keep IP7 for COMPARE

  if (intr & 0x001) Cause |= 0x400;   // SP
  if (intr & 0x002) Cause |= 0x800;   // SI
  if (intr & 0x004) Cause |= 0x1000;  // AI
  if (intr & 0x008) Cause |= 0x2000;  // VI
  if (intr & 0x010) Cause |= 0x4000;  // PI
  if (intr & 0x020) Cause |= 0x8000;  // DP
}

void check_interrupt()
{
  if (MI_register.mi_intr_reg & MI_register.mi_intr_mask_reg) {
    update_cause();
  }
  else {
    Cause &= 0xFFFF83FF;
  }
  if ((Status & 7) != 1) {
    return;
  }
  if (Status & Cause & 0xFF00) {
    if(q == NULL) {
      q = malloc(sizeof(interrupt_queue));
      q->next = NULL;
      q->count = Count;
      q->type = CHECK_INT;
    }
    else {
      interrupt_queue* aux = malloc(sizeof(interrupt_queue));
      aux->next = q;
      aux->count = Count;
      aux->type = CHECK_INT;
      q = aux;
    }
    r4300.next_interrupt = Count;
  }
}

// Just wrapping up some common code
int chk_status(int chk) {
  if(chk) {
    if (MI_register.mi_intr_reg & MI_register.mi_intr_mask_reg) {
      update_cause();
    }
    else {
      return 0;
    }
  }
  if ((Status & 7) != 1) {
    return 0;
  }
  if (!(Status & Cause & 0xFF00)) {
    return 0;
  }
  return 1;
}

void gen_interrupt()
{
  if (savestates_job & LOADSTATE) {
    savestates_load();
    savestates_job &= ~LOADSTATE;
    return;
  }
  if (r4300.skip_jump) {
    if (q->count > Count || (Count - q->count) < 0x80000000) {
      r4300.next_interrupt = q->count;
    }
    else {
      r4300.next_interrupt = 0;
    }
    r4300.pc = r4300.skip_jump;
    r4300.last_pc = r4300.pc;
    r4300.skip_jump=0;
    return;
  } 

  {
    static int intLogCnt = 0;
    if (intLogCnt < 500) {
      intLogCnt++;
      printf("[INT#%d] type=%03X Count=%08X mi_intr=%08X mi_mask=%08X Status=%08X Cause=%08X sp_st=%08X\n",
        intLogCnt, q->type, (unsigned int)Count,
        MI_register.mi_intr_reg, MI_register.mi_intr_mask_reg,
        (u32)Status, (u32)Cause, sp_register.sp_status_reg);
    }
  }
  switch(q->type) {
    case SPECIAL_INT:
      remove_interrupt_event();
      add_interrupt_event_count(SPECIAL_INT, 0);
      return;
    break;
    case VI_INT:
      updateScreen();
#ifdef PROFILE
      refresh_stat();
#endif
      new_vi();
      vi_register.vi_delay = (vi_register.vi_v_sync == 0) ? 500000 : ((vi_register.vi_v_sync + 1)*1500);
      next_vi += vi_register.vi_delay;
      vi_field = (vi_register.vi_status&0x40) ? 1-vi_field : 0; 
      remove_interrupt_event();
      add_interrupt_event_count(VI_INT, next_vi);
  
      MI_register.mi_intr_reg |= 0x08;
      if(!chk_status(1)) {
        // Real N64: the VI interrupt line is asserted only while the current
        // scanline matches VI_INTR and self-clears as soon as the scanline
        // advances (or when VI_CURRENT is written). If the CPU did not take
        // the interrupt now, the line is already gone. Do not leave it
        // latched, or check_interrupt() will fire a CHECK_INT storm as soon
        // as IE/mask is enabled (SM64 boot hang).
        MI_register.mi_intr_reg &= ~0x08;
        return;
      }
      // BUILD 00144: the interrupt IS taken. Keep the MI bit (and the
      // Cause.IP5 bit set by update_cause() inside chk_status) latched so the
      // game's exception handler can read Cause and dispatch to the right
      // device (SM64 maps VI to __osEnqueueAndYield, which never acks VI --
      // it relies on reading Cause.IP5 to select the handler). Clearing the
      // bit here made the handler see Cause=0 -> wrong dispatch -> corrupt
      // thread context restore -> TLB-store-miss loop. Deassert shortly after
      // the exception (pulse model) so that once the handler returns and IE is
      // re-enabled, check_interrupt() does not immediately re-queue a
      // CHECK_INT storm.
      remove_event(VI_PULSE_INT);
      add_interrupt_event_count(VI_PULSE_INT, Count + INTERRUPT_PULSE_LEN);
      {
        static int viSelfCnt = 0;
        if (viSelfCnt < 20) {
          viSelfCnt++;
          printf("[VISELF#%d] VI pulse-end scheduled mi_intr=%08X pc=%08X Count=%08X\n",
            viSelfCnt, MI_register.mi_intr_reg, r4300.pc, (unsigned int)Count);
        }
      }
    break;
  
    case COMPARE_INT:
      remove_interrupt_event();
      Count+=2;
      add_interrupt_event_count(COMPARE_INT, Compare);
      Count-=2;
  
      Cause = (Cause | 0x8000) & 0xFFFFFF83;
      if(!chk_status(0)) {
        return;
      }
    break;
  
    case CHECK_INT:
      remove_interrupt_event();
    break;
  
    case SI_INT:
      PIF_RAMb[0x3F] = 0x0;
      remove_interrupt_event();
      MI_register.mi_intr_reg |= 0x02;
      si_register.si_status |= 0x1000;
      if(!chk_status(1)) {
        // Real N64: the SI interrupt line is asserted only when an SI DMA
        // (PIF RAM transfer) completes and self-deasserts on its own; no
        // SI_STATUS write lowers it (libdragon: SI_CLEAR_INTERRUPT == 0).
        // The DMA data was already moved synchronously by dma_si_write()/
        // dma_si_read(); the sticky SI_STATUS 0x1000 bit stays set for games
        // that poll it. Do not leave the MI bit latched, or check_interrupt()
        // fires a CHECK_INT storm once IE/mask is enabled (SM64 boot hang).
        MI_register.mi_intr_reg &= ~0x02;
        return;
      }
      // BUILD 00144: the interrupt IS taken. Keep the MI bit (and the
      // Cause.IP3 bit set by update_cause() inside chk_status) latched so the
      // game's SI handler can read Cause to confirm the source, then
      // deassert shortly after the exception (pulse model) so that once the
      // handler returns and IE is re-enabled, check_interrupt() does not
      // immediately re-queue a CHECK_INT storm.
      remove_event(SI_PULSE_INT);
      add_interrupt_event_count(SI_PULSE_INT, Count + INTERRUPT_PULSE_LEN);
      {
        static int siSelfCnt = 0;
        if (siSelfCnt < 20) {
          siSelfCnt++;
          printf("[SISELF#%d] SI pulse-end scheduled mi_intr=%08X pc=%08X Count=%08X\n",
            siSelfCnt, MI_register.mi_intr_reg, r4300.pc, (unsigned int)Count);
        }
      }
    break;
  
    case PI_INT:
      remove_interrupt_event();
      MI_register.mi_intr_reg |= 0x10;
      pi_register.read_pi_status_reg &= ~3;
      if(!chk_status(1)) {
        // Real N64: the PI interrupt line is asserted when a PI DMA between
        // the cartridge and RDRAM finishes and self-deasserts on its own; the
        // SM64 OS dispatches PI to its generic thread-switch tail (0x80327B68)
        // and never writes PI_STATUS in the exception path. The "held until
        // PI_STATUS write" wording in some docs describes the latched
        // PI_STATUS bit, not the MI line. Do not leave the MI bit latched, or
        // check_interrupt() fires a CHECK_INT storm once IE/mask is enabled.
        MI_register.mi_intr_reg &= ~0x10;
        return;
      }
      // BUILD 00144: the interrupt IS taken. Keep the MI bit (and the
      // Cause.IP4 bit set by update_cause() inside chk_status) latched so the
      // game's OS dispatcher reads Cause to select the device handler (SM64
      // maps PI to the generic thread-switch tail that never acks PI), then
      // deassert shortly after the exception (pulse model) so that once the
      // handler returns and IE is re-enabled, check_interrupt() does not
      // immediately re-queue a CHECK_INT storm.
      remove_event(PI_PULSE_INT);
      add_interrupt_event_count(PI_PULSE_INT, Count + INTERRUPT_PULSE_LEN);
      {
        static int piSelfCnt = 0;
        if (piSelfCnt < 20) {
          piSelfCnt++;
          printf("[PISELF#%d] PI pulse-end scheduled mi_intr=%08X pc=%08X Count=%08X\n",
            piSelfCnt, MI_register.mi_intr_reg, r4300.pc, (unsigned int)Count);
        }
      }
    break;
  
    case AI_INT:
      if (ai_register.ai_status & 0x80000000) { // full
        u32 ai_event = get_event(AI_INT);
        remove_interrupt_event();
        ai_register.ai_status &= ~0x80000000;
        ai_register.current_delay = ai_register.next_delay;
        ai_register.current_len = ai_register.next_len;
        add_interrupt_event_count(AI_INT, ai_event+ai_register.next_delay);
      }
      else {
        remove_interrupt_event();
        ai_register.ai_status &= ~0x40000000;
      }
      MI_register.mi_intr_reg |= 0x04;
      if(!chk_status(1)) {
        return;
      }
      // BUILD 00144: interrupt taken; keep the AI bit set until the pulse-end
      // event deasserts it so the handler can read Cause (IP1), then clear so
      // IE-reenable does not re-fire a storm.
      remove_event(AI_PULSE_INT);
      add_interrupt_event_count(AI_PULSE_INT, Count + INTERRUPT_PULSE_LEN);
    break;
  
    case SP_INT:
      remove_interrupt_event();
      sp_register.sp_status_reg |= 0x303;
      sp_register.signal2 = 1;
      sp_register.broke = 1;
      sp_register.halt = 1;

      {
        static int spIntCnt = 0;
        if (spIntCnt < 50) {
          spIntCnt++;
          printf("[SP_INT#%d] intr_break=%d sp_st=%08X mi_intr=%08X mi_mask=%08X\n",
            spIntCnt, sp_register.intr_break, sp_register.sp_status_reg,
            MI_register.mi_intr_reg, MI_register.mi_intr_mask_reg);
        }
      }

      if (!sp_register.intr_break) {
        printf("[SP_INT] intr_break=0, returning without exception!\n");
        return;
      }
      MI_register.mi_intr_reg |= 0x01;
      if(!chk_status(1)) {
        printf("[SP_INT] chk_status failed! mi_intr=%08X mi_mask=%08X Status=%08X Cause=%08X\n",
          MI_register.mi_intr_reg, MI_register.mi_intr_mask_reg, (u32)Status, (u32)Cause);
        return;
      }
      // BUILD 00144: interrupt taken; keep the SP bit set until the pulse-end
      // event deasserts it, then clear so IE-reenable does not re-fire.
      remove_event(SP_PULSE_INT);
      add_interrupt_event_count(SP_PULSE_INT, Count + INTERRUPT_PULSE_LEN);
    break;
  
    case DP_INT:
      remove_interrupt_event();
      dpc_register.dpc_status &= ~2;
      dpc_register.dpc_status |= 0x81;
      MI_register.mi_intr_reg |= 0x20;

      if(!chk_status(1)) {
        return;
      }
      // BUILD 00144: interrupt taken; keep the DP bit set until the pulse-end
      // event deasserts it, then clear so IE-reenable does not re-fire.
      remove_event(DP_PULSE_INT);
      add_interrupt_event_count(DP_PULSE_INT, Count + INTERRUPT_PULSE_LEN);
    break;

    /* BUILD 00144: pulse-end events. The associated device interrupt was taken
     * and the MI bit was deliberately left set so the game handler could read
     * Cause. Now deassert it; the handler's ERET will re-enable IE and
     * check_interrupt() must find no pending device bit or it will re-raise a
     * CHECK_INT storm. These events must NOT fall through to exception_general(). */
    case VI_PULSE_INT:
      MI_register.mi_intr_reg &= ~0x08;
      remove_interrupt_event();
      {
        static int vpCnt = 0;
        if (vpCnt < 20) {
          vpCnt++;
          printf("[VIPULSE#%d] VI deasserted mi_intr=%08X pc=%08X Count=%08X\n",
            vpCnt, MI_register.mi_intr_reg, r4300.pc, (unsigned int)Count);
        }
      }
      return;
    case SI_PULSE_INT:
      MI_register.mi_intr_reg &= ~0x02;
      remove_interrupt_event();
      {
        static int siPulseCnt = 0;
        if (siPulseCnt < 20) {
          siPulseCnt++;
          printf("[SIPULSE#%d] SI deasserted mi_intr=%08X pc=%08X Count=%08X\n",
            siPulseCnt, MI_register.mi_intr_reg, r4300.pc, (unsigned int)Count);
        }
      }
      return;
    case PI_PULSE_INT:
      MI_register.mi_intr_reg &= ~0x10;
      remove_interrupt_event();
      {
        static int ppCnt = 0;
        if (ppCnt < 20) {
          ppCnt++;
          printf("[PIPULSE#%d] PI deasserted mi_intr=%08X pc=%08X Count=%08X\n",
            ppCnt, MI_register.mi_intr_reg, r4300.pc, (unsigned int)Count);
        }
      }
      return;
    case AI_PULSE_INT:
      MI_register.mi_intr_reg &= ~0x04;
      remove_interrupt_event();
      return;
    case SP_PULSE_INT:
      MI_register.mi_intr_reg &= ~0x01;
      remove_interrupt_event();
      return;
    case DP_PULSE_INT:
      MI_register.mi_intr_reg &= ~0x20;
      remove_interrupt_event();
      return;

    default:
      remove_interrupt_event();
    break;
  }
  exception_general();
   
  if (savestates_job & SAVESTATE) {
    savestates_save();
    savestates_job &= ~SAVESTATE;
  }
}
