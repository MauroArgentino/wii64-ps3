/**
 * Wii64 - audio.c
 * Copyright (C) 2007, 2008, 2009 Mike Slegeir
 * Copyright (C) 2007, 2008, 2009 emu_kidid
 * 
 * Low-level Audio plugin with linear interpolation & 
 * resampling to 32/48KHz for the GC/Wii
 *
 * Wii64 homepage: http://www.emulatemii.com
 * email address: tehpola@gmail.com
 *                emukidid@gmail.com
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
**/

/*  MEMORY USAGE:
     STATIC:
   	Audio Buffer: 8x 4096 bytes each (~32KB)
   	LWP Control Buffer: 1Kb
*/

#include "../../main/winlnxdefs.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <audio/audio.h>
#include <sys/event_queue.h>
#include <lv2/systime.h>

// Audio timing debug logging (prints to stdout) — DISABLED: causes RPCS3 crash
// #define AUDIO_DEBUG_LOG
#ifdef AUDIO_DEBUG_LOG
static s64 audio_log_t0 = 0; // first timestamp for relative time
#define ATLOG(fmt, ...) do { \
	if(!audio_log_t0) audio_log_t0 = sysGetSystemTime(); \
	printf("[ADEBUG %8.3f] " fmt "\n", (float)(sysGetSystemTime() - audio_log_t0) / 1000000.0f, ##__VA_ARGS__); \
} while(0)
#else
#define ATLOG(fmt, ...) do {} while(0)
#endif

// Uncomment to dump raw PCM to /dev_hdd0/tmp/audio_dump.pcm
// Format: s16le stereo, 48kHz
// Play: aplay -f S16_LE -r 48000 -c 2 audio_dump.pcm
// Or import in Audacity as Raw Audio -> Signed 16-bit PCM, Little-endian, 2 channels, 48000Hz
// // #define AUDIO_PCM_DUMP

// Uncomment to bypass N64 audio entirely and generate a pure 440Hz sine wave.
// If this sounds clean → our audio pipeline is the problem.
// If this also clicks → RPCS3 audio port has inherent crackling.
// #define AUDIO_SINE_TEST  // DISABLED: want real game audio, not 440Hz test tone

// Use a dedicated audio thread so R4300 isn't blocked during block writes.
// The thread drains ring buffer into PS3 blocks at the hardware rate,
// writing silence when no N64 data is available to keep the port alive.
// DISABLED: threaded path causes audio reverberation/distortion.
// The non-threaded synchronous path (a68ecdf) sounds clean.
// #define THREADED_AUDIO
#ifdef AUDIO_PCM_DUMP
static FILE *pcm_dump = NULL;
static int pcm_dump_frames = 0;
static int pcm_dump_ready = 0; // 0=waiting, 1=recording
static int pcm_silence_blocks = 0; // count consecutive silent blocks
#define PCM_DUMP_MAX_FRAMES (48000 * 60) // 60 seconds once audio detected
#define PCM_SILENCE_THRESHOLD 500       // min sample amplitude to count as audio
#define PCM_SILENCE_BLOCKS_NEEDED 3     // need this many non-silent blocks to start
#endif

#include "AudioPlugin.h"
#include "Audio_#1.1.h"

AUDIO_INFO AudioInfo;

#define NUM_BUFFERS 8
#define BUFFER_SIZE 4096
static char buffer[NUM_BUFFERS][BUFFER_SIZE] __attribute__((aligned(32)));
static int which_buffer = 0;
static unsigned int buffer_offset = 0;
static unsigned int read_pos = 0;
static unsigned int drain_level = 0; // bytes to drain in threaded mode
#define NEXT(x) (x=(x+1)%NUM_BUFFERS)
static unsigned int freq;
static unsigned int real_freq;
static float freq_ratio;
extern void dbg_printf(const char *fmt,...);
extern unsigned int usleep(unsigned int us);
// Buffer sizes must be multiples of (AUDIO_BLOCK_SAMPLES * 4 = 1024) so that
// play_buffer() drains every byte with no remainder.  Non-aligned sizes cause
// leftover samples to be silently discarded when advancing to the next buffer,
// producing periodic gaps that sound like helicopter/rotor noise.
static enum { BUFFER_SIZE_32_60 = 2112, BUFFER_SIZE_48_60 = 4096,
              BUFFER_SIZE_32_50 = 2560, BUFFER_SIZE_48_50 = 4096 } buffer_size;

