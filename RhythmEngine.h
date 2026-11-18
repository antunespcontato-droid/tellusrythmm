#pragma once
// Motor rítmico/melódico principal (implementação própria).
#include <cstring>
#include "Types.h"
#include "Rng.h"
#include "Scales.h"
#include "Params.h"
#include "Pattern.h"
#include "StepClock.h"

namespace ps {

constexpr int kMaxNotes = 512;
using NoteBuf = NoteList<kMaxNotes>;

class RhythmEngine {
public:
    Rng rng{12345};

    void reset() {
        resetPending = true; polyIdx = 0; polyDirSign = 1; branchCtr = -1; cascPos = 0;
        thumbT = false; pinkyT = false; bothFlip = false; drunkIdx = -1; skipUntil = -1000000000L;
    }
    void requestSeqReset() { resetPending = true; }

    // ---- notas seguradas (chamado no início do bloco) ----
    void noteOn(int n, bool latch) {
        if (latch && physDown == 0) nHeld = 0;
        ++physDown;
        for (int i = 0; i < nHeld; ++i) if (held[i] == n) { lastNote = n; return; }
        if (nHeld < 16) {
            int i = nHeld++; while (i > 0 && held[i - 1] > n) { held[i] = held[i - 1]; --i; } held[i] = n; // mantém ordenado
        }
        lastNote = n;
    }
    void noteOff(int n, bool latch) {
        if (physDown > 0) --physDown;
        if (latch) return;
        for (int i = 0; i < nHeld; ++i) if (held[i] == n) { for (int j = i; j < nHeld - 1; ++j) held[j] = held[j + 1]; --nHeld; break; }
        if (nHeld > 0 && lastNote == n) lastNote = held[nHeld - 1];
    }
    void clearHeld() { nHeld = 0; physDown = 0; }
    int heldCount() const { return nHeld; }
    int loopStateValue() const { return loopState; }
    long lastStepIndex() const { return lastSeqIdx; }

    void generate(double start, double end, const BlockInfo& bi, const float* P, const Pattern& pat, NoteBuf& out) {
        sc.set((int)P[pRoot], scaleMask((int)P[pScale], (uint16_t)P[pCustomMask]));
        bool loopOn = P[pLoopOn] > 0.5f;
        if (!loopOn) loopState = 0;
        else if (loopState == 0) {
            static const double kLoopBeats[8] = {0.125, 0.25, 0.5, 1, 2, 0, 0, 0};
            int li = clampi((int)P[pLoopLen], 0, 7);
            loopLen = li < 5 ? kLoopBeats[li] : (double)bi.beatsPerBar * (li == 5 ? 1 : li == 6 ? 2 : 4);
            double grid = std::min(loopLen, 1.0);
            recStart = std::ceil(start / grid - 1e-9) * grid;
            recEnd = recStart + loopLen; nLoop = 0; loopState = 1;
        }
        if (loopState == 1 && start >= recEnd - 1e-9) { loopState = 2; playStart = recEnd; }

        if (loopState == 2) { playLoop(start, end, out); return; }
        if (loopState == 1) {
            double b = std::min(end, recEnd);
            gen(start, b, bi, P, pat, out, true);
            if (end > recEnd - 1e-9 && end > recEnd) { loopState = 2; playStart = recEnd; playLoop(recEnd, end, out); }
            return;
        }
        gen(start, end, bi, P, pat, out, false);
    }

private:
    ScaleCtx sc;
    int held[16] = {0}; int nHeld = 0; int physDown = 0; int lastNote = 60;
    // estado de contorno
    int polyIdx = 0, polyDirSign = 1, branchCtr = -1, cascPos = 0, drunkIdx = -1;
    bool thumbT = false, pinkyT = false, bothFlip = false;
    long skipUntil = -1000000000L;
    bool resetPending = true; long seqAnchor = 0; long lastSeqIdx = 0;
    // looper
    int loopState = 0; double loopLen = 1, recStart = 0, recEnd = 0, playStart = 0;
    Note loopBuf[256]; int nLoop = 0;

