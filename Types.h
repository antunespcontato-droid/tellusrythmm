#pragma once
// Tipos básicos do motor (C++ puro, sem JUCE, sem alocação em tempo real).
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace ps {

constexpr int kNumLanes = 8;
constexpr int kMaxSteps = 32;

// Nota completa (início em PPQ + duração em PPQ). O agendador gera note-on/off a partir dela.
struct Note {
    double t = 0.0;      // início, em quarter-notes (PPQ)
    double len = 0.1;    // duração, em PPQ
    uint8_t note = 60;
    uint8_t vel = 100;
    uint8_t ch = 0;      // canal MIDI 0..15
    int8_t lane = -1;    // lane PERC (0..7) ou -1 (gerador principal)
    float pan = 0.f;     // -1..1 (áudio interno do PERC)
    float pitchSemi = 0.f; // ajuste fino em semitons (áudio interno do PERC)
    int16_t pb = 0;      // pitch bend (-8192..8191), 0 = nenhum
    uint8_t nCC = 0;
    uint8_t ccNum[4] = {0, 0, 0, 0};
    uint8_t ccVal[4] = {0, 0, 0, 0};
    bool onDone = false; // controle interno do agendador
};

template <int N>
struct NoteList {
    Note n[N];
    int count = 0;
    bool push(const Note& x) { if (count >= N) return false; n[count++] = x; return true; }
    void clear() { count = 0; }
};

enum class EvType : uint8_t { NoteOn, NoteOff, CC, PitchBend };

// Evento de saída com offset em amostras dentro do bloco.
struct OutEvent {
    int sample = 0;
    EvType type = EvType::NoteOn;
    uint8_t ch = 0;
    uint8_t d1 = 0;      // nota / cc / (pitchbend LSB)
    uint8_t d2 = 0;      // velocity / valor / (pitchbend MSB)
    int8_t lane = -1;
    float pan = 0.f;
    float pitchSemi = 0.f;
    float lenSec = 0.f;  // duração (só note-on) – usado pelo áudio interno
};

struct OutBuffer {
    static constexpr int kCap = 4096;
    OutEvent e[kCap];
    int count = 0;
    void clear() { count = 0; }
    bool push(const OutEvent& x) { if (count >= kCap) return false; e[count++] = x; return true; }
};

struct BlockInfo {
    double ppqStart = 0.0;
    double bpm = 120.0;
    double sampleRate = 44100.0;
    int numSamples = 0;
    bool playing = false;
    int beatsPerBar = 4;
};

inline int posMod(long a, int m) { long r = a % m; return (int)(r < 0 ? r + m : r); }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

} // namespace ps
