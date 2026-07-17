#include <audio/audio.h>
#include <sys/thread.h>
#include <sys/event_queue.h>
#include <string.h>
#include "../ui/MenuAudioSynthesizer.h"

static sys_ppu_thread_t s_audio_tid;
static bool s_audio_run = false;
static u32 s_portNum;
static bool s_port_open = false;

extern char menuActive;

static void audio_thread(void* arg)
{
    audioPortParam portParam;
    portParam.numChannels = AUDIO_PORT_2CH;
    portParam.numBlocks = AUDIO_BLOCK_8;
    portParam.attrib = 0x1000;
    portParam.level = 1.0f;

    if (audioPortOpen(&portParam, &s_portNum) != 0) return;
    s_port_open = true;

    audioPortConfig portConfig;
    audioGetPortConfig(s_portNum, &portConfig);

    sys_event_queue_t eventQ;
    sys_ipc_key_t key;
    audioCreateNotifyEventQueue(&eventQ, &key);
    audioSetNotifyEventQueue(key);

    audioPortStart(s_portNum);

    float* audioData = (float*)(uintptr_t)portConfig.audioDataStart;
    u32 blockSize = AUDIO_BLOCK_SAMPLES * 2 * sizeof(float);

    g_menuAudioSynthesizer.init();

    while (s_audio_run) {
        sys_event_t event;
        sysEventQueueReceive(eventQ, &event, 0);

        u32 blockIdx = (u32)event.data_1;
        float* targetBuffer = (float*)((uint8_t*)audioData + (blockIdx * blockSize));

        if (menuActive == 1) {
            g_menuAudioSynthesizer.process(targetBuffer, AUDIO_BLOCK_SAMPLES);
        } else {
            memset(targetBuffer, 0, blockSize);
        }
    }

    g_menuAudioSynthesizer.stop();

    audioPortStop(s_portNum);
    audioRemoveNotifyEventQueue(key);
    audioPortClose(s_portNum);
    sysEventQueueDestroy(eventQ, 0);
    s_port_open = false;
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
}