    void playLoop(double a, double b, NoteBuf& out) {
        for (int i = 0; i < nLoop; ++i) {
            double rel = loopBuf[i].t;
            double k = std::ceil((a - playStart - rel) / loopLen - 1e-12);
            double t = playStart + k * loopLen + rel;
            while (t < b) {
                if (t >= a) { Note n = loopBuf[i]; n.t = t; n.onDone = false; out.push(n); }
                t += loopLen;
            }
        }
    }

    int dirSign(int dir) {
        switch (dir) { case 0: return 1; case 1: return -1;
            case 2: bothFlip = !bothFlip; return bothFlip ? 1 : -1;
            default: return rng.chance(0.5f) ? 1 : -1; }
    }
    int shapeDegree(int shape, int steps, int dir) {
        int sign = dirSign(dir), d = 0;
        switch (shape) {
            case 0: d = rng.range(1, steps); break;
            case 1: { branchCtr = (branchCtr + 1) % (2 * steps); d = (branchCtr % 2 == 0) ? 0 : (branchCtr + 1) / 2; break; }
            case 2: cascPos = cascPos % steps + 1; d = cascPos; break;
            case 3: thumbT = !thumbT; d = thumbT ? 0 : rng.range(1, steps); break;
            default: pinkyT = !pinkyT; d = pinkyT ? steps : rng.range(0, steps - 1); break;
        }
        return d * sign;
    }
    int octaveShift(const float* P) {
        int R = (int)P[pOctRange]; if (R <= 0 || !rng.chance(P[pOctChance])) return 0;
        int dir = (int)P[pDirection];
        if (dir == 0) return rng.range(1, R);
        if (dir == 1) return -rng.range(1, R);
        return rng.range(-R, R);
    }
    int deviate(int base, const float* P) {
        int d = shapeDegree((int)P[pShape], (int)P[pSteps], (int)P[pDirection]);
        int n = d != 0 ? sc.stepDegrees(base, d) : base;
        return n;
    }
    int finalize(int n, const float* P) {
        n += 12 * octaveShift(P);
        if (P[pKeyFreeze] > 0.5f) n = sc.quantize(n);
        return clampi(n, 0, 127);
    }
    int pickPool(int dir) {
        int n = nHeld; if (n <= 1) { polyIdx = 0; return held[0]; }
        switch (dir) {
            case 0: polyIdx = (polyIdx + 1) % n; break;
            case 1: polyIdx = (polyIdx - 1 + n) % n; break;
            case 2: { int nx = polyIdx + polyDirSign; if (nx >= n || nx < 0) { polyDirSign = -polyDirSign; nx = polyIdx + polyDirSign; } polyIdx = clampi(nx, 0, n - 1); break; }
            default: polyIdx = rng.range(0, n - 1); break;
        }
        return held[polyIdx];
    }
    int buildChord(int* outN, const float* P) {
        int nV = clampi((int)P[pVoices], 1, 6), c = 0;
        for (int i = 0; i < nV; ++i) {
            int n = i < nHeld ? held[i] : sc.stepDegrees(outN[c - 1], 2);
            if (rng.chance(P[pFlux] * 0.6f)) n = sc.stepDegrees(n, rng.chance(0.5f) ? 1 : -1);
            if (i > 0 && rng.chance(P[pSpread])) n += 12;
            outN[c++] = clampi(n, 0, 127);
        }
        return c;
    }
    long seqIndex(long g, const float* P) {
        int len = clampi((int)P[pSeqLen], 1, kMaxSteps);
        int e = clampi((int)P[pSeqEnd] - 1, 0, len - 1);
        int s = clampi((int)P[pSeqStart] - 1, 0, len - 1);
        if (s > e) std::swap(s, e);
        int span = e - s + 1; int dir = (int)P[pSeqDir];
        long rel = g - seqAnchor; int idx;
        switch (dir) {
            case 1: idx = e - posMod(rel, span); break;
            case 2: { if (span == 1) idx = s; else { int per = 2 * span - 2; int k = posMod(rel, per); idx = s + (k < span ? k : per - k); } break; }
            case 3: idx = rng.range(s, e); break;
            case 4: { if (drunkIdx < s || drunkIdx > e) drunkIdx = s; else drunkIdx = clampi(drunkIdx + rng.range(-1, 1), s, e); idx = drunkIdx; break; }
            default: idx = s + posMod(rel, span); break;
        }
        return idx;
    }

