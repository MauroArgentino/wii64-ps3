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
#include <sys/file.h>
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

#include <zlib.h>

extern int *autoinc_save_slot;
void pauseAudio(void);
void resumeAudio(void);
void resetAudioAfterLoad(void);

int savestates_job = 0;

static unsigned int savestates_slot = 0;
static u32 saved_ai_int = 0; /* pending AI_INT absolute count restored on load */

#define ST_MAGIC      0x53545053 /* "STPS" */
#define ST_VERSION    1
#define ST_VERSION_Z  2           /* body compressed with zlib (DEFLATE) */
#define ST_PATH       "/dev_usb000/wii64/savestates/"

/* Fixed-size part of the body: cpu + ai + 32*TLB (all byte-exact structs).
 * The variable RDRAM (rdram_words*4) and the 0x800-word SP_DMEM follow. */
#define ST_BODY_FIXED  (sizeof(st_cpu) + sizeof(st_ai) + 32*sizeof(st_tlb))
#define ST_BODY_SPDMEM (0x800*4)
#define ST_BODY_MAX    (ST_BODY_FIXED + ST_RDRAM_MAXW*4 + ST_BODY_SPDMEM)

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

	/* Ensure the savestates/ directory exists before writing (PS3 runtime). */
	{
		sysFSStat st;
		if (sysLv2FsStat("/dev_usb000/wii64/savestates", &st) != 0)
			sysLv2FsMkdir("/dev_usb000/wii64/savestates", 0777);
	}

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

/* The N64 framebuffer lives in the C++ video layer (REG.VI_ORIGIN / VI).
 * Capture + 1/10 BMP are done there via this extern "C" bridge, which also
 * owns the in-game "photo" shrink animation on save. */
extern int ps3_fb_snapshot(const char *st_path);

/* Serialize the state body (everything after the 64-byte header) into a
 * caller-provided buffer. Returns the number of bytes written, or 0 on a
 * NULL RDRAM. The buffer must be at least ST_BODY_MAX bytes. */
static size_t st_serialize_body(u8 *out, u32 rdram_words)
{
	int i;
	size_t pos = 0;
	st_cpu cpu;
	st_ai ai;

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
	memcpy(out + pos, &cpu, sizeof(cpu)); pos += sizeof(cpu);

	/* AI audio hardware state + pending AI_INT absolute count. */
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
	memcpy(out + pos, &ai, sizeof(ai)); pos += sizeof(ai);

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
		memcpy(out + pos, &t, sizeof(t)); pos += sizeof(t);
	}

	/* RDRAM (cap at 8MB) then SP_DMEM (contiguous with SP_IMEM). */
	if (rdram_words * 4 > 0x800000) rdram_words = 0x800000 / 4;
	if (rdram) memcpy(out + pos, rdram, (size_t)rdram_words*4);
	pos += (size_t)rdram_words*4;
	memcpy(out + pos, SP_DMEM, ST_BODY_SPDMEM);
	pos += ST_BODY_SPDMEM;

	return pos;
}

/* Restore emulator state from a serialized body buffer. Returns 0 on success,
 * -1 if the buffer is too small (corrupt). */
