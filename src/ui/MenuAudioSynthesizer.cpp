#include "MenuAudioSynthesizer.h"
#include <string.h>

MenuAudioSynthesizer g_menuAudioSynthesizer;

MenuAudioSynthesizer::MenuAudioSynthesizer()
    : sampleCounter(0), nextChordChange(0),
      padLfoPhase(0.0f), padLfoStep(0.0f),
      introProgress(0.0f), introComplete(false),
      currentChord(0), chordCrossfade(1.0f), prevChordVol(1.0f), nextChordVol(0.0f)
{
    generateSineTable();
    for (int i = 0; i < MAX_VOICES; i++) {
        voices[i].active = false;
        voices[i].isPad = false;
        voices[i].waveShape = 0;
        voices[i].freqEnd = 0.0f;
        voices[i].freqInc = 0.0f;
        voices[i].envDecay = 0.0f;
        voices[i].reachedPeak = false;
    }
    pendingHover = false;
}

MenuAudioSynthesizer::~MenuAudioSynthesizer() {}

void MenuAudioSynthesizer::generateSineTable() {
    for (int i = 0; i < SINE_TABLE_SIZE; i++)
        sineTable[i] = sinf(((float)i / (float)SINE_TABLE_SIZE) * 6.283185f);
}

float MenuAudioSynthesizer::lookupSine(float phase) {
    int index = (int)(phase * (float)(SINE_TABLE_SIZE - 1));
    index &= (SINE_TABLE_SIZE - 1);
    return sineTable[index];
}

// Onda triangular: fase 0..1 -> -1..1..-1 (dos rampas lineales)
float MenuAudioSynthesizer::lookupTriangle(float phase) {
    float p = phase - (float)(int)phase;
    float t = (p < 0.5f) ? (p * 2.0f) : ((1.0f - p) * 2.0f);
    return t * 2.0f - 1.0f;
}

void MenuAudioSynthesizer::init() {
    sampleCounter = 0;
    introProgress = 0.0f;
    introComplete = false;
    currentChord = 0;
    chordCrossfade = 1.0f;
    prevChordVol = 1.0f;
    nextChordVol = 0.0f;
    padLfoPhase = 0.0f;

    nextChordChange = MENU_SAMPLE_RATE * 3; // El ambiente empieza tras la intro (3s)

    // Reiniciar todas las voces
    for (int i = 0; i < MAX_VOICES; i++) {
        voices[i].active = false;
        voices[i].isPad = false;
        voices[i].waveShape = 0;
        voices[i].freqEnd = 0.0f;
        voices[i].freqInc = 0.0f;
        voices[i].envDecay = 0.0f;
    }
    pendingHover = false;

    // Intro Wii + programar el ambiente evolutivo
    programIntro();
}

void MenuAudioSynthesizer::stop() {
    for (int i = 0; i < MAX_VOICES; i++) {
        voices[i].active = false;
        voices[i].isPad = false;
        voices[i].waveShape = 0;
        voices[i].freqEnd = 0.0f;
        voices[i].freqInc = 0.0f;
        voices[i].envDecay = 0.0f;
    }
    pendingHover = false;
}

// Blip de navegación: triángulo que barre 420Hz->150Hz en 40ms con
// decaimiento exponencial (equivalente al playWiiHover del menú Wii).
// Se llama desde el hilo principal; solo marca un flag para process().
void MenuAudioSynthesizer::triggerHoverBlip() {
    pendingHover = true;
}

void MenuAudioSynthesizer::spawnHoverBlip() {
    for (int i = NUM_PAD_VOICES; i < MAX_VOICES; i++) {
        if (!voices[i].active) {
            voices[i].active = true;
            voices[i].isPad = false;
            voices[i].waveShape = 1;                 // triangle
            voices[i].freq = 420.0f;
            voices[i].freqEnd = 150.0f;
            // 40ms -> 1920 muestras a 48kHz; barrido lineal 420->150
            voices[i].freqInc = (150.0f - 420.0f) / 1920.0f;
            voices[i].phase = 0.0f;
            voices[i].step = 420.0f / (float)MENU_SAMPLE_RATE;
            voices[i].volume = 0.11f;
            voices[i].targetVol = 0.11f;
            voices[i].currentVol = 0.11f;            // ataque inmediato
            voices[i].attackRate = 0.0f;
            voices[i].releaseRate = 0.9f;
            voices[i].reachedPeak = true;              // pico inmediato
            // Decaimiento 0.11 -> ~0.0011 en 1920 muestras (exponencial)
            voices[i].envDecay = 0.9976f;
            voices[i].samplesLeft = 1920;
            voices[i].delaySamples = 0;
            voices[i].lfoPhase = 0.0f;
            voices[i].lfoStep = 0.0f;
            voices[i].lfoDepth = 0.0f;
            return;
        }
    }
}