    void emit(const Note& n, NoteBuf& out, bool capture) {
        out.push(n);
        if (capture && nLoop < 256 && n.t >= recStart - 1e-9 && n.t < recEnd) { loopBuf[nLoop] = n; loopBuf[nLoop].t = n.t - recStart; ++nLoop; }
    }

    void gen(double a, double b, const BlockInfo& bi, const float* P, const Pattern& pat, NoteBuf& out, bool capture) {
        if (b <= a) return;
        double spb = kResSpb[clampi((int)P[pRes], 0, 5)];
        if (P[pTriplet] > 0.5f && P[pRes] < 4) spb *= 1.5;
        static const double kSpeed[3] = {0.5, 1.0, 2.0};
        spb *= kSpeed[clampi((int)P[pSpeed], 0, 2)];
        double stepLen = 1.0 / spb;
        bool seqMode = P[pSource] > 0.5f;
        bool poly = P[pMode] > 0.5f;
        int ch = clampi((int)P[pOutChannel] - 1, 0, 15);
        int s8 = std::max(1, (int)(spb / 2)), s4 = std::max(1, (int)spb), s2 = std::max(1, (int)(spb * 2));
        long stepsPerBar = (long)(spb * bi.beatsPerBar);

        forEachStep(a, b, spb, P[pSwing], [&](long g, double t) {
            if (resetPending) { seqAnchor = g; resetPending = false; }
            if (nHeld == 0) return;
            if (g < skipUntil) return;

            const Step* st = nullptr; bool grooveDriven = true; bool fire;
            if (seqMode) {
                long idx = seqIndex(g, P); lastSeqIdx = idx; st = &pat.s[idx];
                if (st->mode == 0) return;
                if (st->mode == 1) { grooveDriven = false; fire = rng.chance(st->prob); }
                else { st = &pat.s[idx]; fire = rng.chance(P[pDensity]); }
            } else fire = rng.chance(P[pDensity]);
            if (!fire) return;

            double stretch = 1.0;
            if (grooveDriven) {
                if (s2 > 1 && posMod(g, s2) == 0 && rng.chance(P[pGroove2])) stretch = s2;
                else if (s4 > 1 && posMod(g, s4) == 0 && rng.chance(P[pGroove4])) stretch = s4;
                else if (s8 > 1 && posMod(g, s8) == 0 && rng.chance(P[pGroove8])) stretch = s8;
                if (stretch > 1.0) skipUntil = g + (long)stretch;
            }

            // --- dinâmica ---
            float v = (st && st->mode == 1) ? (float)st->vel : P[pVelocity];
            bool accent = st ? st->accent != 0 : (posMod(g, std::max(1, s4)) == 0);
            if (accent) v += (127.f - v) * P[pAccentAmt] * (st ? 1.f : 0.5f);
            v += rng.bi() * P[pVelRand] * 50.f + rng.bi() * P[pHuman] * 8.f;
            v *= P[pIntensity];
            int vel = clampi((int)std::lround(v), 1, 127);

            double len = (double)P[pGate] * stepLen * stretch * (st ? st->gate : 1.f);
            len *= 1.0 + (double)rng.bi() * P[pGateRand] * 0.8;
            len = std::max(len, 0.01);

            double tt = t + (double)rng.uni() * P[pHuman] * 0.5 * stepLen;   // atraso humanizado (nunca antecipa)
            if (tt < a) tt = a;

            // --- altura ---
            int notes[8]; int nn = 0;
            int stepPitch = st ? st->pitch : 0;
            if (poly) {
                if (rng.chance(P[pChordBlend])) {
                    int tmp[8]; int c = buildChord(tmp, P);
                    for (int i = 0; i < c && nn < 8; ++i) notes[nn++] = clampi(tmp[i] + stepPitch, 0, 127);
                } else {
                    int base = rng.chance(P[pChance]) ? pickPool((int)P[pDirection]) : held[0];
                    if (rng.chance(P[pExtend])) base = deviate(base, P);
                    notes[nn++] = finalize(base + stepPitch, P);
                }
            } else {
                int base = lastNote + stepPitch;
                if (rng.chance(P[pMelodyProb])) base = deviate(base, P);
                notes[nn++] = finalize(base, P);
            }
            if (rng.chance(P[pStackProb]) && nn < 6) {
                int top = notes[0];
                notes[nn++] = clampi(sc.stepDegrees(top, 2), 0, 127);
                if (rng.chance(0.5f) && nn < 8) notes[nn++] = clampi(sc.stepDegrees(top, 4), 0, 127);
            }

            // --- repetições (ratchet/glitch) ---
            int rep = 1; float gl = P[pGlitch];
            if (st && st->ratchet > 1) rep = st->ratchet;
            else if (gl > 0.f && rng.chance(gl * 0.8f)) rep = rng.range(2, 4);
            double sub = stepLen / rep;

            int16_t pb = 0;
            if (P[pPbRand] > 0.f && rng.chance(P[pPbRand])) pb = (int16_t)(rng.bi() * P[pPbRand] * 8191.f);

            for (int r = 0; r < rep; ++r) {
                double tr = tt + r * sub;
                if (gl > 0.66f && r > 0) tr += (double)rng.bi() * 0.1 * sub;
                if (tr < a) tr = a;
                double lr = rep > 1 ? std::min(len, sub * 0.9) : len;
                for (int i = 0; i < nn; ++i) {
                    Note n; n.t = tr; n.len = lr; n.note = (uint8_t)notes[i]; n.ch = (uint8_t)ch;
                    n.vel = (uint8_t)clampi(vel - (r > 0 ? r * 4 : 0), 1, 127); n.pb = pb;
                    if (i == 0) addCCs(n, P, lr / stepLen, g, stepsPerBar);
                    emit(n, out, capture);
                }
            }
        });
    }