#ifdef THREADED_AUDIO
#include <sys/thread.h>
#include <sys/sem.h>
static sys_ppu_thread_t audio_thread;
static sys_sem_t buffer_full;
static sys_sem_t buffer_empty;
static int   thread_running = 0;
#define AUDIO_STACK_SIZE 4096
static char  audio_stack[AUDIO_STACK_SIZE];
#define AUDIO_PRIORITY 100
static int   thread_buffer = 0;
static int   audio_paused = 0;
static inline void sem_init(sys_sem_t *sem, s32 init, s32 max){
	sys_sem_attr_t attr;
	memset(&attr, 0, sizeof(attr));
	attr.attr_protocol = SYS_SEM_ATTR_PROTOCOL;
	sysSemCreate(sem, &attr, init, max);
}
#define sem_wait(s)    sysSemWait((s), 0x7FFFFFFFFFFFFFFFULL)
#define sem_post(s)    sysSemPost((s), 1)
#define sem_destroy(s) sysSemDestroy(s)
#else // !THREADED_AUDIO
#define thread_buffer which_buffer
#endif

char audioEnabled;
u32 portNum;
audioPortParam params;
audioPortConfig config;
static u64 snd_key;
static sys_event_queue_t snd_queue;

EXPORT void CALL
AiDacrateChanged( int SystemType )
{
	// Taken from mupen_audio
	freq = 32000; //default to 32khz incase we get a bad systemtype
	switch (SystemType){
	      case SYSTEM_NTSC:
		freq = 48681812 / (*AudioInfo.AI_DACRATE_REG + 1);
		break;
	      case SYSTEM_PAL:
		freq = 49656530 / (*AudioInfo.AI_DACRATE_REG + 1);
		break;
	      case SYSTEM_MPAL:
		freq = 48628316 / (*AudioInfo.AI_DACRATE_REG + 1);
		break;
	}

	// PS3 audio port always plays at 48kHz. Always resample to 48kHz
	// to match the PS3 audio port playback rate, regardless of N64 native rate.
	real_freq = 48000;
	freq_ratio = (float)freq / (float)real_freq;

	dbg_printf("Initializing frequency: %d (N64 freq %d, resampling ratio %f)\r\n", real_freq, freq, freq_ratio);
	buffer_size = (SystemType != SYSTEM_PAL) ?
	               BUFFER_SIZE_48_60 : BUFFER_SIZE_48_50;
}