// Acorde Cm9 perpetuo: C2, Eb3, G3, Bb3, D4, C4
// Las frecuencias base se modulan lentamente con LFO para crear respiración
void MenuAudioSynthesizer::initPadVoices() {
    float padFreqs[NUM_PAD_VOICES] = {
        65.41f,   // C2 (sub bass)
        155.56f,  // Eb3
        196.00f,  // G3
        233.08f,  // Bb3
        293.66f,  // D4
        130.81f   // C3
    };
    float padVols[NUM_PAD_VOICES] = {
        0.22f, 0.14f, 0.13f, 0.10f, 0.08f, 0.16f
    };
    // LFO steps: frecuencias irracionales para que nunca se repita exactamente
    float lfoSteps[NUM_PAD_VOICES] = {
        0.000083f,  // ~0.083 Hz (12s cycle)
        0.000131f,  // ~0.131 Hz
        0.000067f,  // ~0.067 Hz
        0.000103f,  // ~0.103 Hz
        0.000051f,  // ~0.051 Hz
        0.000119f   // ~0.119 Hz
    };
    float lfoDepths[NUM_PAD_VOICES] = {
        0.03f, 0.06f, 0.05f, 0.07f, 0.08f, 0.04f
    };

    for (int i = 0; i < NUM_PAD_VOICES; i++) {
        voices[i].active = true;
        voices[i].isPad = true;
        voices[i].freq = padFreqs[i];
        voices[i].phase = 0.0f;
        voices[i].step = padFreqs[i] / (float)MENU_SAMPLE_RATE;
        voices[i].volume = padVols[i];
        voices[i].targetVol = padVols[i];
        voices[i].currentVol = 0.0f; // Fade in lentamente
        voices[i].attackRate = padVols[i] / ((float)MENU_SAMPLE_RATE * 3.0f); // 3s fade in
        voices[i].releaseRate = 1.0f;
        voices[i].samplesLeft = 0; // Nunca muere
        voices[i].delaySamples = 0;
        voices[i].lfoPhase = 0.0f;
        voices[i].lfoStep = lfoSteps[i];
        voices[i].lfoDepth = lfoDepths[i];
    }
}

// Nota ambient efímera (para matices)
void MenuAudioSynthesizer::playAmbientNote(float freq, float delaySec, float durationSec, float vol) {
    for (int i = NUM_PAD_VOICES; i < MAX_VOICES; i++) {
        if (!voices[i].active) {
            voices[i].active = true;
            voices[i].isPad = false;
            voices[i].waveShape = 0;
            voices[i].freq = freq;
            voices[i].freqEnd = 0.0f;
            voices[i].freqInc = 0.0f;
            voices[i].phase = 0.0f;
            voices[i].step = freq / (float)MENU_SAMPLE_RATE;
            voices[i].volume = vol * 0.12f;
            voices[i].targetVol = vol * 0.12f;
            voices[i].currentVol = 0.0f;
            voices[i].attackRate = voices[i].volume / ((float)MENU_SAMPLE_RATE * 0.5f); // 0.5s attack
            voices[i].releaseRate = 0.99997f;
            voices[i].envDecay = 0.0f;
            voices[i].samplesLeft = (uint32_t)(durationSec * (float)MENU_SAMPLE_RATE);
            voices[i].delaySamples = (uint32_t)(delaySec * (float)MENU_SAMPLE_RATE);
            voices[i].lfoPhase = 0.0f;
            voices[i].lfoStep = (float)(rand() % 100) * 0.00001f + 0.00005f;
            voices[i].lfoDepth = 0.03f;
            return;
        }
    }
}

