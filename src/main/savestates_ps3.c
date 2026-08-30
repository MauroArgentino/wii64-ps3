/**
 * Mupen64 - savestates.c
 * Copyright (C) 2002 Hacktarux
 * Copyright (C) 2008, 2009 emu_kidid
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "savestates.h"
#include "guifuncs.h"
#include "rom.h"
#include "../core/n64_memory/memory.h"
#include "../core/n64_memory/flashram.h"
#include "../core/r4300/macros.h"
#include "../core/r4300/r4300.h"
#include "../core/r4300/interrupt.h"
#include "../core/r4300/exception.h"
#include "wii64config.h"
#include "../debug.h"

extern int *autoinc_save_slot;
void pauseAudio(void);
void resumeAudio(void);
void resetAudioAfterLoad(void);

int savestates_job = 0;

static unsigned int savestates_slot = 0;
static u32 saved_ai_int = 0; /* pending AI_INT absolute count restored on load */

#define ST_MAGIC      0x53545053 /* "STPS" */
#define ST_VERSION    1
#define ST_PATH       "/dev_usb000/wii64/"

/* RDRAM: 4MB (0x100000 words) without USE_EXPANSION, 8MB with it.
 * We persist the actual allocated buffer; caps at 8MB. */
#define ST_RDRAM_WORDS  0x100000
#define ST_RDRAM_MAXW   0x200000

/* Deterministic subset of the R4300 struct we persist. Pointers are rebuilt on
 * load, so we only mirror value members. */
typedef struct {
	u32  pc, last_pc;
	unsigned long long gpr[32];
	unsigned long long hi, lo;
	unsigned long long local_gpr[2];
	u32  reg_cop0[32];
	unsigned long long fpr_data[32];
	s32  fcr0, fcr31;
	u32  next_interrupt, cic_chip;
	u32  delay_slot, skip_jump;
	s32  stop, llbit;
} st_cpu;

/* AI audio hardware state + the absolute count of the pending AI_INT. */
typedef struct {
	u32 ai_dram_addr;
	u32 ai_len;
	u32 ai_control;
	u32 ai_status;
	u32 ai_dacrate;
	u32 ai_bitrate;
	u32 next_delay;
	u32 next_len;
	u32 current_delay;
	u32 current_len;
	u32 ai_int_count;
	int present;
} st_ai;

typedef struct {
	unsigned short   mask;
	u32   vpn2;
	char g;
	unsigned char asid;
	u32   pfn_even;
	char c_even, d_even, v_even;
	u32   pfn_odd;
	char c_odd, d_odd, v_odd;
	char r;
} st_tlb;

typedef struct {
	u32   magic;
	u32   version;
	u32   slot;
	u32   rdram_words;
	char  rom_name[32];
	char  _pad[12];
} st_header;

void savestates_select_slot(unsigned int s)
{
   if (s > 9) return;
   savestates_slot = s;
}