void fillBuffer(f32 *buf)
{
	u32 i;
#ifdef AUDIO_PCM_DUMP
	s32 has_audio = 0;
#endif

#ifdef AUDIO_SINE_TEST
	// Generate a pure 440Hz sine wave, bypassing all N64 audio.
	// This tests whether the RPCS3 audio port itself crackles.
	{
		static unsigned int sine_phase = 0;
		for(i=0;i<AUDIO_BLOCK_SAMPLES;i++) {
			float val = sinf((float)sine_phase * 2.0f * 3.14159265f / 48000.0f * 440.0f) * 0.5f;
			buf[i*2 + 0] = val;
			buf[i*2 + 1] = val;
			sine_phase++;
		}
		// Advance read_pos so play_buffer()'s drain loop terminates
		read_pos += AUDIO_BLOCK_SAMPLES * 4;
		return;
	}
#endif

#ifdef AUDIO_PCM_DUMP
	// Detect real audio: scan block for samples above silence threshold
	if(!pcm_dump_ready && pcm_dump_frames == 0){
		for(i=0;i<AUDIO_BLOCK_SAMPLES;i++) {
			s16 sL = *((s16*)&buffer[thread_buffer][read_pos + i*4]);
			s16 sR = *((s16*)&buffer[thread_buffer][read_pos + i*4 + 2]);
			if(abs(sL) > PCM_SILENCE_THRESHOLD || abs(sR) > PCM_SILENCE_THRESHOLD){
				has_audio = 1;
				break;
			}
		}
		if(has_audio){
			pcm_silence_blocks++;
			if(pcm_silence_blocks >= PCM_SILENCE_BLOCKS_NEEDED){
				pcm_dump = fopen("/dev_hdd0/tmp/audio_dump.pcm", "wb");
				pcm_dump_ready = 1;
			}
		} else {
			pcm_silence_blocks = 0;
		}
	}
#endif

	for(i=0;i<AUDIO_BLOCK_SAMPLES;i++) {
		s16 left = *((s16*)&buffer[thread_buffer][read_pos]);
		s16 right = *((s16*)&buffer[thread_buffer][read_pos + 2]);
		buf[i*2 + 0] = (f32)left/32768.0f;
		buf[i*2 + 1] = (f32)right/32768.0f;

#ifdef AUDIO_PCM_DUMP
		if(pcm_dump && pcm_dump_frames < PCM_DUMP_MAX_FRAMES){
			fwrite(&left, 2, 1, pcm_dump);
			fwrite(&right, 2, 1, pcm_dump);
			pcm_dump_frames++;
		}
#endif

		read_pos += 4;
	}

#ifdef AUDIO_PCM_DUMP
	if(pcm_dump && pcm_dump_frames >= PCM_DUMP_MAX_FRAMES){
		fclose(pcm_dump);
		pcm_dump = NULL;
	}
#endif
}

// Block writer: fills PS3 audio blocks at the correct rate.
// Uses a static block counter instead of polling readIndex (which behaves
// differently on RPCS3 vs real PS3 hardware).
static u32 next_write_block = 0;
static f32 *portDataStart = NULL;

s32 playOneBlock()
{
	f32 *buf;

	if(!portDataStart)
		portDataStart = (f32*)((u64)config.audioDataStart);

	buf = portDataStart + config.channelCount * AUDIO_BLOCK_SAMPLES * next_write_block;
	ATLOG("playOneBlock: writing to port block %d/%ld (portAddr=%08x)", next_write_block, config.numBlocks, (u32)portDataStart);
	fillBuffer(buf);
	next_write_block = (next_write_block + 1) % config.numBlocks;
	return 0;
}

#ifdef THREADED_AUDIO
static void play_buffer(void);
static void audio_thread_entry(void *arg){
	printf("[AUDIO] Thread started\n");
	play_buffer();
	printf("[AUDIO] Thread exiting\n");
	sysThreadExit(NULL);
}
#endif
static int port_started = 0;

#ifdef AUDIO_SINE_TEST
#include <math.h>
static void sine_boot_tone(void) {
	audioPortStart(portNum);
	port_started = 1;
	f32 *portData = (f32*)((u64)config.audioDataStart);
	if (!portData) { printf("[AUDIO] SINE_TEST: portData is NULL!\n"); return; }
	int block;
	unsigned int phase = 0;
	for (block = 0; block < config.numBlocks; block++) {
		f32 *buf = portData + config.channelCount * AUDIO_BLOCK_SAMPLES * block;
		int i;
		for (i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
			float val = sinf((float)phase * 2.0f * 3.14159265f / 48000.0f * 440.0f) * 0.3f;
			buf[i*2 + 0] = val;
			buf[i*2 + 1] = val;
			phase++;
		}
	}
	printf("[AUDIO] SINE_TEST: filled %ld blocks with 440Hz tone (port started)\n", config.numBlocks);
}
#endif