// Nota general: forma de onda (0=sine,1=triangle), ataque lineal y
// decaimiento exponencial a silencio en la duración restante.
void MenuAudioSynthesizer::playNote(float freq, float delaySec, float durationSec, float vol, int shape, float attackSec) {
    for (int i = NUM_PAD_VOICES; i < MAX_VOICES; i++) {
        if (!voices[i].active) {
            voices[i].active = true;
            voices[i].isPad = false;
            voices[i].waveShape = shape;
            voices[i].freq = freq;
            voices[i].freqEnd = 0.0f;
            voices[i].freqInc = 0.0f;
            voices[i].phase = 0.0f;
            voices[i].step = freq / (float)MENU_SAMPLE_RATE;
            voices[i].volume = vol;
            voices[i].targetVol = vol;
            voices[i].currentVol = 0.0f;
            voices[i].attackRate = (attackSec > 0.0f) ? vol / ((float)MENU_SAMPLE_RATE * attackSec) : vol;
            voices[i].releaseRate = 0.9f;
            voices[i].reachedPeak = false;             // ADSR con ataque
            float remainSec = durationSec - attackSec;
            if (remainSec < 0.01f) remainSec = 0.01f;
            if (vol > 0.0f)
                voices[i].envDecay = powf(0.001f / vol, 1.0f / ((float)MENU_SAMPLE_RATE * remainSec));
            else
                voices[i].envDecay = 0.0f;
            voices[i].samplesLeft = (uint32_t)(durationSec * (float)MENU_SAMPLE_RATE);
            voices[i].delaySamples = (uint32_t)(delaySec * (float)MENU_SAMPLE_RATE);
            voices[i].lfoPhase = 0.0f;
            voices[i].lfoStep = 0.0f;
            voices[i].lfoDepth = 0.0f;
            return;
        }
    }
}

// Intro Wii: acorde Cmaj9 en "swell" + plinks cristalinos (playWiiIntro del .html)
void MenuAudioSynthesizer::programIntro() {
    const float cmaj9[5] = {261.63f, 329.63f, 392.00f, 493.88f, 587.33f}; // C4 E4 G4 B4 D5
    for (int i = 0; i < 5; i++)
        playNote(cmaj9[i], 0.0f, 4.0f, 0.12f, 0, 0.1f); // swell 0.1s, decay 4s
    // Plinks cristalinos
    playNote(1046.50f, 0.5f, 0.6f, 0.05f, 0, 0.05f); // C6
    playNote(1318.51f, 0.8f, 0.6f, 0.05f, 0, 0.05f); // E6
    playNote(1567.98f, 1.2f, 0.6f, 0.05f, 0, 0.05f); // G6
}

// Ambiente procedural: acorde aleatorio (triángulo) + pings melódicos (generateEvolution)
void MenuAudioSynthesizer::generateAmbient() {
    const float chordPool[3][4] = {
        {174.61f, 220.00f, 261.63f, 329.63f}, // Fmaj7
        {261.63f, 329.63f, 392.00f, 493.88f}, // Cmaj7
        {196.00f, 246.94f, 293.66f, 392.00f}  // G6
    };
    int c = rand() % 3;
    for (int i = 0; i < 4; i++)
        playNote(chordPool[c][i], 0.0f, 12.0f, 0.02f, 1, 3.6f); // triángulo, 12s, ataque 30% (3.6s)

    const float scale[5] = {523.25f, 659.25f, 783.99f, 880.00f, 1046.50f};
    for (int i = 0; i < 2; i++) {
        float delay = 1.0f + (float)(rand() % 8000) / 1000.0f; // 1..9s
        playNote(scale[rand() % 5], delay, 4.0f, 0.03f, 0, 1.2f); // sine, 4s, ataque 30% (1.2s)
    }
}

