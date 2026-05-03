#ifndef MENUAUDIOSYNTHESIZER_H
#define MENUAUDIOSYNTHESIZER_H

#include <vector>
#include <stdint.h>
#include <ppu-types.h>
#include <math.h>

#define WII_SAMPLE_RATE 48000
#define MAX_VOICES 16
#define SINE_TABLE_SIZE 2048

enum WaveType { WAVE_SINE, WAVE_TRIANGLE };

struct AudioVoice {
    bool active;
    WaveType type;
    float freq;
    float phase;
    float step;       // Incremento de fase pre-calculado
    float volume;
    float currentVol;
    float attackStep;
    float decayRate;
    uint32_t samplesLeft;
    uint32_t delaySamples;
};

class MenuAudioSynthesizer { // Renombrado
public:
   MenuAudioSynthesizer();
    ~MenuAudioSynthesizer();

    void init();
    void startIntro();
    void process(float* buffer, uint32_t numSamples);
    void updateLogic(); // Llamar desde el hilo principal (update del menú)

private:
    AudioVoice voices[MAX_VOICES];
    uint64_t sampleCounter;
    uint64_t nextEvolutionSamples;
    float sineTable[SINE_TABLE_SIZE];
    bool isPlaying;
    bool introFinished;

    void playNote(float freq, float delaySec, float durationSec, float vol, WaveType type);
    void generateEvolution();
    
    // Generadores de ondas
    float lookupSine(float phase);
    float calculateTriangle(float phase);
};

// Instancia global para facilitar el acceso desde el hilo de audio
extern MenuAudioSynthesizer g_menuAudioSynthesizer; // Renombrado
#endif // MENUAUDIOSYNTHESIZER_H