static void play_buffer(void){
#ifndef THREADED_AUDIO
	if(!port_started){
		audioPortStart(portNum);
		port_started = 1;
		ATLOG("audio port STARTED (non-threaded)");
	}
	// Non-threaded: drain ring buffer into PS3 blocks on R4300 thread.
	// No pacing — writes as fast as CPU produces audio.
	unsigned int blocks_written = 0;
	while(read_pos + AUDIO_BLOCK_SAMPLES * 4 <= buffer_offset){
		playOneBlock();
		blocks_written++;
	}
	if(blocks_written > 0)
		ATLOG("non-threaded drain: wrote %u blocks buf_off=%u read_pos=%u", blocks_written, buffer_offset, read_pos);
#else // THREADED_AUDIO
	// Audio thread: waits for add_to_buffer() to fill a buffer, drains it
	// into PS3 audio blocks.
	while(thread_running){
		// Block until add_to_buffer() signals data is ready
		sem_wait(buffer_full);
		if(!thread_running) break;

		// Start port on first buffer (thread owns port lifecycle)
		if(!port_started){
			audioPortStart(portNum);
			port_started = 1;
			ATLOG("audio port STARTED");
		}

		// Drain this buffer into PS3 audio blocks
		unsigned int level = drain_level;
		unsigned int blocks_written = 0;
		while(read_pos + AUDIO_BLOCK_SAMPLES * 4 <= level){
			playOneBlock();
			blocks_written++;
		}
		ATLOG("thread drain: level=%u blocks=%u next_write=%d", level, blocks_written, next_write_block);

		// Signal add_to_buffer() that this buffer slot is free
		NEXT(thread_buffer);
		read_pos = 0;
		sem_post(buffer_empty);
	}
	printf("[AUDIO] Thread leaving loop\n");
#endif
}

static unsigned int input_frames_left = 0;

static void inline copy_to_buffer(int* buffer, int* stream, unsigned int out_length, unsigned int in_frames){
	// Cubic Hermite (Catmull-Rom) resampler: uses 4 input samples per output
	// sample for C1-continuous output (smooth first derivative).
	unsigned int di;
	float si;
	int max_idx = (in_frames > 0) ? (int)in_frames - 1 : 0;
	for(di = 0, si = 0.0f; di < out_length; ++di, si += freq_ratio){
		float t = si - floorf(si);
		float t2 = t * t;
		float t3 = t2 * t;
		int idx = (int)si;

		int idx0 = (idx > 0) ? idx - 1 : idx;
		int idx1 = idx;
		int idx2 = (idx + 1 <= max_idx) ? idx + 1 : max_idx;
		int idx3 = (idx + 2 <= max_idx) ? idx + 2 : max_idx;

		short* s0 = (short*)(stream + idx0);
		short* s1 = (short*)(stream + idx1);
		short* s2 = (short*)(stream + idx2);
		short* s3 = (short*)(stream + idx3);

		short* osample = (short*)(buffer + di);

		float l = 0.5f * (
			(2.0f * s1[0]) +
			(-s0[0] + s2[0]) * t +
			(2.0f*s0[0] - 5.0f*s1[0] + 4.0f*s2[0] - s3[0]) * t2 +
			(-s0[0] + 3.0f*s1[0] - 3.0f*s2[0] + s3[0]) * t3
		);
		float r = 0.5f * (
			(2.0f * s1[1]) +
			(-s0[1] + s2[1]) * t +
			(2.0f*s0[1] - 5.0f*s1[1] + 4.0f*s2[1] - s3[1]) * t2 +
			(-s0[1] + 3.0f*s1[1] - 3.0f*s2[1] + s3[1]) * t3
		);

		osample[0] = (short)(l < -32768.0f ? -32768 : (l > 32767.0f ? 32767 : l + 0.5f));
		osample[1] = (short)(r < -32768.0f ? -32768 : (r > 32767.0f ? 32767 : r + 0.5f));
	}
}

