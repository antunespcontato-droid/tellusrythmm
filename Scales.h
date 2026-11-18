#pragma once
#include <cstdint>
#include "Types.h"
namespace ps {

struct ScaleDef { const char* name; int n; int iv[12]; };

// Conjunto próprio de escalas de uso comum (NÃO é cópia da lista do original).
inline const ScaleDef* scaleTable(int& count) {
    static const ScaleDef t[] = {
        {"Chromatic", 12, {0,1,2,3,4,5,6,7,8,9,10,11}},
        {"Major", 7, {0,2,4,5,7,9,11}},
        {"Minor", 7, {0,2,3,5,7,8,10}},
        {"Dorian", 7, {0,2,3,5,7,9,10}},
        {"Phrygian", 7, {0,1,3,5,7,8,10}},
        {"Lydian", 7, {0,2,4,6,7,9,11}},
        {"Mixolydian", 7, {0,2,4,5,7,9,10}},
        {"Locrian", 7, {0,1,3,5,6,8,10}},
        {"Harmonic Minor", 7, {0,2,3,5,7,8,11}},
        {"Melodic Minor", 7, {0,2,3,5,7,9,11}},
        {"Phrygian Dominant", 7, {0,1,4,5,7,8,10}},
        {"Double Harmonic", 7, {0,1,4,5,7,8,11}},
        {"Hungarian Minor", 7, {0,2,3,6,7,8,11}},
        {"Neapolitan Minor", 7, {0,1,3,5,7,8,11}},
        {"Neapolitan Major", 7, {0,1,3,5,7,9,11}},
        {"Lydian Dominant", 7, {0,2,4,6,7,9,10}},
        {"Altered", 7, {0,1,3,4,6,8,10}},
        {"Persian", 7, {0,1,4,5,6,8,11}},
        {"Enigmatic", 7, {0,1,4,6,8,10,11}},
        {"Whole Tone", 6, {0,2,4,6,8,10}},
        {"Diminished W-H", 8, {0,2,3,5,6,8,9,11}},
        {"Diminished H-W", 8, {0,1,3,4,6,7,9,10}},
        {"Major Pentatonic", 5, {0,2,4,7,9}},
        {"Minor Pentatonic", 5, {0,3,5,7,10}},
        {"Blues", 6, {0,3,5,6,7,10}},
        {"Hirajoshi", 5, {0,2,3,7,8}},
        {"In Sen", 5, {0,1,5,7,10}},
        {"Iwato", 5, {0,1,5,6,10}},
        {"Kumoi", 5, {0,2,3,7,9}},
        {"Pelog (12-TET approx)", 5, {0,1,3,7,8}},
        {"Egyptian", 5, {0,2,5,7,10}},
        {"Prometheus", 6, {0,2,4,6,9,10}},
    };
    count = (int)(sizeof(t) / sizeof(t[0]));
    return t;
}
inline int numScales() { int c; scaleTable(c); return c; }       // escalas fixas
inline int scaleChoiceCount() { return numScales() + 1; }        // +1 = "Custom"

// Máscara de 12 bits (bit i = intervalo i a partir da tônica).
inline uint16_t scaleMask(int index, uint16_t customMask) {
    int c; const ScaleDef* t = scaleTable(c);
    if (index >= c) return customMask ? customMask : 0x0001;
    uint16_t m = 0; for (int i = 0; i < t[index].n; ++i) m |= (uint16_t)(1u << t[index].iv[i]);
    return m;
}

struct ScaleCtx {
    int root = 0; int count = 0; int pcs[12] = {0};
    void set(int rootPc, uint16_t mask) {
        root = ((rootPc % 12) + 12) % 12; count = 0;
        for (int i = 0; i < 12; ++i) if (mask & (1u << i)) pcs[count++] = i;
        if (count == 0) { pcs[0] = 0; count = 1; }
    }
    static int floorDiv(int a, int b) { int q = a / b; if ((a % b != 0) && ((a < 0) != (b < 0))) --q; return q; }
    // índice absoluto de grau (oitava*count + i) da nota de escala mais próxima (empate -> abaixo)
    int degreeIndex(int note) const {
        int rel = note - root; int oct = floorDiv(rel, 12); int pc = rel - oct * 12;
        int best = 0, bestD = 99;
        for (int i = 0; i < count; ++i) { int d = std::abs(pc - pcs[i]); if (d < bestD) { bestD = d; best = i; } }
        // considera também o primeiro grau da oitava seguinte
        if (std::abs(pc - (pcs[0] + 12)) < bestD) return (oct + 1) * count;
        return oct * count + best;
    }
    int noteFromDegree(int idx) const {
        int oct = floorDiv(idx, count); int j = idx - oct * count;
        return root + oct * 12 + pcs[j];
    }
    int quantize(int note) const { return noteFromDegree(degreeIndex(note)); }
    int stepDegrees(int note, int deg) const { return noteFromDegree(degreeIndex(note) + deg); }
};

} // namespace ps
