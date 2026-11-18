#pragma once
// MÓDULO ADICIONAL (implementação própria – NÃO é um recurso confirmado do Rhythmizer 5):
// sequenciador percussivo de 8 lanes, polirrítmico, voltado a Psytrance / Progressive Psytrance.
#include "Types.h"
#include "Rng.h"
#include "Params.h"
#include "Pattern.h"
#include "StepClock.h"
#include "RhythmEngine.h"

namespace ps {

constexpr int kLaneNote[kNumLanes] = {36, 39, 42, 46, 70, 37, 45, 56};
inline const char* laneName(int l) {
    static const char* n[kNumLanes] = {"KICK", "CLAP", "C-HAT", "O-HAT", "SHAKER", "RIM", "TOM", "PERC"};
    return n[l];
}

class PercEngine {
public:
    Rng rng{999};
    long lastStep[kNumLanes] = {0};

    void generate(double a, double b, const BlockInfo& bi, const float* P, const PatternBank& bank, NoteBuf& out) {
        if (b <= a) return;
        int ch = clampi((int)P[pPercCh] - 1, 0, 15);
        for (int l = 0; l < kNumLanes; ++l) {
            if (P[laneParam(l, lpMute)] > 0.5f) continue;
            const Pattern& pat = bank.lane[l];
            int len = clampi((int)P[laneParam(l, lpLen)], 1, kMaxSteps);
            double spb = kResSpb[clampi((int)P[laneParam(l, lpRes)], 0, 5)];
            double stepLen = 1.0 / spb;
            float lProb = P[laneParam(l, lpProb)], level = P[laneParam(l, lpLevel)];
            forEachStep(a, b, spb, P[pPercSwing], [&](long g, double t) {
                int idx = posMod(g, len); lastStep[l] = idx;
                const Step& st = pat.s[idx];
                bool fill = fillActive(t, P, bi.beatsPerBar);
                bool play = false; float velMul = 1.f;
                if (st.mode == 1) { play = rng.chance(st.prob * lProb) && !rng.chance(P[pPercRandom] * 0.08f); }
                else if (st.mode == 2) { play = fill && rng.chance(st.prob * lProb); velMul = 0.8f; }
                else {
                    if (fill && l != 0 && rng.chance(P[pPercVar] * 0.5f * lProb)) { play = true; velMul = 0.55f; }
                    else if (rng.chance(P[pPercRandom] * 0.1f * lProb)) { play = true; velMul = 0.45f; }
                }
                if (!play) return;
                float v = (float)st.vel * velMul * level * P[pPercInt] + rng.bi() * P[pPercRandom] * 25.f;
                float semi = P[laneParam(l, lpPitch)] + (float)st.pitch + P[pPercPitch];
                int rep = st.ratchet;
                if (fill && rng.chance(P[pPercVar])) rep = std::min(4, rep * 2);
                double sub = stepLen / rep;
                for (int r = 0; r < rep; ++r) {
                    Note n; n.t = t + r * sub; if (n.t < a) n.t = a;
                    n.len = std::max(0.01, (double)P[laneParam(l, lpGate)] * st.gate * std::min(stepLen, sub) * 0.9);
                    n.note = (uint8_t)clampi(kLaneNote[l] + (int)std::lround(semi), 0, 127);
                    n.vel = (uint8_t)clampi((int)std::lround(v * (r > 0 ? 0.85f : 1.f)), 1, 127);
                    n.ch = (uint8_t)ch; n.lane = (int8_t)l;
                    n.pan = clampf(st.pan + P[laneParam(l, lpPan)], -1.f, 1.f);
                    n.pitchSemi = semi;
                    out.push(n);
                }
            });
        }
    }

private:
    static bool fillActive(double t, const float* P, int bpb) {
        if (P[pPercVar] <= 0.f) return false;
        static const int ev[4] = {1, 2, 4, 8};
        int every = ev[clampi((int)P[pPercFill], 0, 3)];
        long bar = (long)std::floor(t / bpb);
        double inBar = t - (double)bar * bpb;
        return posMod(bar, every) == every - 1 && inBar >= bpb - 1.0;
    }
};

// ---- Geradores de pontos de partida (editáveis depois pelo usuário) ----
// Notação: 'X' acento, 'x' normal, 'o' fantasma, '.' vazio (16 passos, repetidos até 32).
inline void setPatternFromMask(Pattern& p, const char* m16, Rng& r, uint8_t baseVel = 100) {
    p.clear();
    for (int i = 0; i < kMaxSteps; ++i) {
        char c = m16[i % 16]; Step s;
        if (c == '.') { s.mode = 0; }
        else {
            s.mode = 1;
            s.vel = c == 'X' ? (uint8_t)std::min(127, baseVel + 22) : c == 'o' ? (uint8_t)(baseVel * 45 / 100) : baseVel;
            s.vel = (uint8_t)clampi((int)s.vel + r.range(-4, 4), 1, 127);
            s.accent = c == 'X';
        }
        p.s[i] = s;
    }
}

// variant: 0 Full-On, 1 Progressive, 2 Minimal Psy, 3 Rolling
inline void applyPsyGroove(PatternBank& bank, int variant, Rng& r) {
    static const char* kick = "X...X...X...X...";
    const char* clap = "....x.......x...";
    const char* chat[4] = {"xoxoxoxoxoxoxoxo", "x.x.x.x.x.x.x.x.", "o.o.o.o.o.o.o.o.", "xxoxxxoxxxoxxxox"};
    const char* ohat = "..X...X...X...X.";
    const char* shaker[4] = {"oxoxoxoxoxoxoxox", "xoxxxoxxxoxxxoxx", "................", "xxxxxxxxxxxxxxxx"};
    const char* rim[4] = {"....o..o....o..o", "...x......x.....", "................", "..o....o..o....o"};
    const char* tom[4] = {"............x..x", "................", "................", "...........o..xx"};
    const char* perc[4] = {"..o..x..o..x..o.", "......x.......x.", "................", "................"};
    variant = clampi(variant, 0, 3);
    setPatternFromMask(bank.lane[0], kick, r, 112);
    setPatternFromMask(bank.lane[1], variant == 2 ? "................" : clap, r, 92);
    setPatternFromMask(bank.lane[2], chat[variant], r, 78);
    setPatternFromMask(bank.lane[3], ohat, r, 92);
    setPatternFromMask(bank.lane[4], shaker[variant], r, 70);
    setPatternFromMask(bank.lane[5], rim[variant], r, 84);
    setPatternFromMask(bank.lane[6], tom[variant], r, 90);
    setPatternFromMask(bank.lane[7], perc[variant], r, 84);
    if (variant == 2 || variant == 3) {   // textura euclidiana
        bank.lane[7].euclid(16, variant == 2 ? 5 : 7, r.range(0, 15));
        for (auto& s : bank.lane[7].s) { s.vel = (uint8_t)(70 + r.range(0, 30)); s.pan = r.bi() * 0.6f; }
    }
    for (int l = 4; l < kNumLanes; ++l) for (auto& s : bank.lane[l].s) s.pan = (s.pan == 0.f) ? r.bi() * 0.4f : s.pan;
}

inline void randomizeLane(Pattern& p, int lane, int length, Rng& r) {
    static const int pulses[kNumLanes] = {4, 2, 8, 4, 12, 3, 3, 5};
    p.clear();
    int k = clampi(pulses[lane] + r.range(-1, 2), 1, length);
    p.euclid(length, k, r.range(0, length - 1));
    for (int i = 0; i < kMaxSteps; ++i) {
        Step& s = p.s[i]; s.vel = (uint8_t)(70 + r.range(0, 50)); s.prob = r.chance(0.8f) ? 1.f : 0.6f;
        s.pan = lane >= 4 ? r.bi() * 0.5f : 0.f; s.gate = 0.6f + r.uni() * 0.6f;
    }
}

} // namespace ps