static void st_build_name(char *out, size_t outlen)
{
	char clean[40];
	int i, n = 0;

	memset(clean, 0, sizeof(clean));
	if (ROM_HEADER && ROM_HEADER->nom) {
		for (i = 0, n = 0; i < 32 && ROM_HEADER->nom[i] && n < (int)sizeof(clean)-1; i++) {
			char ch = ROM_HEADER->nom[i];
			if (ch == ' ' || ch == '/' || ch == '\\' || ch == ':' || ch == '*'
			    || ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|')
				ch = '_';
			clean[n++] = ch;
		}
	}
	if (n == 0) strcpy(clean, "rom");
	snprintf(out, outlen, ST_PATH "%s_%u.st", clean, savestates_slot);
}

//returns 0 on file not existing
int savestates_exists(int mode)
{
	char path[160];
	FILE *f;

	st_build_name(path, sizeof(path));
	f = fopen(path, "rb");
	if (f) { fclose(f); return 1; }
	return 0;
}

void savestates_save()
{
	char path[160];
	FILE *f;
	int i;
	st_header hdr;
	st_cpu cpu;
	size_t rdram_bytes = ST_RDRAM_WORDS*4;

	if (!rdram) return;

	memset(&hdr, 0, sizeof(hdr));
	hdr.magic   = ST_MAGIC;
	hdr.version = ST_VERSION;
	hdr.slot    = savestates_slot;
	hdr.rdram_words = 0;
	if (ROM_HEADER && ROM_HEADER->nom)
		memcpy(hdr.rom_name, ROM_HEADER->nom, 32);
	else
		memcpy(hdr.rom_name, "unknown", 8);

	memset(&cpu, 0, sizeof(cpu));
	cpu.pc            = r4300.pc;
	cpu.last_pc       = r4300.last_pc;
	for (i = 0; i < 32; i++) cpu.gpr[i] = r4300.gpr[i];
	cpu.hi            = r4300.hi;
	cpu.lo            = r4300.lo;
	cpu.local_gpr[0]  = r4300.local_gpr[0];
	cpu.local_gpr[1]  = r4300.local_gpr[1];
	for (i = 0; i < 32; i++) cpu.reg_cop0[i] = r4300.reg_cop0[i];
	for (i = 0; i < 32; i++) cpu.fpr_data[i] = r4300.fpr_data[i];
	cpu.fcr0          = r4300.fcr0;
	cpu.fcr31         = r4300.fcr31;
	cpu.next_interrupt= r4300.next_interrupt;
	cpu.cic_chip      = r4300.cic_chip;
	cpu.delay_slot    = r4300.delay_slot;
	cpu.skip_jump     = r4300.skip_jump;
	cpu.stop          = r4300.stop;
	cpu.llbit         = r4300.llbit;

	st_build_name(path, sizeof(path));
	f = fopen(path, "wb");
	if (!f) { printf("Save State: no se pudo escribir %s", path); return; }

	fwrite(&hdr, 1, sizeof(hdr), f);
	fwrite(&cpu, 1, sizeof(cpu), f);

	/* AI audio hardware state + pending AI_INT absolute count. */
	st_ai ai;
	memset(&ai, 0, sizeof(ai));
	ai.ai_dram_addr  = ai_register.ai_dram_addr;
	ai.ai_len        = ai_register.ai_len;
	ai.ai_control    = ai_register.ai_control;
	ai.ai_status     = ai_register.ai_status;
	ai.ai_dacrate    = ai_register.ai_dacrate;
	ai.ai_bitrate    = ai_register.ai_bitrate;
	ai.next_delay    = ai_register.next_delay;
	ai.next_len      = ai_register.next_len;
	ai.current_delay = ai_register.current_delay;
	ai.current_len   = ai_register.current_len;
	ai.ai_int_count  = get_event(AI_INT);
	ai.present       = (ai_register.current_delay != 0);
	fwrite(&ai, 1, sizeof(ai), f);

	/* Persist the volatile TLB fields only (derivations are recomputed on load). */
	for (i = 0; i < 32; i++) {
		st_tlb t;
		memset(&t, 0, sizeof(t));
		t.mask     = tlb_e[i].mask;
		t.vpn2     = tlb_e[i].vpn2;
		t.g        = tlb_e[i].g;
		t.asid     = tlb_e[i].asid;
		t.pfn_even = tlb_e[i].pfn_even;
		t.c_even   = tlb_e[i].c_even;
		t.d_even   = tlb_e[i].d_even;
		t.v_even   = tlb_e[i].v_even;
		t.pfn_odd  = tlb_e[i].pfn_odd;
		t.c_odd    = tlb_e[i].c_odd;
		t.d_odd    = tlb_e[i].d_odd;
		t.v_odd    = tlb_e[i].v_odd;
		t.r        = tlb_e[i].r;
		fwrite(&t, 1, sizeof(t), f);
	}
	if (rdram_bytes > 0x800000) rdram_bytes = 0x800000;
	fwrite(rdram, 4, rdram_bytes/4, f);
	fwrite(SP_DMEM, 4, 0x800, f);
	fclose(f);
	DBG_LOG("[ST] Saved slot %u -> %s\n", savestates_slot, path);
}

void savestates_load()
{
	char path[160];
	FILE *f;
	int i;
	st_header hdr;
	st_cpu cpu;
	size_t rdram_bytes;
	u32 sp_dmem[0x800];

	st_build_name(path, sizeof(path));
	f = fopen(path, "rb");
	if (!f) { printf("Load State: no existe %s", path); return; }

	if (fread(&hdr, 1, sizeof(hdr), f) != sizeof(hdr) ||
	    hdr.magic != ST_MAGIC || hdr.version != ST_VERSION) {
		fclose(f);
		printf("Load State: archivo invalido");
		return;
	}
	if (fread(&cpu, 1, sizeof(cpu), f) != sizeof(cpu)) {
		fclose(f);
		printf("Load State: archivo corrupto");
		return;
	}

	/* Restore AI audio hardware state. */
	st_ai ai;
	memset(&ai, 0, sizeof(ai));
	if (fread(&ai, 1, sizeof(ai), f) != sizeof(ai)) {
		fclose(f);
		printf("Load State: archivo corrupto (ai)");
		return;
	}
	ai_register.ai_dram_addr  = ai.ai_dram_addr;
	ai_register.ai_len        = ai.ai_len;
	ai_register.ai_control    = ai.ai_control;
	ai_register.ai_status     = ai.ai_status;
	ai_register.ai_dacrate    = ai.ai_dacrate;
	ai_register.ai_bitrate    = ai.ai_bitrate;
	ai_register.next_delay    = ai.next_delay;
	ai_register.next_len      = ai.next_len;
	ai_register.current_delay = ai.current_delay;
	ai_register.current_len   = ai.current_len;
	saved_ai_int = ai.ai_int_count;   /* pending AI_INT absolute count (or 0) */

	(void)ai.present;

	/* CPU values (pointers are re-established afterward). */
	r4300.pc            = cpu.pc;
	r4300.last_pc       = cpu.last_pc;
	for (i = 0; i < 32; i++) r4300.gpr[i] = cpu.gpr[i];
	r4300.hi            = cpu.hi;
	r4300.lo            = cpu.lo;
	r4300.local_gpr[0]  = cpu.local_gpr[0];
	r4300.local_gpr[1]  = cpu.local_gpr[1];
	for (i = 0; i < 32; i++) r4300.reg_cop0[i] = cpu.reg_cop0[i];
	for (i = 0; i < 32; i++) r4300.fpr_data[i] = cpu.fpr_data[i];
	r4300.fcr0          = cpu.fcr0;
	r4300.fcr31         = cpu.fcr31;
	r4300.next_interrupt= cpu.next_interrupt;
	r4300.cic_chip      = cpu.cic_chip;
	r4300.delay_slot    = cpu.delay_slot;
	r4300.skip_jump     = cpu.skip_jump;
	r4300.stop          = cpu.stop;
	r4300.llbit         = cpu.llbit;

	/* TLB: restore volatile fields, recompute derivations, rebuild LUT. */
	memset(tlb_LUT_r, 0, 0x100000*4);
	memset(tlb_LUT_w, 0, 0x100000*4);
	for (i = 0; i < 32; i++) {
		st_tlb t;
		if (fread(&t, 1, sizeof(t), f) != sizeof(t)) break;
		tlb_e[i].mask     = t.mask;
		tlb_e[i].vpn2     = t.vpn2;
		tlb_e[i].g        = t.g;
		tlb_e[i].asid     = t.asid;
		tlb_e[i].pfn_even = t.pfn_even;
		tlb_e[i].c_even   = t.c_even;
		tlb_e[i].d_even   = t.d_even;
		tlb_e[i].v_even   = t.v_even;
		tlb_e[i].pfn_odd  = t.pfn_odd;
		tlb_e[i].c_odd    = t.c_odd;
		tlb_e[i].d_odd    = t.d_odd;
		tlb_e[i].v_odd    = t.v_odd;
		tlb_e[i].r        = t.r;

		tlb_e[i].start_even = tlb_e[i].vpn2 << 13;
		tlb_e[i].end_even   = tlb_e[i].start_even + (tlb_e[i].mask << 12) + 0xFFF;
		tlb_e[i].phys_even  = tlb_e[i].pfn_even << 12;
		tlb_e[i].start_odd  = tlb_e[i].end_even + 1;
		tlb_e[i].end_odd    = tlb_e[i].start_odd + (tlb_e[i].mask << 12) + 0xFFF;
		tlb_e[i].phys_odd   = tlb_e[i].pfn_odd << 12;

		tlb_map(i);
	}

	/* RDRAM. */
	if (rdram) {
		rdram_bytes = ST_RDRAM_WORDS*4;
		if (fread(rdram, 4, rdram_bytes/4, f) != rdram_bytes/4) {
			fclose(f);
			printf("Load State: rdram corrupto");
			return;
		}
	}

	/* SP_DMEM + SP_IMEM form a single contiguous block sized 0x800 words. */
	if (fread(sp_dmem, 4, 0x800, f) == 0x800) {
		for (i = 0; i < 0x800; i++) SP_DMEM[i] = sp_dmem[i];
	}

	fclose(f);

	/* Rebuild floating point register pointers + re-arm the interrupt queue. */
	set_fpr_pointers(r4300.fcr31 & 0x3);
	{
		u32 saved_next = cpu.next_interrupt;
		init_interrupt();
		clear_queue();

		/* Re-arm the pending AI audio interrupt so the restored AI registers
		 * keep advancing (audio playback depends on get_event(AI_INT)). */
		if (saved_ai_int && r4300.reg_cop0[9] < saved_ai_int) {
			add_interrupt_event_count(AI_INT, saved_ai_int);
		}

		if (saved_next && saved_next != 0x7FFFFFFF) {
			next_vi = r4300.next_interrupt = saved_next;
			vi_register.vi_delay = saved_next;
			add_interrupt_event_count(VI_INT, saved_next);
		} else {
			r4300.next_interrupt = 0x7FFFFFFF;
		}
	}
	saved_ai_int = 0;

	/* Re-sync the audio backend so the restored AI registers restart it. */
	resetAudioAfterLoad();

	DBG_LOG("[ST] Loaded slot %u <- %s\n", savestates_slot, path);
}
