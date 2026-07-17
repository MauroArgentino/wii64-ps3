#include "MenuAudioSynthesizer.h"
#include <string.h>

MenuAudioSynthesizer g_menuAudioSynthesizer;

MenuAudioSynthesizer::MenuAudioSynthesizer()
    : sampleCounter(0), nextChordChange(0), introProgress(0.0f), introComplete(false),
      currentChord(0), chordCrossfade(1.0f), prevChordVol(1.0f), nextChordVol(0.0f),
      padLfoPhase(0.0f), padLfoStep(0.0f)
{
    generateSineTable();
    for (int i = 0; i < MAX_VOICES; i++) {
        voices[i].active = false;
        voices[i].isPad = false;
    }
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

void MenuAudioSynthesizer::init() {
    sampleCounter = 0;
    introProgress = 0.0f;
    introComplete = false;
    currentChord = 0;
    chordCrossfade = 1.0f;
    prevChordVol = 1.0f;
    nextChordVol = 0.0f;
    padLfoPhase = 0.0f;

    // Chord change cada 15 segundos
    nextChordChange = MENU_SAMPLE_RATE * 15;

    // Reiniciar todas las voces
    for (int i = 0; i < MAX_VOICES; i++) {
        voices[i].active = false;
        voices[i].isPad = false;
    }

    // Iniciar el pad base perpetuo
    initPadVoices();
}

void MenuAudioSynthesizer::stop() {
    for (int i = 0; i < MAX_VOICES; i++) {
        voices[i].active = false;
        voices[i].isPad = false;
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
            voices[i].freq = freq;
            voices[i].phase = 0.0f;
            voices[i].step = freq / (float)MENU_SAMPLE_RATE;
            voices[i].volume = vol * 0.12f;
            voices[i].targetVol = vol * 0.12f;
            voices[i].currentVol = 0.0f;
            voices[i].attackRate = voices[i].volume / ((float)MENU_SAMPLE_RATE * 0.5f); // 0.5s attack
            voices[i].releaseRate = 0.99997f;
            voices[i].samplesLeft = (uint32_t)(durationSec * (float)MENU_SAMPLE_RATE);
            voices[i].delaySamples = (uint32_t)(delaySec * (float)MENU_SAMPLE_RATE);
            voices[i].lfoPhase = 0.0f;
            voices[i].lfoStep = (float)(rand() % 100) * 0.00001f + 0.00005f;
            voices[i].lfoDepth = 0.03f;
            return;
        }
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

    // --- Intro ---
    if (!introComplete) {
        introProgress += (float)numSamples / ((float)MENU_SAMPLE_RATE * 6.0f); // 6s intro
        if (introProgress >= 1.0f) {
            introProgress = 1.0f;
            introComplete = true;
        }

        // Arpeggio que sube y baja durante la intro
        // Notas del arpeggio: C4, Eb4, G4, Bb4, D5, C5, Bb4, G4, Eb4, C4 (sube y baja)
        float arpNotes[] = {261.63f, 311.13f, 392.00f, 466.16f, 587.33f, 523.25f, 466.16f, 392.00f, 311.13f, 261.63f};
        int numArpNotes = 10;
        float arpPosition = introProgress * (float)numArpNotes;
        int noteIdx = (int)arpPosition;
        float noteFrac = arpPosition - (float)noteIdx;

        if (noteIdx < numArpNotes - 1) {
            // Crossfade entre notas adyacentes del arpeggio
            float vol1 = (1.0f - noteFrac) * 0.06f * (0.5f + 0.5f * introProgress);
            float vol2 = noteFrac * 0.06f * (0.5f + 0.5f * introProgress);
            if (vol1 > 0.001f)
                playAmbientNote(arpNotes[noteIdx], 0.0f, 1.5f, vol1 / 0.12f);
            if (vol2 > 0.001f)
                playAmbientNote(arpNotes[noteIdx + 1], 0.0f, 1.5f, vol2 / 0.12f);
        }

        // El fade-in del pad se acelera durante la intro
        for (int i = 0; i < NUM_PAD_VOICES; i++) {
            voices[i].attackRate = voices[i].volume / ((float)MENU_SAMPLE_RATE * 2.0f);
        }
    }

    // --- Evolucion del acorde ---
    if (sampleCounter >= nextChordChange && introComplete) {
        changeChord();
        nextChordChange = sampleCounter + (uint64_t)((float)MENU_SAMPLE_RATE * 18.0f); // Cada 18s

        // Notas efimeras ocasionales (matices)
        // Pentatonica de Do menor: C, Eb, F, G, Bb
        float scale[] = {523.25f, 622.25f, 698.46f, 783.99f, 932.33f, 1046.50f};
        int numPings = 1 + (sampleCounter % 3);
        for (int i = 0; i < numPings; i++) {
            float delay = 2.0f + (float)(sampleCounter % 7000) / 2000.0f;
            float noteVol = 0.02f + (float)(sampleCounter % 200) / 10000.0f;
            playAmbientNote(scale[sampleCounter % 6], delay, 6.0f + (float)(sampleCounter % 4000) / 1000.0f, noteVol / 0.12f);
        }
    }

    // Crossfade de acordes
    if (chordCrossfade < 1.0f) {
        chordCrossfade += (float)numSamples / ((float)MENU_SAMPLE_RATE * 4.0f); // 4s crossfade
        if (chordCrossfade > 1.0f) chordCrossfade = 1.0f;
    }

    // --- LFO maestro del pad (respiración global) ---
    padLfoStep = 0.000077f; // ~0.077 Hz, ciclo de ~13 segundos
    float masterLfo = lookupSine(padLfoPhase);
    padLfoPhase += padLfoStep;
    if (padLfoPhase > 1.0f) padLfoPhase -= 1.0f;

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

            float currentStep = (v.freq * (1.0f + lfoMod)) / (float)MENU_SAMPLE_RATE;
            float sample = lookupSine(v.phase);

            // Attack suave
            if (v.currentVol < v.targetVol) {
                v.currentVol += v.attackRate;
                if (v.currentVol > v.targetVol) v.currentVol = v.targetVol;
            }

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
