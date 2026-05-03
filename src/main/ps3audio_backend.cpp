#include <audio/audio.h>
#include <sys/thread.h>
#include <sys/event_queue.h>
#include <string.h>
#include "../ui/MenuAudioSynthesizer.h"

static sys_ppu_thread_t s_audio_tid;
static bool s_audio_run = false;
static u32 s_portNum;

extern "C" char menuActive; // Variable global del emulador que indica si el menú está abierto

static void audio_thread(void* arg)
{
    audioPortParam portParam;
    portParam.numChannels = AUDIO_PORT_2CH;
    portParam.numBlocks = AUDIO_BLOCK_16; // Más bloques para evitar cortes
    portParam.attrib = 0x1000; // Atributo EXT (usado por Hermes para estabilidad)
    portParam.level = 1.0f;    // Nivel de salida nominal

    if (audioPortOpen(&portParam, &s_portNum) != 0) return;

    audioPortConfig portConfig;
    audioGetPortConfig(s_portNum, &portConfig);

    sys_event_queue_t eventQ;
    sys_ipc_key_t key;
    audioCreateNotifyEventQueue(&eventQ, &key);
    audioSetNotifyEventQueue(key);

    audioPortStart(s_portNum);

    float* audioData = (float*)(uintptr_t)portConfig.audioDataStart;
    u32 blockSize = AUDIO_BLOCK_SAMPLES * 2 * sizeof(float);

    while (s_audio_run) {
        sys_event_t event;
        sysEventQueueReceive(eventQ, &event, 0);

        // event.data_1 contiene el índice del bloque que RSX/Audio terminó de leer
        u32 blockIdx = (u32)event.data_1;
        float* targetBuffer = (float*)((uint8_t*)audioData + (blockIdx * blockSize));

        if (menuActive) {
            // Si el menú está activo, nuestro sintetizador llena el buffer
            g_menuAudioSynthesizer.process(targetBuffer, AUDIO_BLOCK_SAMPLES);
        } else {
            // Si no, silencio (o el audio del juego si decides mezclarlo aquí)
            memset(targetBuffer, 0, blockSize);
        }
    }

    audioPortStop(s_portNum);
    audioRemoveNotifyEventQueue(key);
    audioPortClose(s_portNum);
    sysEventQueueDestroy(eventQ, 0);
    sysThreadExit(0);
}

extern "C" void ps3_audio_init()
{
    if (s_audio_run) return;
    audioInit();
    s_audio_run = true;
    // Prioridad 100 es segura y suficiente para audio en PS3 sin bloquear el sistema
    sysThreadCreate(&s_audio_tid, audio_thread, NULL, 100, 64*1024, THREAD_JOINABLE, (char*)"WiiAudioThread");
}

extern "C" void ps3_audio_exit()
{
    s_audio_run = false;
    u64 ret;
    sysThreadJoin(s_audio_tid, &ret);
    audioQuit();
}