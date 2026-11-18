#pragma once
// MÓDULO ADICIONAL (implementação própria – NÃO confirmado como recurso separado do Rhythmizer 5;
// o mais próximo no original é o Looper/Glitch). Repete fragmentos do fluxo de notas gerado.
#include <cstring>
#include "Types.h"
#include "Rng.h"
#include "Params.h"
#include "RhythmEngine.h"

namespace ps {

class BeatRepeat {
public:
    Rng rng{4242};
    void reset() { nHist = 0; nWin = 0; }

    // 'io' entra com as notas vivas do bloco e sai com o resultado (vivas filtradas + repetições).
    void process(double a, double b, const float* P, int bpb, bool enabled, NoteBuf& io) {
        record(io);
        if (!enabled) { nWin = 0; return; }
        // descarta janelas terminadas
        int k = 0; for (int i = 0; i < nWin; ++i) if (wins[i].e > a) wins[k++] = wins[i]; nWin = k;

        static const double kSize[7] = {0.125, 0.25, 1.0 / 6.0, 0.5, 1.0 / 3.0, 1.0, 2.0};
        double interval = beatsOf((int)P[pBrInterval], bpb), dur = beatsOf((int)P[pBrDuration], bpb);
        bool freeze = P[pBrFreeze] > 0.5f;
        int sizeIdx = clampi((int)P[pBrSize], 0, 6);
        double grid = freeze ? kSize[sizeIdx] : interval;
        if (freeze) dur = kSize[sizeIdx];

        for (double B = std::ceil(a / grid - 1e-9) * grid; B < b - 1e-12; B += grid) {
            if (nWin > 0 && wins[nWin - 1].e >= B - 1e-9) { if (freeze) wins[nWin - 1].e = B + dur; continue; }
            if (!(freeze || rng.chance(P[pBrChance]))) continue;
            int si = rng.chance(P[pBrRandom]) ? rng.range(0, 6) : sizeIdx;
            makeWindow(B, kSize[si], dur);
        }

        // filtra notas vivas dentro das janelas ("gate" do ao vivo)
        NoteBuf& tmp = scratch; tmp.clear();
        for (int i = 0; i < io.count; ++i) {
            const Note& n = io.n[i]; bool inside = false;
            for (int w = 0; w < nWin; ++w) if (n.t >= wins[w].s && n.t < wins[w].e) { inside = true; break; }
            if (inside && rng.chance(P[pBrMix])) continue;
            tmp.push(n);
        }
        // gera repetições
        float st = P[pBrStutter], gate = P[pBrGate], var = P[pBrVar];
        float inten = 0.4f + 0.6f * P[pBrIntensity];
        for (int w = 0; w < nWin; ++w) {
            Win& W = wins[w];
            long c0 = std::max(0L, (long)std::floor((a - W.s) / W.period));
            for (long c = c0; ; ++c) {
                double base = W.s + c * W.period;
                if (base >= b || base >= W.e) break;
                float vf = st >= 0 ? std::min(1.6f, 1.f + 0.15f * st * (float)c) : std::pow(1.f + 0.35f * st, (float)c);
                for (int i = 0; i < W.nf; ++i) {
                    Note n = W.frag[i]; double t = base + n.t; // n.t guarda tempo relativo
                    if (t < a || t >= b || t >= W.e) continue;
                    if (var > 0.f && rng.chance(var * 0.3f)) continue;
                    n.t = t; n.onDone = false;
                    n.len = std::max(0.01, std::min(n.len, W.period) * gate);
                    n.vel = (uint8_t)clampi((int)std::lround(n.vel * vf * inten), 1, 127);
                    if (var > 0.f && rng.chance(var * 0.3f)) {
                        static const int sh[6] = {-12, -7, -5, 5, 7, 12}; int s = sh[rng.range(0, 5)];
                        n.note = (uint8_t)clampi((int)n.note + s, 0, 127); n.pitchSemi += (float)s;
                    }
                    tmp.push(n);
                }
            }
        }
        std::memcpy(io.n, tmp.n, sizeof(Note) * (size_t)tmp.count); io.count = tmp.count;
    }

private:
    struct Win { double s = 0, e = 0, period = 0.25; Note frag[32]; int nf = 0; };
    Win wins[4]; int nWin = 0;
    Note hist[512]; int nHist = 0;
    NoteBuf scratch;

    static double beatsOf(int idx, int bpb) {
        switch (clampi(idx, 0, 4)) { case 0: return 1; case 1: return 2; case 2: return bpb; case 3: return 2.0 * bpb; default: return 4.0 * bpb; }
    }
    void record(const NoteBuf& io) {
        for (int i = 0; i < io.count; ++i) {
            if (nHist >= 512) { std::memmove(hist, hist + 64, sizeof(Note) * (512 - 64)); nHist -= 64; }
            hist[nHist++] = io.n[i];
        }
    }
    void makeWindow(double B, double size, double dur) {
        Win W; W.s = B; W.e = B + dur; W.period = size; W.nf = 0;
        for (int i = 0; i < nHist && W.nf < 32; ++i)
            if (hist[i].t >= B - size - 1e-9 && hist[i].t < B - 1e-9) { Note n = hist[i]; n.t = hist[i].t - (B - size); W.frag[W.nf++] = n; }
        if (W.nf == 0) return;
        if (nWin >= 4) { for (int i = 1; i < 4; ++i) wins[i - 1] = wins[i]; nWin = 3; }
        wins[nWin++] = W;
    }
};

} // namespace ps
