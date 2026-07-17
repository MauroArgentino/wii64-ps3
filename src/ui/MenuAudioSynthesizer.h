#ifndef MENUAUDIOSYNTHESIZER_H
#define MENUAUDIOSYNTHESIZER_H

#include <stdint.h>
#include <ppu-types.h>
#include <math.h>

#define MENU_SAMPLE_RATE 48000
#define MAX_VOICES 24
#define SINE_TABLE_SIZE 2048
#define NUM_PAD_VOICES 6

struct SynthVoice {
    bool active;
    bool isPad;        // Voz perpetua del pad base (nunca muere)
    float freq;
    float phase;
    float step;
    float volume;
    float currentVol;
    float targetVol;
    float attackRate;
    float releaseRate;
    uint32_t samplesLeft;
    uint32_t delaySamples;
    // LFO para esta voz
    float lfoPhase;
    float lfoStep;
    float lfoDepth;
};

class MenuAudioSynthesizer {
public:
    MenuAudioSynthesizer();
    ~MenuAudioSynthesizer();

    void init();
    void stop();
    void process(float* buffer, uint32_t numSamples);

private:
    SynthVoice voices[MAX_VOICES];
    uint64_t sampleCounter;
    uint64_t nextChordChange;
    float sineTable[SINE_TABLE_SIZE];

    // Estado del pad continuo
    float padLfoPhase;
    float padLfoStep;

    // Estado de la intro
    float introProgress;   // 0.0 a 1.0
    bool introComplete;

    // Chords para evolucion
    int currentChord;
    float chordCrossfade;
    float prevChordVol;
    float nextChordVol;

    void initPadVoices();
    void playAmbientNote(float freq, float delaySec, float durationSec, float vol);
    void changeChord();

    float lookupSine(float phase);
    void generateSineTable();
};

extern MenuAudioSynthesizer g_menuAudioSynthesizer;

#endif