// Cambios de acorde lentos para evolucionar el pad
void MenuAudioSynthesizer::changeChord() {
    currentChord = (currentChord + 1) % 4;
    prevChordVol = 1.0f;
    nextChordVol = 0.0f;
    chordCrossfade = 0.0f;

    // Ajustar frecuencias del pad hacia el nuevo acorde con crossfade suave
    const float chords[4][NUM_PAD_VOICES] = {
        {65.41f, 155.56f, 196.00f, 233.08f, 293.66f, 130.81f}, // Cm9
        {58.27f, 146.83f, 174.61f, 220.00f, 261.63f, 116.54f},  // Bbmaj7
        {61.74f, 164.81f, 207.65f, 246.94f, 311.13f, 123.47f},  // C#maj7
        {73.42f, 146.83f, 185.00f, 220.00f, 277.18f, 146.83f}   // Dm7
    };

    for (int i = 0; i < NUM_PAD_VOICES; i++) {
        voices[i].freq = chords[currentChord][i];
        voices[i].step = chords[currentChord][i] / (float)MENU_SAMPLE_RATE;
    }
}

void MenuAudioSynthesizer::process(float* buffer, uint32_t numSamples) {
    memset(buffer, 0, numSamples * 2 * sizeof(float));

    // --- Ambiente evolutivo: cada 10s se genera un acorde ambiental nuevo ---
    if (sampleCounter >= nextChordChange) {
        generateAmbient();
        nextChordChange = sampleCounter + (uint64_t)((float)MENU_SAMPLE_RATE * 10.0f); // Cada 10s
    }

    // --- LFO maestro del pad (respiración global) ---
    padLfoStep = 0.000077f; // ~0.077 Hz, ciclo de ~13 segundos
    float masterLfo = lookupSine(padLfoPhase);
    padLfoPhase += padLfoStep;
    if (padLfoPhase > 1.0f) padLfoPhase -= 1.0f;

    // --- Consumir blip de navegación pendiente (disparado desde el menú) ---
    if (pendingHover) {
        pendingHover = false;
        spawnHoverBlip();
    }

    // --- Mezclar voces ---
    for (int i = 0; i < MAX_VOICES; i++) {
        SynthVoice& v = voices[i];
        if (!v.active) continue;

        uint32_t s = 0;

        // Manejo de delay
        if (v.delaySamples > numSamples) {
            v.delaySamples -= numSamples;
            continue;
        } else if (v.delaySamples > 0) {
            s = v.delaySamples;
            v.delaySamples = 0;
        }

        for (; s < numSamples; s++) {
            // Modulación LFO de frecuencia (vibrato sutil)
            float lfoMod = lookupSine(v.lfoPhase) * v.lfoDepth;
            v.lfoPhase += v.lfoStep;
            if (v.lfoPhase > 1.0f) v.lfoPhase -= 1.0f;

            // Glide de frecuencia (solo blips de navegación)
            if (v.freqInc != 0.0f) v.freq += v.freqInc;

            float currentStep = (v.freq * (1.0f + lfoMod)) / (float)MENU_SAMPLE_RATE;
            float sample = (v.waveShape == 1) ? lookupTriangle(v.phase) : lookupSine(v.phase);

            // Ataque suave (una sola vez hasta el pico)
            if (!v.reachedPeak) {
                v.currentVol += v.attackRate;
                if (v.currentVol >= v.targetVol) {
                    v.currentVol = v.targetVol;
                    v.reachedPeak = true;
                }
            }

            // Decaimiento exponencial (solo tras el pico -- ADSR correcto)
            if (v.envDecay != 0.0f && v.reachedPeak) v.currentVol *= v.envDecay;

            // Aplicar LFO de amplitud (solo para voces del pad)
            float ampMod = 1.0f;
            if (v.isPad) {
                // Respiar: modulación de amplitud sutil
                ampMod = 0.85f + 0.15f * masterLfo;
            }

            float finalOut = sample * v.currentVol * ampMod;

            // Stereo: voces pares ligeramente a la izquierda, impares a la derecha
            float pan = (i % 2 == 0) ? 0.55f : 0.45f;
            buffer[s * 2]     += finalOut * pan;
            buffer[s * 2 + 1] += finalOut * (1.0f - pan);

            v.phase += currentStep;
            if (v.phase > 1.0f) v.phase -= 1.0f;

            // Voces efimeras se apagan
            if (!v.isPad) {
                if (v.samplesLeft > 0) {
                    v.samplesLeft--;
                } else {
                    // Release
                    v.currentVol *= v.releaseRate;
                    if (v.currentVol < 0.00001f) {
                        v.active = false;
                        break;
                    }
                }
            }
        }
    }

    sampleCounter += numSamples;
}
