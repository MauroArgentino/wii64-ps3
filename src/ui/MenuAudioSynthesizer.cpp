#include "MenuAudioSynthesizer.h" // Renombrado
#include <stdlib.h>
#include <string.h>

MenuAudioSynthesizer g_menuAudioSynthesizer; // Renombrado

MenuAudioSynthesizer::MenuAudioSynthesizer() : sampleCounter(0), nextEvolutionSamples(0), isPlaying(false), introFinished(false) { // Renombrado
    for(int i=0; i<MAX_VOICES; i++) voices[i].active = false;

    // Pre-calcular tabla de senos para ahorro de CPU
    for(int i=0; i<SINE_TABLE_SIZE; i++)
        sineTable[i] = sinf(((float)i / (float)SINE_TABLE_SIZE) * 6.283185f);
}

MenuAudioSynthesizer::~MenuAudioSynthesizer() {} // Renombrado

void MenuAudioSynthesizer::init() { // Renombrado
    isPlaying = true;
    startIntro();
}

void MenuAudioSynthesizer::playNote(float freq, float delaySec, float durationSec, float vol, WaveType type) { // Renombrado
    for(int i=0; i<MAX_VOICES; i++) {
        if(!voices[i].active) {
            voices[i].type = type;
            voices[i].freq = freq;
            voices[i].phase = 0.0f;
            voices[i].step = freq / WII_SAMPLE_RATE;
            voices[i].volume = vol * 0.15f; // Bajamos el volumen de cada voz (evita saturación)
            voices[i].currentVol = 0.0f;
            voices[i].delaySamples = (uint32_t)(delaySec * WII_SAMPLE_RATE);
            voices[i].samplesLeft = (uint32_t)(durationSec * WII_SAMPLE_RATE);
            
            // Envolvente más lenta (evita el sonido metálico/chicharra)
            voices[i].attackStep = voices[i].volume / (WII_SAMPLE_RATE * 0.2f); 
            voices[i].decayRate = 0.99998f; // Decay mucho más suave
            
            voices[i].active = true;
            break;
        }
    }
}

void MenuAudioSynthesizer::startIntro() { // Renombrado
    // El acorde Swell Cmaj9 que te gustó
    float swellFreqs[] = {261.63f, 329.63f, 392.00f, 493.88f, 587.33f};
    for(int i=0; i<5; i++) playNote(swellFreqs[i], 0.0f, 4.0f, 0.08f, WAVE_SINE);

    // Plinks cristalinos con delay
    playNote(1046.50f, 0.5f, 0.8f, 0.04f, WAVE_SINE);
    playNote(1318.51f, 0.8f, 0.8f, 0.04f, WAVE_SINE);
    playNote(1567.98f, 1.2f, 0.8f, 0.04f, WAVE_SINE);
    
    nextEvolutionSamples = WII_SAMPLE_RATE * 5; // Empezar ambiente en 5 segundos
}

void MenuAudioSynthesizer::generateEvolution() { // Renombrado
    const float chords[3][4] = {
        {174.61f, 220.00f, 261.63f, 329.63f}, // Fmaj7
        {261.63f, 329.63f, 392.00f, 493.88f}, // Cmaj7
        {196.00f, 246.94f, 293.66f, 392.00f}  // G6
    };
    
    int r = rand() % 3;
    for(int i=0; i<4; i++) playNote(chords[r][i], 0.0f, 12.0f, 0.02f, WAVE_TRIANGLE);

    // Pings melódicos aleatorios (Escala Pentatónica)
    float scale[] = {523.25f, 659.25f, 783.99f, 880.00f, 1046.50f};
    for(int i=0; i<2; i++) {
        float randomDelay = 1.0f + (float)(rand() % 800) / 100.0f;
        playNote(scale[rand() % 5], randomDelay, 4.0f, 0.025f, WAVE_SINE);
    }
}

float MenuAudioSynthesizer::lookupSine(float phase) { // Renombrado
    // Acceso ultra-rápido a tabla
    int index = (int)(phase * (SINE_TABLE_SIZE - 1));
    return sineTable[index];
}

float MenuAudioSynthesizer::calculateTriangle(float phase) { // Renombrado
    return 4.0f * fabsf(phase - 0.5f) - 1.0f;
}

void MenuAudioSynthesizer::process(float* buffer, uint32_t numSamples) { // Renombrado
    if(!isPlaying) return;

    // Lógica de tiempo fuera del bucle de mezcla
    if(sampleCounter >= nextEvolutionSamples) {
        generateEvolution();
        nextEvolutionSamples = sampleCounter + (uint64_t)(10.0f * WII_SAMPLE_RATE);
    }

    // Limpiar buffer de salida (Stereo)
    memset(buffer, 0, numSamples * 2 * sizeof(float));

    for(int i=0; i<MAX_VOICES; i++) {
        AudioVoice& v = voices[i];
        if(!v.active) continue;

        uint32_t s = 0;
        // Saltar delay si existe
        if (v.delaySamples > numSamples) {
            v.delaySamples -= numSamples;
            continue;
        } else if (v.delaySamples > 0) {
            s = v.delaySamples;
            v.delaySamples = 0;
        }

        for(; s < numSamples; s++) {
            float sample = (v.type == WAVE_SINE) ? lookupSine(v.phase) : calculateTriangle(v.phase);
            
            if(v.currentVol < v.volume) v.currentVol += v.attackStep;
            v.currentVol *= v.decayRate;
            
            float finalOut = sample * v.currentVol * 0.25f; // Ganancia maestra muy conservadora
            buffer[s*2] += finalOut;
            buffer[s*2 + 1] += finalOut;

            v.phase += v.step;
            if(v.phase > 1.0f) v.phase -= 1.0f;

            if(v.samplesLeft > 0) v.samplesLeft--;
            else v.active = false;

            if (v.currentVol < 0.00001f && v.samplesLeft < 1000) { v.active = false; break; }
        }
    }

    // Incrementar contador global al final del bloque
    sampleCounter += numSamples;
}

void MenuAudioSynthesizer::updateLogic() { // Renombrado
    // Esta función se puede usar para disparar eventos visuales sincronizados
    // con la música si fuera necesario.
}