static int st_restore_body(const u8 *in, size_t len, u32 rdram_words)
{
	int i;
	size_t pos = 0;
	st_cpu cpu;
	st_ai ai;
	st_tlb t;
	u32 sp_dmem[0x800];
	size_t rdram_bytes = (size_t)rdram_words * 4;

	if (rdram_bytes > 0x800000) rdram_bytes = 0x800000;
	if (len < ST_BODY_FIXED + rdram_bytes + ST_BODY_SPDMEM) return -1;

	if (pos + sizeof(cpu) > len) return -1;
	memcpy(&cpu, in + pos, sizeof(cpu)); pos += sizeof(cpu);

	/* Restore AI audio hardware state. */
	memset(&ai, 0, sizeof(ai));
	if (pos + sizeof(ai) > len) return -1;
	memcpy(&ai, in + pos, sizeof(ai)); pos += sizeof(ai);
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
	if (pos + 32*sizeof(st_tlb) > len) return -1;
	for (i = 0; i < 32; i++) {
		memcpy(&t, in + pos, sizeof(t)); pos += sizeof(t);
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
	if (pos + rdram_bytes > len) return -1;
	if (rdram) memcpy(rdram, in + pos, rdram_bytes);
	pos += rdram_bytes;

	/* SP_DMEM + SP_IMEM form a single contiguous block sized 0x800 words. */
	if (pos + sizeof(sp_dmem) > len) return -1;
	memcpy(sp_dmem, in + pos, sizeof(sp_dmem)); pos += sizeof(sp_dmem);
	for (i = 0; i < 0x800; i++) SP_DMEM[i] = sp_dmem[i];

	return 0;
}

/* Re-arm interrupt queue + audio after restoring a body (shared by v1/v2). */
static void st_finish_load(void)
{
	u32 saved_next = r4300.next_interrupt;
	set_fpr_pointers(r4300.fcr31 & 0x3);
	init_interrupt();
	clear_queue();

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
	saved_ai_int = 0;

	resetAudioAfterLoad();
}

void savestates_save()
{
	char path[160];
	FILE *f;
	st_header hdr;
	u32 rdram_words = ST_RDRAM_WORDS;
	size_t body_len, comp_len;
	u8 *body, *comp;
	int use_z = 0;

	if (!rdram) return;

	/* Serialize body into a heap buffer. */
	body = (u8*)malloc(ST_BODY_MAX);
	if (!body) return;
	body_len = st_serialize_body(body, rdram_words);

	/* Try to compress with zlib. If it fits smaller (or equal), store version 2. */
	comp_len = compressBound((uLong)body_len);
	comp = (u8*)malloc(comp_len);
	if (comp && body_len <= 0x7FFFFFFF
	    && compress2(comp, (uLongf*)&comp_len, body, (uLong)body_len, 1) == Z_OK
	    && comp_len < body_len) {
		use_z = 1;
	}

	memset(&hdr, 0, sizeof(hdr));
	hdr.magic   = ST_MAGIC;
	hdr.version = use_z ? ST_VERSION_Z : ST_VERSION;
	hdr.slot    = savestates_slot;
	hdr.rdram_words = rdram_words;
	if (ROM_HEADER && ROM_HEADER->nom)
		memcpy(hdr.rom_name, ROM_HEADER->nom, 32);
	else
		memcpy(hdr.rom_name, "unknown", 8);

	st_build_name(path, sizeof(path));
	f = fopen(path, "wb");
	if (!f) { printf("Save State: no se pudo escribir %s", path); free(body); if (comp) free(comp); return; }

	fwrite(&hdr, 1, sizeof(hdr), f);
	if (use_z) {
		/* Body descriptor: uncompressed size then compressed size, then stream. */
		u32 ul = (u32)body_len, cl = (u32)comp_len;
		fwrite(&ul, 1, sizeof(ul), f);
		fwrite(&cl, 1, sizeof(cl), f);
		fwrite(comp, 1, comp_len, f);
	} else {
		fwrite(body, 1, body_len, f);
	}
	fclose(f);

	if (comp) free(comp);
	free(body);

	ps3_fb_snapshot(path);
	DBG_LOG("[ST] Saved slot %u -> %s (%s, %u->%u bytes)\n",
	        savestates_slot, path, use_z ? "zlib" : "raw",
	        (unsigned)body_len, use_z ? (unsigned)comp_len : (unsigned)body_len);
}

void savestates_load()
{
	char path[160];
	FILE *f;
	int i;
	st_header hdr;
	u32 rdram_words;
	u8 *body = NULL;
	size_t body_len = 0;
	int rc;

	st_build_name(path, sizeof(path));
	f = fopen(path, "rb");
	if (!f) { printf("Load State: no existe %s", path); return; }

	if (fread(&hdr, 1, sizeof(hdr), f) != sizeof(hdr) ||
	    hdr.magic != ST_MAGIC || (hdr.version != ST_VERSION && hdr.version != ST_VERSION_Z)) {
		fclose(f);
		printf("Load State: archivo invalido");
		return;
	}
	rdram_words = hdr.rdram_words ? hdr.rdram_words : ST_RDRAM_WORDS;

	if (hdr.version == ST_VERSION_Z) {
		/* Compressed body: read descriptor + stream, decompress to a buffer. */
		u32 dul = 0, dcl = 0;
		uLongf dLen = 0;
		if (fread(&dul, 1, sizeof(dul), f) != sizeof(dul) ||
		    fread(&dcl, 1, sizeof(dcl), f) != sizeof(dcl) || dul == 0 ||
		    dul > ST_BODY_MAX || dcl == 0 || dcl > ST_BODY_MAX) {
			fclose(f);
			printf("Load State: descriptor corrupto");
			return;
		}
		body = (u8*)malloc(dul);
		dLen = dul;   /* capacity for uncompress (uLongf is 64-bit on PPU) */
		{
			u8 *cd = (u8*)malloc(dcl);
			if (!body || !cd) { if (body) free(body); if (cd) free(cd); fclose(f); return; }
			if (fread(cd, 1, dcl, f) != dcl ||
			    uncompress(body, &dLen, cd, (uLong)dcl) != Z_OK) {
				free(cd); free(body); fclose(f);
				printf("Load State: descompresion fallo");
				return;
			}
			free(cd);
		}
		body_len = (size_t)dLen;
	} else {
		/* Legacy version 1: raw body, read it all into a buffer. */
		size_t raw_len = ST_BODY_FIXED + (size_t)rdram_words*4 + ST_BODY_SPDMEM;
		if (raw_len > ST_BODY_MAX * 4) { fclose(f); printf("Load State: invalido"); return; }
		body = (u8*)malloc(raw_len);
		if (!body) { fclose(f); return; }
		if (fread(body, 1, raw_len, f) != raw_len) {
			free(body); fclose(f);
			printf("Load State: archivo corrupto");
			return;
		}
		body_len = raw_len;
	}

	fclose(f);

	/* Restore emulator state from the body buffer, then re-arm interrupts. */
	rc = st_restore_body(body, body_len, rdram_words);
	(void)i;
	free(body);
	if (rc != 0) {
		printf("Load State: archivo corrupto (body)");
		return;
	}
	st_finish_load();

	DBG_LOG("[ST] Loaded slot %u <- %s\n", savestates_slot, path);
}