static void inline add_to_buffer(void* stream, unsigned int length){
	unsigned int stream_offset = 0;
	unsigned int rlengthi;
	unsigned int lengthLeft = length >> 2;
	unsigned int rlengthLeft = (unsigned int)ceilf((float)lengthLeft / freq_ratio);

	input_frames_left = lengthLeft;
	ATLOG("add_to_buffer: length=%u in_frames=%u out_frames=%u buf_off=%u/%u", length, lengthLeft, rlengthLeft, buffer_offset, buffer_size);

	while(input_frames_left > 0){
		unsigned int avail_out = (buffer_size - buffer_offset) >> 2;
		rlengthi = (rlengthLeft < avail_out) ? rlengthLeft : avail_out;
		if(rlengthi == 0) break;

		unsigned int in_consumed = 0;
		if(rlengthi > 0)
			in_consumed = (unsigned int)((float)(rlengthi - 1) * freq_ratio) + 1;
		if(in_consumed > input_frames_left) in_consumed = input_frames_left;

#ifdef THREADED_AUDIO
		sem_wait(buffer_empty);
#endif
		copy_to_buffer((int*)(buffer[which_buffer] + buffer_offset),
		               (int*)(stream + stream_offset), rlengthi, in_consumed);

		buffer_offset += rlengthi << 2;

		stream_offset += in_consumed << 2;
		input_frames_left -= in_consumed;
		rlengthLeft -= rlengthi;

		if(buffer_offset < buffer_size){
			if(input_frames_left == 0){
#ifdef THREADED_AUDIO
				ATLOG("add_to_buffer: partial fill buf_off=%u/%u (waiting for more)", buffer_offset, buffer_size);
				sem_post(buffer_empty);
#endif
				return;
			}
			continue;
		}

		// Buffer is full — signal audio thread
		ATLOG("add_to_buffer: BUFFER FULL buf=%d drain_level=%u -> signaling thread", which_buffer, buffer_offset);
#ifdef THREADED_AUDIO
		// Ensure buffer data is visible to audio thread before signaling
		__asm__ __volatile__("lwsync" ::: "memory");
		drain_level = buffer_offset;
		sem_post(buffer_full);
#else
		play_buffer();
#endif

		NEXT(which_buffer);
		buffer_offset = 0;
#ifndef THREADED_AUDIO
		read_pos = 0;
#endif
	}
}

EXPORT void CALL
AiLenChanged( void )
{
	if(!audioEnabled) return;

	static int aiLogCnt = 0;
	short* stream = (short*)(AudioInfo.RDRAM +
		         (*AudioInfo.AI_DRAM_ADDR_REG & 0xFFFFFF));
	unsigned int length = *AudioInfo.AI_LEN_REG;

	if (aiLogCnt < 20) {
		aiLogCnt++;
		printf("[AUDIO] AiLenChanged#%d: addr=%08X len=%u freq=%u ratio=%.3f\n",
			aiLogCnt, *AudioInfo.AI_DRAM_ADDR_REG & 0xFFFFFF, length, freq, freq_ratio);
	}
	add_to_buffer(stream, length);
}

EXPORT DWORD CALL
AiReadLength( void )
{
	// I don't know if this is the data they're trying to get
	return 0;
	//return AUDIO_GetDMABytesLeft();
}

EXPORT void CALL
AiUpdate( BOOL Wait )
{
}

EXPORT void CALL
CloseDLL( void )
{
}

EXPORT void CALL
DllAbout( HWND hParent )
{
	printf ("Gamecube audio plugin\n\tby Mike Slegeir" );
}

EXPORT void CALL
DllConfig ( HWND hParent )
{
}

EXPORT void CALL
DllTest ( HWND hParent )
{
}

EXPORT void CALL
GetDllInfo( PLUGIN_INFO * PluginInfo )
{
	PluginInfo->Version = 0x0101;
	PluginInfo->Type    = PLUGIN_TYPE_AUDIO;
	sprintf(PluginInfo->Name,"Gamecube audio plugin\n\tby Mike Slegeir");
	PluginInfo->NormalMemory  = TRUE;
	PluginInfo->MemoryBswaped = TRUE;
}

