#include <audio/audio.h>
#include <sys/thread.h>
#include <sys/systime.h>
#include <lv2/systime.h>
#include <string.h>
#include "../ui/MenuAudioSynthesizer.h"

extern char menuActive;

static sys_ppu_thread_t s_audio_tid;
static volatile bool s_audio_run = false;
static volatile bool s_port_open = false;

static u32  s_portNum   = 0;
static f32* s_portData  = NULL;
static u32  s_numBlocks = 0;

// Contador estático de bloques (igual que audio.c playOneBlock): NO usamos
// notify events ni readIndex porque ambos se comportan mal en RPCS3.
static u32 next_write_block = 0;

static int openMenuPort(void)
{
	audioPortParam pp;
	pp.numChannels = AUDIO_PORT_2CH;
	pp.numBlocks   = AUDIO_BLOCK_8;
	pp.attrib      = 0x1000;
	pp.level       = 1.0f;

	if (audioPortOpen(&pp, &s_portNum) != 0) return -1;

	audioPortConfig cfg;
	audioGetPortConfig(s_portNum, &cfg);
	s_portData  = (f32*)((u64)cfg.audioDataStart);
	s_numBlocks = cfg.numBlocks;
	if (s_numBlocks < 1) s_numBlocks = AUDIO_BLOCK_8;

	if (audioPortStart(s_portNum) != 0) {
		audioPortClose(s_portNum);
		return -1;
	}

	g_menuAudioSynthesizer.init();
	next_write_block = 0;
	s_port_open = true;
	return 0;
}

static void closeMenuPort(void)
{
	if (!s_port_open) return;
	g_menuAudioSynthesizer.stop();
	audioPortStop(s_portNum);
	audioPortClose(s_portNum);
	s_port_open = false;
}

// Pacing en tiempo real a 48kHz usando sysGetSystemTime() (alta resolución).
// El sysUsleep fijo atrasaba en RPCS3 y producía underruns ("diafragma roto").
// Acá escribimos cada bloque exactamente en su momento y dormimos solo el resto.
static void audio_thread(void* arg)
{
	while (s_audio_run) {
		if (menuActive == 1) {
			if (!s_port_open) {
				if (openMenuPort() != 0) { sysUsleep(50000); continue; }
			}

			u64 total_samples = 0;
			s64 start = sysGetSystemTime();

			while (s_audio_run && menuActive == 1) {
				s64 now = sysGetSystemTime();
				s64 due = start + (s64)(total_samples * 1000000ULL / 48000ULL);
				if (now >= due) {
					f32* buf = s_portData + 2 * AUDIO_BLOCK_SAMPLES * (next_write_block % s_numBlocks);
					g_menuAudioSynthesizer.process(buf, AUDIO_BLOCK_SAMPLES);
					next_write_block = (next_write_block + 1) % s_numBlocks;
					total_samples += AUDIO_BLOCK_SAMPLES;
					continue; // recupera bloques atrasados al instante
				}
				s64 remaining = due - now;
				if (remaining > 1000)
					sysUsleep((u32)(remaining - 900)); // deja ~0.9ms para ajuste fino
				else
					sysUsleep(100);
			}
		} else {
			// En el juego (menuActive==0): cerramos el puerto para NO chocar con el
			// audio del ROM (audio.c usa el puerto 1). Se reabre al volver al menú.
			if (s_port_open) closeMenuPort();
			sysUsleep(20000);
		}
	}

	closeMenuPort();
	sysThreadExit(0);
}

extern "C" void ps3_audio_init()
{
	if (s_audio_run) return;
	audioInit();
	s_audio_run = true;
	sysThreadCreate(&s_audio_tid, audio_thread, NULL, 100, 64*1024, THREAD_JOINABLE, (char*)"MenuAudioThread");
}

extern "C" void ps3_audio_exit()
{
	if (!s_audio_run) return;
	s_audio_run = false;
	u64 ret;
	sysThreadJoin(s_audio_tid, &ret);
	s_port_open = false;
}
