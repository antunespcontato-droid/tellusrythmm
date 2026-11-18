#pragma once
// Agrega os módulos (Rhythm + PERC + Beat Repeat), agenda notas em PPQ e converte em eventos com offset de amostra.
#include "Types.h"
#include "Params.h"
#include "Pattern.h"
#include "RhythmEngine.h"
#include "PercEngine.h"
#include "BeatRepeat.h"

namespace ps {

class Engine {
public:
    PatternBank bank;               // cópia usada pelo thread de áudio
    RhythmEngine rhythm;
    PercEngine perc;
    BeatRepeat brMain, brPerc;

    void prepare() { flushPending(); wasRunning = false; rhythm.reset(); brMain.reset(); brPerc.reset(); freePpq = 0; }
    void seed(uint64_t s) { rhythm.rng.reseed(s); perc.rng.reseed(s * 3 + 1); brMain.rng.reseed(s * 5 + 2); brPerc.rng.reseed(s * 7 + 3); }

    void noteOn(int n, const float* P) { rhythm.noteOn(n, P[pLatch] > 0.5f); }
    void noteOff(int n, const float* P) { rhythm.noteOff(n, P[pLatch] > 0.5f); }
    void allNotesOff() { rhythm.clearHeld(); }
    void requestSeqReset() { rhythm.requestSeqReset(); }

    void process(const BlockInfo& biIn, const float* P, OutBuffer& out) {
        out.clear();
        BlockInfo bi = biIn;
        if (bi.bpm <= 1.0) bi.bpm = 120.0;
        if (bi.beatsPerBar < 1) bi.beatsPerBar = 4;
        const double pps = bi.bpm / 60.0 / bi.sampleRate;          // PPQ por amostra
        const bool freeRun = P[pFreeRun] > 0.5f;
        const bool run = bi.playing || freeRun;
        double a = bi.playing ? bi.ppqStart : freePpq;
        double b = a + bi.numSamples * pps;

        if (!run) { if (wasRunning) flush(out, 0); wasRunning = false; freePpq = 0; return; }
        if (!bi.playing) { freePpq = b; } else freePpq = b;

        if (!wasRunning || std::fabs(a - expectedStart) > 0.01) {
            flush(out, 0);
            if (P[pSeqResetOnPlay] > 0.5f || !wasRunning) rhythm.requestSeqReset();
            brMain.reset(); brPerc.reset();
        }
        wasRunning = true; expectedStart = b;

        mainNotes.clear(); percNotes.clear();
        if (P[pMainOn] > 0.5f) rhythm.generate(a, b, bi, P, bank.main, mainNotes);
        if (P[pPercOn] > 0.5f) perc.generate(a, b, bi, P, bank, percNotes);
        brMain.process(a, b, P, bi.beatsPerBar, P[pBrOn] > 0.5f && P[pBrMain] > 0.5f, mainNotes);
        brPerc.process(a, b, P, bi.beatsPerBar, P[pBrOn] > 0.5f && P[pBrPerc] > 0.5f, percNotes);
        for (int i = 0; i < mainNotes.count; ++i) addPending(mainNotes.n[i]);
        for (int i = 0; i < percNotes.count; ++i) addPending(percNotes.n[i]);
        emitDue(a, b, pps, bi, out);
        sortOut(out);
    }

    int pendingCount() const { return nPend; }

private:
    static constexpr int kPend = 1024;
    Note pend[kPend]; int nPend = 0;
    NoteBuf mainNotes, percNotes;
    bool wasRunning = false; double expectedStart = 0, freePpq = 0;

    void addPending(const Note& n) { if (nPend < kPend) { pend[nPend] = n; pend[nPend].onDone = false; ++nPend; } }
    void flushPending() { nPend = 0; }

    static void pushOff(OutBuffer& out, const Note& n, int s, double bpm) {
        OutEvent e; e.sample = s; e.type = EvType::NoteOff; e.ch = n.ch; e.d1 = n.note; e.d2 = 0; e.lane = n.lane; (void)bpm;
        out.push(e);
        if (n.pb != 0) { OutEvent p; p.sample = s; p.type = EvType::PitchBend; p.ch = n.ch; p.d1 = 0; p.d2 = 64; out.push(p); }
    }
    void flush(OutBuffer& out, int sample) {
        for (int i = 0; i < nPend; ++i) if (pend[i].onDone) pushOff(out, pend[i], sample, 120.0);
        nPend = 0;
    }

    void emitDue(double a, double b, double pps, const BlockInfo& bi, OutBuffer& out) {
        const int ns = bi.numSamples;
        for (int i = 0; i < nPend; ++i) {
            Note& n = pend[i];
            int sOn = -1;
            if (!n.onDone && n.t < b) {
                sOn = clampi((int)std::lround((std::max(n.t, a) - a) / pps), 0, ns - 1);
                // mesma nota ainda soando? desliga antes de retocar
                for (int j = 0; j < nPend; ++j) if (j != i && pend[j].len > -1e8 && pend[j].onDone && pend[j].note == n.note && pend[j].ch == n.ch && pend[j].lane == n.lane) {
                    pushOff(out, pend[j], sOn, bi.bpm); pend[j].len = -1e9; pend[j].t = -1e9; // marca para remoção
                }
                if (n.pb != 0) { int v = clampi((int)n.pb + 8192, 0, 16383); OutEvent p; p.sample = sOn; p.type = EvType::PitchBend; p.ch = n.ch; p.d1 = (uint8_t)(v & 127); p.d2 = (uint8_t)(v >> 7); out.push(p); }
                for (int c = 0; c < n.nCC; ++c) { OutEvent e; e.sample = sOn; e.type = EvType::CC; e.ch = n.ch; e.d1 = n.ccNum[c]; e.d2 = n.ccVal[c]; out.push(e); }
                OutEvent e; e.sample = sOn; e.type = EvType::NoteOn; e.ch = n.ch; e.d1 = n.note; e.d2 = n.vel; e.lane = n.lane; e.pan = n.pan; e.pitchSemi = n.pitchSemi;
                e.lenSec = (float)(n.len * 60.0 / bi.bpm);
                out.push(e); n.onDone = true;
            }
            if (n.onDone) {
                double offT = n.t + n.len;
                if (offT < b) {
                    int sOff = clampi((int)std::lround((offT - a) / pps), 0, ns - 1);
                    if (sOn >= 0 && sOff <= sOn) sOff = sOn + 1;
                    if (sOff <= ns - 1) { pushOff(out, n, sOff, bi.bpm); n.len = -1e9; n.t = -1e9; }
                }
            }
        }
        int k = 0; for (int i = 0; i < nPend; ++i) if (pend[i].len > -1e8) pend[k++] = pend[i]; nPend = k;
    }

    static int prio(EvType t) { return t == EvType::NoteOff ? 0 : (t == EvType::NoteOn ? 2 : 1); }
    static void sortOut(OutBuffer& o) {
        for (int i = 1; i < o.count; ++i) {
            OutEvent x = o.e[i]; long key = (long)x.sample * 4 + prio(x.type); int j = i - 1;
            while (j >= 0 && (long)o.e[j].sample * 4 + prio(o.e[j].type) > key) { o.e[j + 1] = o.e[j]; --j; }
            o.e[j + 1] = x;
        }
    }
};

} // namespace ps