EXPORT BOOL CALL
InitiateAudio( AUDIO_INFO Audio_Info )
{
	AudioInfo = Audio_Info;
	
	// audioInit() is a one-time global init — don't reinitialize on ROM switch
	static int audio_initialized = 0;
	if (!audio_initialized) {
		s32 ret = audioInit();
		dbg_printf("audioInit: %08x\n",ret);
		audio_initialized = 1;
	}

	params.numChannels = AUDIO_PORT_2CH;
	params.numBlocks = AUDIO_BLOCK_8;
	params.attrib = 0x1000;
	params.level = 1.0f;
	s32 ret = audioPortOpen(&params,&portNum);
	dbg_printf("audioPortOpen: %08x\n",ret);
	dbg_printf("      portNum: %d\n",portNum);

	ret = audioGetPortConfig(portNum,&config);
	dbg_printf("audioGetPortConfig: %08x\n",ret);
	dbg_printf("config.readIndex: %08x\n",config.readIndex);
	dbg_printf("config.status: %d\n",config.status);
	dbg_printf("config.channelCount: %ld\n",config.channelCount);
	dbg_printf("config.numBlocks: %ld\n",config.numBlocks);
	dbg_printf("config.portSize: %d\n",config.portSize);
	dbg_printf("config.audioDataStart: %08x\n",config.audioDataStart);

	// Debug log: port geometry
	printf("[ADEBUG] PORT: numBlocks=%ld blockSamples=%d blockSize=%ld totalBytes=%d ch=%ld dataStart=%08x\n",
		config.numBlocks, AUDIO_BLOCK_SAMPLES,
		(long)(config.channelCount * AUDIO_BLOCK_SAMPLES * sizeof(f32)),
		config.portSize, config.channelCount, config.audioDataStart);

	ret = audioCreateNotifyEventQueue(&snd_queue,&snd_key);
	dbg_printf("audioCreateNotifyEventQueue: %08x\n",ret);
	dbg_printf("snd_queue: %16lx\n",(long unsigned int)snd_queue);
	dbg_printf("snd_key: %16lx\n",snd_key);

	ret = audioSetNotifyEventQueue(snd_key);
	dbg_printf("audioSetNotifyEventQueue: %08x\n",ret);

	ret = sysEventQueueDrain(snd_queue);
	dbg_printf("sysEventQueueDrain: %08x\n",ret);

	ATLOG("InitiateAudio: freq=%u real_freq=%u ratio=%.3f", freq, real_freq, freq_ratio);

#ifdef AUDIO_SINE_TEST
	// sine_boot_tone(); // DISABLED with AUDIO_SINE_TEST
#endif

	return TRUE;
}

EXPORT void CALL RomOpen()
{
#ifdef THREADED_AUDIO
	sem_init(&buffer_full, 0, NUM_BUFFERS);
	sem_init(&buffer_empty, NUM_BUFFERS, NUM_BUFFERS);
	thread_running = 1;
	printf("[AUDIO] RomOpen: creating audio thread\n");
	int ret = sysThreadCreate(&audio_thread, audio_thread_entry, NULL, AUDIO_PRIORITY, AUDIO_STACK_SIZE, THREAD_JOINABLE, "audio");
	printf("[AUDIO] sysThreadCreate returned %d\n", ret);
	thread_buffer = which_buffer = 0;
	read_pos = 0;
	drain_level = 0;
	audio_paused = 1;
#endif
}

EXPORT void CALL
RomClosed( void )
{
	// Signal thread to exit and wake it if blocked on semaphore
#ifdef THREADED_AUDIO
	thread_running = 0;
	sem_post(buffer_full);
	sysThreadJoin(audio_thread, NULL);
	sem_destroy(buffer_full);
	sem_destroy(buffer_empty);
	audio_paused = 0;
#endif

	// Stop and close audio port
	next_write_block = 0;
	port_started = 0;
	portDataStart = NULL;
	buffer_offset = 0;
	read_pos = 0;
	drain_level = 0;

	int ret = audioPortStop(portNum);
	dbg_printf("audioPortStop: %08x\n",ret);

	// Remove event queue before closing port
	audioRemoveNotifyEventQueue(snd_key);
	sysEventQueueDestroy(snd_queue, 0);

	// Close the audio port to free the handle
	ret = audioPortClose(portNum);
	dbg_printf("audioPortClose: %08x\n",ret);
}

EXPORT void CALL
ProcessAlist( void )
{
}

void pauseAudio(void){
#ifdef THREADED_AUDIO
	audio_paused = 1;
#endif
	int ret = audioPortStop(portNum);
	dbg_printf("audioPortStop: %08x\n",ret);
}

void resumeAudio(void){
#ifdef THREADED_AUDIO
	if(audio_paused && audioEnabled){
		audio_paused = 0;
	}
#endif
}