    void addCCs(Note& n, const float* P, double lenRatio, long g, long stepsPerBar) {
        static const int numIdx[4] = {pCc1Num, pCc2Num, pCc3Num, pCc4Num};
        static const int srcIdx[4] = {pCc1Src, pCc2Src, pCc3Src, pCc4Src};
        static const int amtIdx[4] = {pCc1Amt, pCc2Amt, pCc3Amt, pCc4Amt};
        for (int s = 0; s < 4; ++s) {
            int cc = (int)P[numIdx[s]]; if (cc <= 0 || n.nCC >= 4) continue;
            float val = 0;
            switch ((int)P[srcIdx[s]]) {
                case 0: val = rng.uni() * 127.f; break;
                case 1: val = (float)n.vel; break;
                case 2: val = (float)n.note; break;
                case 3: val = clampf((float)lenRatio * 63.f, 0.f, 127.f); break;
                default: val = stepsPerBar > 0 ? 127.f * (float)posMod(g, (int)stepsPerBar) / (float)stepsPerBar : 0.f; break;
            }
            n.ccNum[n.nCC] = (uint8_t)clampi(cc, 1, 127);
            n.ccVal[n.nCC] = (uint8_t)clampi((int)std::lround(val * P[amtIdx[s]]), 0, 127);
            ++n.nCC;
        }
    }
};

} // namespace ps
