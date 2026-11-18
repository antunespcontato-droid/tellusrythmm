// Testes do motor (C++ puro). Compilar: g++ -std=c++17 -O1 -fsanitize=address,undefined -I../Source/Engine engine_tests.cpp
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <set>
#include <map>
#include <chrono>
#include <new>
#include "Engine.h"
#include "DrumSynth.h"
using namespace ps;

// ---- contador de alocações (para provar ausência de alocação no process) ----
static bool gCount = false; static long gAllocs = 0;
void* operator new(std::size_t n) { if (gCount) ++gAllocs; void* p = std::malloc(n ? n : 1); if (!p) throw std::bad_alloc(); return p; }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

static int gFail = 0, gPass = 0;
#define CHECK(c, ...) do { if (c) { ++gPass; } else { ++gFail; std::printf("  FAIL %s:%d  %s  ", __FILE__, __LINE__, #c); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static std::vector<ParamDef> gDefs;
static void defaults(float* P) { for (int i = 0; i < kNumParams; ++i) P[i] = gDefs[i].def; }

struct Ev { double abs; EvType type; int ch, d1, d2, lane; double ppq; };
struct Run { std::vector<Ev> ev; double sr; double bpm; };

// Executa 'bars' compassos de 4/4 em blocos de tamanho variável. 'hook' pode alterar P/engine a cada bloco.
template <class Hook>
static Run run(Engine& e, float* P, double bpm, double bars, int blockMode, Hook&& hook, double sr = 44100.0, double startPpq = 0.0, bool playing = true) {
    Run r; r.sr = sr; r.bpm = bpm; OutBuffer out;
    double totalSamples = bars * 4.0 * 60.0 / bpm * sr; long done = 0; unsigned rs = 17; int blk = 0;
    while (done < (long)totalSamples) {
        int n = blockMode > 0 ? blockMode : 32 + (int)((rs = rs * 1664525u + 1013904223u) >> 20) % 2000;
        n = (int)std::min<long>(n, (long)totalSamples - done); if (n <= 0) break;
        BlockInfo bi; bi.bpm = bpm; bi.sampleRate = sr; bi.numSamples = n; bi.playing = playing; bi.ppqStart = startPpq + (double)done / sr * bpm / 60.0;
        hook(blk++, e, P, bi);
        e.process(bi, P, out);
        for (int i = 0; i < out.count; ++i) {
            const OutEvent& o = out.e[i];
            CHECK(o.sample >= 0 && o.sample < n, "sample fora do bloco %d/%d", o.sample, n);
            r.ev.push_back({(double)done + o.sample, o.type, o.ch, o.d1, o.d2, o.lane, startPpq + (double)(done + o.sample) / sr * bpm / 60.0});
        }
        done += n;
    }
    return r;
}
static auto noHook = [](int, Engine&, float*, const BlockInfo&) {};
static std::vector<Ev> ons(const Run& r, int lane = -2) { std::vector<Ev> v; for (auto& e : r.ev) if (e.type == EvType::NoteOn && (lane == -2 || e.lane == lane)) v.push_back(e); return v; }
static int countType(const Run& r, EvType t) { int c = 0; for (auto& e : r.ev) c += e.type == t; return c; }

static void mainOnly(float* P) { P[pPercOn] = 0; P[pMainOn] = 1; }
static void setupMono(Engine& e, float* P) { defaults(P); mainOnly(P); P[pMelodyProb] = 0; P[pOctRange] = 0; P[pVelRand] = 0; P[pKeyFreeze] = 0; e.prepare(); e.seed(1); e.noteOn(60, P); }

static void testParams() {
    std::printf("[params]\n");
    CHECK(gDefs.size() == (size_t)kNumParams, "n=%zu", gDefs.size());
    std::set<std::string> ids;
    for (auto& d : gDefs) { CHECK(ids.insert(d.id).second, "id duplicado %s", d.id); CHECK(d.def >= d.mn && d.def <= d.mx, "default fora da faixa: %s", d.id);
        if (d.kind == KChoice && d.choices && d.choices[0]) { int c = 1; for (const char* p = d.choices; *p; ++p) c += *p == '|'; CHECK(c == (int)d.mx + 1, "choices de %s: %d vs max %g", d.id, c, d.mx); } }
    CHECK(numScales() + 1 == (int)gDefs[pScale].mx + 1, "escalas %d", numScales());
    CHECK(std::string(gDefs[pDensity].id) == "density", "ordem");
    CHECK(std::string(gDefs[laneParam(3, lpRes)].id) == "l3_res", "%s", gDefs[laneParam(3, lpRes)].id);
}
static void testScales() {
    std::printf("[scales]\n");
    ScaleCtx s; s.set(0, scaleMask(1, 0));  // C maior
    CHECK(s.quantize(61) == 60, "C# -> C"); CHECK(s.quantize(63) == 62 || s.quantize(63) == 64, "D# ");
    CHECK(s.stepDegrees(60, 2) == 64, "C+2 graus = E"); CHECK(s.stepDegrees(60, -1) == 59, "C-1 grau = B"); CHECK(s.stepDegrees(60, 7) == 72, "oitava");
    s.set(9, scaleMask(2, 0)); // A menor
    CHECK(s.stepDegrees(69, 2) == 72, "A+2=C"); CHECK(s.quantize(70) == 69 || s.quantize(70) == 71, "A#");
    for (int sc = 0; sc < numScales(); ++sc) { ScaleCtx c; c.set(3, scaleMask(sc, 0)); for (int n = 20; n < 100; ++n) { int q = c.quantize(n); int rel = ((q - 3) % 12 + 12) % 12; CHECK(scaleMask(sc, 0) & (1u << rel), "quantize fora da escala %d n=%d", sc, n); } }
}
static void testClock() {
    std::printf("[clock]\n");
    for (double sw : {0.0, 0.5, 1.0}) {
        std::map<long, int> cnt; double t = 0; unsigned rs = 5;
        while (t < 64.0) { double d = 0.01 + ((rs = rs * 1664525u + 1013904223u) >> 16) % 1000 / 400.0; forEachStep(t, t + d, 4.0, sw, [&](long g, double) { cnt[g]++; }); t += d; }
        bool ok = true; for (auto& kv : cnt) if (kv.second != 1) ok = false;
        CHECK(ok, "passo disparou != 1x (swing %g)", sw); CHECK(cnt.size() >= 250, "passos %zu", cnt.size());
    }
}
static void testGroove() {
    std::printf("[groove / mono]\n");
    for (double bpm : {60.0, 90.0, 128.0, 140.0, 174.0, 200.0}) {
        Engine e; float P[kNumParams]; setupMono(e, P);
        auto r = run(e, P, bpm, 4, 0, noHook);
        CHECK(ons(r).size() == 64, "bpm %g: %zu notas (esperado 64)", bpm, ons(r).size());
        // alinhamento: cada nota na grade de 1/16 (±1 amostra)
        double maxErr = 0; for (auto& o : ons(r)) { double st = o.ppq * 4; maxErr = std::max(maxErr, std::fabs(st - std::round(st)) * (60.0 / bpm / 4) * r.sr); }
        CHECK(maxErr <= 1.01, "erro de grade %.3f amostras (bpm %g)", maxErr, bpm);
        CHECK(countType(r, EvType::NoteOn) >= countType(r, EvType::NoteOff), "on/off");
    }
    { Engine e; float P[kNumParams]; setupMono(e, P); P[pDensity] = 0; CHECK(ons(run(e, P, 120, 4, 0, noHook)).size() == 0, "density 0"); }
    { Engine e; float P[kNumParams]; setupMono(e, P); P[pDensity] = 0.5f; size_t n = ons(run(e, P, 120, 16, 0, noHook)).size(); CHECK(n > 95 && n < 165, "density 50%%: %zu/256", n); }
    { Engine e; float P[kNumParams]; setupMono(e, P); P[pSwing] = 1; auto o = ons(run(e, P, 120, 2, 0, noHook)); bool ok = true;
      for (auto& x : o) { double st = x.ppq * 4; long g = (long)std::llround(std::floor(st + 1e-6)); (void)g; }
      // passos ímpares atrasados em meio passo
      int late = 0; for (auto& x : o) { double frac = x.ppq * 4 - std::floor(x.ppq * 4 + 1e-6); if (frac > 0.45 && frac < 0.55) ++late; } CHECK(late == (int)o.size() / 2, "swing: %d atrasadas de %zu", late, o.size()); (void)ok; }
    { Engine e; float P[kNumParams]; setupMono(e, P); P[pGroove4] = 1; auto o = ons(run(e, P, 120, 4, 0, noHook)); CHECK(o.size() == 16, "groove 1/4 = 100%%: %zu (esperado 16)", o.size()); }
    { Engine e; float P[kNumParams]; setupMono(e, P); P[pGlitch] = 1; auto o = ons(run(e, P, 120, 4, 0, noHook)); CHECK(o.size() > 90, "glitch gera repetições: %zu", o.size()); }
    { Engine e; float P[kNumParams]; setupMono(e, P); P[pHuman] = 1; auto o = ons(run(e, P, 120, 2, 0, noHook)); double mx = 0; for (auto& x : o) { double f = x.ppq * 4 - std::floor(x.ppq * 4 + 1e-6); mx = std::max(mx, f); } CHECK(mx > 0.05 && mx <= 0.51, "humanize deslocamento max %.3f passo", mx); }
    { Engine e; float P[kNumParams]; setupMono(e, P); P[pMelodyProb] = 1; P[pKeyFreeze] = 1; P[pScale] = 1; P[pRoot] = 0; P[pSteps] = 5; P[pOctRange] = 2; P[pOctChance] = 1; P[pShape] = 0;
      std::set<int> notes; ScaleCtx s; s.set(0, scaleMask(1, 0)); bool allIn = true;
      for (int sh = 0; sh < 5; ++sh) { P[pShape] = (float)sh; for (int dir = 0; dir < 4; ++dir) { P[pDirection] = (float)dir; auto o = ons(run(e, P, 120, 4, 0, noHook)); for (auto& x : o) { notes.insert(x.d1); if (s.quantize(x.d1) != x.d1) allIn = false; } } }
      CHECK(allIn, "Key Freeze: nota fora da escala"); CHECK(notes.size() > 6, "melodia varia: %zu notas distintas", notes.size()); }
}
static void testSequencer() {
    std::printf("[sequencer]\n");
    { Engine e; float P[kNumParams]; setupMono(e, P); P[pSource] = 1; e.bank.main.clear(); for (int i : {0, 4, 8, 12}) { e.bank.main.s[i].mode = 1; }
      auto o = ons(run(e, P, 120, 4, 0, noHook)); CHECK(o.size() == 16, "4 passos x 4 compassos = %zu", o.size());
      for (auto& x : o) { double beat = std::fmod(x.ppq, 1.0); CHECK(beat < 0.001 || beat > 0.999, "fora da semínima: %.4f", beat); } }
    { Engine e; float P[kNumParams]; setupMono(e, P); P[pSource] = 1; e.bank.main.clear(); for (int i = 0; i < 16; ++i) { e.bank.main.s[i].mode = 1; e.bank.main.s[i].pitch = (int8_t)i; }
      P[pSeqStart] = 5; P[pSeqEnd] = 8; auto o = ons(run(e, P, 120, 2, 0, noHook)); bool ok = o.size() == 32; for (size_t i = 0; i < o.size(); ++i) ok = ok && (o[i].d1 == 60 + 4 + (int)(i % 4));
      CHECK(ok, "loop 5..8 forward (%zu notas)", o.size());
      Engine e2; setupMono(e2, P); P[pSource] = 1; P[pSeqStart] = 5; P[pSeqEnd] = 8; P[pSeqDir] = 1; e2.bank = e.bank; o = ons(run(e2, P, 120, 1, 0, noHook)); ok = o.size() == 16; for (size_t i = 0; i < o.size(); ++i) ok = ok && (o[i].d1 == 60 + 7 - (int)(i % 4));
      CHECK(ok, "reverse");
      Engine e3; setupMono(e3, P); P[pSource] = 1; P[pSeqStart] = 5; P[pSeqEnd] = 8; P[pSeqDir] = 2; e3.bank = e.bank; o = ons(run(e3, P, 120, 1, 0, noHook));
      std::vector<int> exp = {4, 5, 6, 7, 6, 5, 4, 5, 6, 7, 6, 5, 4, 5, 6, 7}; ok = o.size() == 16; for (size_t i = 0; ok && i < o.size(); ++i) ok = o[i].d1 == 60 + exp[i]; CHECK(ok, "ping-pong"); }
    { Engine e; float P[kNumParams]; setupMono(e, P); P[pSource] = 1; e.bank.main.clear(); for (int i = 0; i < 16; ++i) { e.bank.main.s[i].mode = 1; e.bank.main.s[i].prob = (i % 2) ? 0.f : 1.f; }
      CHECK(ons(run(e, P, 120, 4, 0, noHook)).size() == 32, "probabilidade 0 nunca toca"); }
    { Engine e; float P[kNumParams]; setupMono(e, P); P[pSource] = 1; e.bank.main.clear(); e.bank.main.s[0].mode = 1; e.bank.main.s[0].ratchet = 4; e.bank.main.s[0].vel = 90;
      auto o = ons(run(e, P, 120, 1, 0, noHook)); CHECK(o.size() == 4, "ratchet 4 -> %zu", o.size()); }
    { Engine e; float P[kNumParams]; setupMono(e, P); P[pSource] = 1; P[pSeqLen] = 12; P[pSeqEnd] = 12; e.bank.main.clear(); for (int i = 0; i < 12; ++i) e.bank.main.s[i].mode = 1;
      CHECK(ons(run(e, P, 120, 3, 0, noHook)).size() == 48, "length 12 não afeta nº de disparos"); }
    { Engine e; float P[kNumParams]; setupMono(e, P); P[pSource] = 1; e.bank.main.clear(); e.bank.main.s[0].mode = 1; e.bank.main.s[0].accent = 1; e.bank.main.s[0].vel = 60; P[pAccentAmt] = 1;
      auto o = ons(run(e, P, 120, 1, 0, noHook)); CHECK(o.size() == 1 && o[0].d2 == 127, "accent 100%% -> vel %d", o.empty() ? -1 : o[0].d2); }
    { Engine e; float P[kNumParams]; setupMono(e, P); P[pSource] = 1; e.bank.main.clear(); for (int i = 0; i < 16; ++i) e.bank.main.s[i].mode = 2; P[pDensity] = 1;
      CHECK(ons(run(e, P, 120, 1, 0, noHook)).size() == 16, "passo em modo Groove usa gerador"); }
}
static void testPoly() {
    std::printf("[poly]\n");
    Engine e; float P[kNumParams]; setupMono(e, P); P[pMode] = 1; e.noteOn(64, P); e.noteOn(67, P); P[pChance] = 1; P[pExtend] = 0; P[pOctRange] = 0;
    for (int dir = 0; dir < 4; ++dir) { P[pDirection] = (float)dir; auto o = ons(run(e, P, 120, 2, 0, noHook)); bool ok = !o.empty(); std::set<int> s; for (auto& x : o) { s.insert(x.d1); if (x.d1 != 60 && x.d1 != 64 && x.d1 != 67) ok = false; } CHECK(ok, "poly dir %d notas fora do acorde", dir); CHECK(s.size() == 3, "poly dir %d usa todas as notas (%zu)", dir, s.size()); }
    P[pChordBlend] = 1; P[pVoices] = 3; P[pFlux] = 0; P[pSpread] = 0; P[pDirection] = 0; auto o = ons(run(e, P, 120, 1, 0, noHook)); std::map<long, int> grp; for (auto& x : o) grp[(long)std::llround(x.abs)]++;
    bool ok = !grp.empty(); for (auto& kv : grp) ok = ok && kv.second == 3; CHECK(ok, "fusion fader 100%% -> acordes de 3 vozes");
    P[pChordBlend] = 0; P[pStackProb] = 1; o = ons(run(e, P, 120, 1, 0, noHook)); CHECK(o.size() >= 32, "poly stack adiciona notas: %zu", o.size());
    e.allNotesOff(); P[pStackProb] = 0; CHECK(ons(run(e, P, 120, 1, 0, noHook)).size() == 0, "sem notas seguradas = silêncio");
}
static void testLatch() {
    std::printf("[latch]\n");
    Engine e; float P[kNumParams]; defaults(P); P[pLatch] = 1; e.prepare(); e.noteOn(60, P); e.noteOff(60, P); CHECK(e.rhythm.heldCount() == 1, "latch mantém");
    e.noteOn(64, P); CHECK(e.rhythm.heldCount() == 1, "nova nota substitui (latch)"); e.noteOn(67, P); CHECK(e.rhythm.heldCount() == 2, "acorde em latch");
    P[pLatch] = 0; Engine f; f.prepare(); f.noteOn(60, P); f.noteOff(60, P); CHECK(f.rhythm.heldCount() == 0, "sem latch solta");
}
static void testLooper() {
    std::printf("[looper]\n");
    Engine e; float P[kNumParams]; setupMono(e, P); P[pMelodyProb] = 1; P[pSteps] = 7; P[pKeyFreeze] = 0; P[pLoopLen] = 5; /* 1 compasso */
    auto hook = [](int, Engine&, float* P, const BlockInfo& bi) { P[pLoopOn] = bi.ppqStart > 3.01 ? 1.f : 0.f; };
    auto r = run(e, P, 120, 8, 441, hook);
    // a gravação começa em 4.0 (compasso 2) e termina em 8.0; depois repete idêntico
    auto o = ons(r); std::vector<std::pair<double, int>> rec, rep;
    for (auto& x : o) { if (x.ppq >= 4.0 - 1e-6 && x.ppq < 8.0 - 1e-6) rec.push_back({x.ppq - 4.0, x.d1}); else if (x.ppq >= 8.0 - 1e-6 && x.ppq < 12.0 - 1e-6) rep.push_back({std::fmod(x.ppq - 8.0, 4.0), x.d1}); }
    CHECK(rec.size() == 16, "gravou %zu", rec.size()); CHECK(rep.size() == 16, "repetiu %zu notas em 1 compasso", rep.size());
    bool same = rec.size() == rep.size(); for (size_t i = 0; same && i < rec.size(); ++i) same = std::fabs(rec[i].first - rep[i].first) < 0.01 && rec[i].second == rep[i].second;
    CHECK(same, "o loop repete exatamente o material gravado");
    // desligar o loop volta ao gerador
    Engine e2; setupMono(e2, P); P[pMelodyProb] = 1; P[pLoopLen] = 3;
    auto hook2 = [](int, Engine&, float* P, const BlockInfo& bi) { P[pLoopOn] = (bi.ppqStart >= 4.0 && bi.ppqStart < 12.0) ? 1.f : 0.f; };
    auto o2 = ons(run(e2, P, 120, 6, 0, hook2)); int after = 0; for (auto& x : o2) if (x.ppq >= 13.0) ++after; CHECK(after > 20, "gerador volta após desligar loop (%d)", after);
}
static void testCC() {
    std::printf("[cc/pitchbend]\n");
    Engine e; float P[kNumParams]; setupMono(e, P); P[pCc1Num] = 74; P[pCc1Src] = 0; P[pCc2Num] = 20; P[pCc2Src] = 1; P[pPbRand] = 1;
    auto r = run(e, P, 120, 2, 0, noHook); int cc74 = 0, cc20 = 0, pb = 0; for (auto& x : r.ev) { if (x.type == EvType::CC && x.d1 == 74) ++cc74; if (x.type == EvType::CC && x.d1 == 20) ++cc20; if (x.type == EvType::PitchBend) ++pb; }
    CHECK(cc74 == 32 && cc20 == 32, "cc74=%d cc20=%d", cc74, cc20); CHECK(pb > 20, "pitch-bend %d", pb);
}
static void testPerc() {
    std::printf("[perc]\n");
    Engine e; float P[kNumParams]; defaults(P); P[pMainOn] = 0; e.prepare(); e.seed(3); Rng r(1); applyPsyGroove(e.bank, 0, r);
    auto run1 = run(e, P, 140, 2, 0, noHook);
    CHECK(ons(run1, 0).size() == 8, "kick 4x4: %zu em 2 compassos", ons(run1, 0).size());
    CHECK(ons(run1, 3).size() == 8, "open hat off-beat: %zu", ons(run1, 3).size());
    for (auto& x : ons(run1, 3)) { double s = std::fmod(x.ppq * 4, 4.0); CHECK(std::fabs(s - 2.0) < 0.02, "ohat fora de contratempo %.3f", s); }
    CHECK(ons(run1, 2).size() >= 28, "closed hat 16avos: %zu", ons(run1, 2).size());
    for (auto& x : ons(run1)) CHECK(x.ch == 9, "canal 10");
    // mute / prob
    { Engine e2; float P2[kNumParams]; defaults(P2); P2[pMainOn] = 0; e2.prepare(); Rng r2(1); applyPsyGroove(e2.bank, 0, r2); P2[laneParam(0, lpMute)] = 1; P2[laneParam(1, lpProb)] = 0; auto rr = run(e2, P2, 140, 2, 0, noHook); CHECK(ons(rr, 0).empty() && ons(rr, 1).empty(), "mute/prob 0"); }
    // polirritmia: lane com 12 passos (padrão 4x4 => muda o ciclo)
    { Engine e2; float P2[kNumParams]; defaults(P2); P2[pMainOn] = 0; e2.prepare(); e2.bank.lane[7].clear(); e2.bank.lane[7].s[0].mode = 1; P2[laneParam(7, lpLen)] = 12; auto rr = run(e2, P2, 120, 3, 0, noHook); auto o = ons(rr, 7);
      CHECK(o.size() == 4, "lane 12 passos: %zu hits em 3 compassos", o.size()); if (o.size() >= 2) CHECK(std::fabs((o[1].ppq - o[0].ppq) - 3.0) < 0.01, "período 3 beats"); }
    // swing / ratchet / pan / pitch
    { Engine e2; float P2[kNumParams]; defaults(P2); P2[pMainOn] = 0; P2[pPercSwing] = 1; e2.prepare(); e2.bank.lane[4].clear(); for (int i = 0; i < 16; ++i) e2.bank.lane[4].s[i].mode = 1; auto rr = run(e2, P2, 120, 1, 0, noHook); int late = 0; for (auto& x : ons(rr, 4)) { double f = x.ppq * 4 - std::floor(x.ppq * 4 + 1e-6); if (f > 0.45) ++late; } CHECK(late == 8, "swing perc %d", late); }
    { Engine e2; float P2[kNumParams]; defaults(P2); P2[pMainOn] = 0; e2.prepare(); e2.bank.lane[5].clear(); e2.bank.lane[5].s[0].mode = 1; e2.bank.lane[5].s[0].ratchet = 3; e2.bank.lane[5].s[0].pitch = 5; auto rr = run(e2, P2, 120, 1, 0, noHook); auto o = ons(rr, 5); CHECK(o.size() == 3, "repetições %zu", o.size()); CHECK(!o.empty() && o[0].d1 == kLaneNote[5] + 5, "pitch na nota %d", o.empty() ? -1 : o[0].d1); }
    // variação / fill: último beat de cada 2 compassos recebe mais notas
    { Engine e2; float P2[kNumParams]; defaults(P2); P2[pMainOn] = 0; P2[pPercVar] = 1; P2[pPercFill] = 0; e2.prepare(); Rng r2(1); applyPsyGroove(e2.bank, 1, r2); Engine e3; float P3[kNumParams]; defaults(P3); P3[pMainOn] = 0; e3.prepare(); e3.bank = e2.bank;
      CHECK(ons(run(e2, P2, 140, 4, 0, noHook)).size() > ons(run(e3, P3, 140, 4, 0, noHook)).size() + 8, "fill adiciona notas"); }
    // randomização controlada
    { Engine e2; float P2[kNumParams]; defaults(P2); P2[pMainOn] = 0; P2[pPercRandom] = 1; e2.prepare(); e2.bank.lane[2].clear(); CHECK(ons(run(e2, P2, 120, 16, 0, noHook), 2).size() > 5, "random cria notas fantasma"); }
    for (int v = 0; v < 4; ++v) { PatternBank b; Rng r3(v); applyPsyGroove(b, v, r3); int on = 0; for (auto& p : b.lane) for (int i = 0; i < 16; ++i) on += p.s[i].mode == 1; CHECK(on > 8, "template %d tem notas", v); }
}
static void testBeatRepeat() {
    std::printf("[beat repeat]\n");
    Engine e; float P[kNumParams]; setupMono(e, P); P[pMelodyProb] = 1; P[pSteps] = 9; P[pBrOn] = 1; P[pBrMain] = 1; P[pBrChance] = 1; P[pBrInterval] = 3 /*2 compassos*/; P[pBrDuration] = 2 /*1 compasso*/; P[pBrSize] = 3 /*1/8*/; P[pBrMix] = 1; P[pBrVar] = 0; P[pBrStutter] = 0; P[pBrIntensity] = 1; P[pBrGate] = 1;
    auto o = ons(run(e, P, 120, 8, 0, noHook));
    // janela em [8,12) beats (a cada 2 compassos; a primeira não tem história em [0,..) então começa no compasso 3 e depois)
    std::vector<Ev> w; for (auto& x : o) if (x.ppq >= 8.0 - 1e-6 && x.ppq < 12.0 - 1e-6) w.push_back(x);
    CHECK(w.size() == 16, "janela tem %zu notas", w.size());
    bool rep = w.size() == 16; for (size_t i = 2; rep && i < w.size(); ++i) rep = w[i].d1 == w[i - 2].d1;   // período de 1/8 = 2 notas de 1/16
    CHECK(rep, "fragmento de 1/8 repete (período 2 notas)");
    // fora da janela, notas continuam variando (melodia aleatória)
    std::set<int> outside; for (auto& x : o) if (x.ppq >= 4.0 && x.ppq < 8.0 - 1e-6) outside.insert(x.d1); CHECK(outside.size() > 3, "fora da janela segue variando");
    // freeze (tempo real)
    Engine e2; setupMono(e2, P); P[pMelodyProb] = 1; P[pBrOn] = 1; P[pBrChance] = 0; P[pBrFreeze] = 0; P[pBrSize] = 1 /*1/16*/;
    auto hook = [](int, Engine&, float* P, const BlockInfo& bi) { P[pBrFreeze] = (bi.ppqStart >= 4.0 && bi.ppqStart < 6.0) ? 1.f : 0.f; };
    auto o2 = ons(run(e2, P, 120, 3, 0, hook)); std::set<int> fz; int cnt = 0; for (auto& x : o2) if (x.ppq >= 4.5 && x.ppq < 5.8) { fz.insert(x.d1); ++cnt; }
    CHECK(fz.size() <= 2 && cnt >= 4, "freeze repete 1 nota (distintas=%zu, n=%d)", fz.size(), cnt);
    // stutter decrescente
    Engine e3; setupMono(e3, P); P[pBrOn] = 1; P[pBrChance] = 1; P[pBrInterval] = 2; P[pBrDuration] = 2; P[pBrSize] = 3; P[pBrStutter] = -1; P[pBrFreeze] = 0; P[pMelodyProb] = 0;
    auto o3 = ons(run(e3, P, 120, 3, 0, noHook)); int first = 0, last = 0; for (auto& x : o3) if (x.ppq >= 4.0 && x.ppq < 4.5) first = std::max(first, x.d2); else if (x.ppq >= 7.0 && x.ppq < 8.0) last = std::max(last, x.d2);
    CHECK(first > last && last > 0, "stutter negativo reduz velocity (%d -> %d)", first, last);
}
static void testTransport() {
    std::printf("[transport / host sync]\n");
    // blocos de tamanhos diferentes => mesmas posições absolutas de nota
    std::vector<std::vector<double>> runs;
    for (int bs : {64, 256, 512, 1024, 0}) { Engine e; float P[kNumParams]; setupMono(e, P); auto r = run(e, P, 133.3, 4, bs, noHook); std::vector<double> t; for (auto& x : ons(r)) t.push_back(x.ppq); runs.push_back(t); }
    bool same = true; for (size_t i = 1; i < runs.size(); ++i) { same = same && runs[i].size() == runs[0].size(); for (size_t k = 0; same && k < runs[0].size(); ++k) same = std::fabs(runs[i][k] - runs[0][k]) < 0.002; }
    CHECK(same, "posições idênticas para qualquer tamanho de bloco (%zu notas)", runs[0].size());
    // todo note-on tem note-off; nada preso após stop
    { Engine e; float P[kNumParams]; setupMono(e, P); P[pGate] = 2; P[pGlitch] = 0.5f; P[pGroove2] = 1; auto r = run(e, P, 150, 4, 0, noHook);
      OutBuffer out; BlockInfo bi; bi.playing = false; bi.numSamples = 512; bi.sampleRate = 44100; bi.bpm = 150; e.process(bi, P, out); Run r2 = r; for (int i = 0; i < out.count; ++i) r2.ev.push_back({0, out.e[i].type, out.e[i].ch, out.e[i].d1, out.e[i].d2, out.e[i].lane, 0});
      std::map<int, int> open; bool bad = false; for (auto& x : r2.ev) { if (x.type == EvType::NoteOn) open[x.d1]++; else if (x.type == EvType::NoteOff) { if (--open[x.d1] < 0) bad = true; } }
      int stuck = 0; for (auto& kv : open) stuck += std::abs(kv.second); CHECK(!bad && stuck == 0, "notas presas após Stop (stuck=%d, off-sem-on=%d)", stuck, (int)bad); }
    // salto de transporte (loop do Ableton): flush
    { Engine e; float P[kNumParams]; setupMono(e, P); P[pGate] = 2; OutBuffer out; BlockInfo bi; bi.bpm = 120; bi.sampleRate = 44100; bi.numSamples = 4096; bi.playing = true; bi.ppqStart = 0; e.process(bi, P, out); bi.ppqStart = 100.0; e.process(bi, P, out);
      int offs = 0; for (int i = 0; i < out.count; ++i) offs += out.e[i].type == EvType::NoteOff && out.e[i].sample == 0; CHECK(offs >= 1, "salto de posição liberou notas (%d)", offs); }
    // free run com host parado
    { Engine e; float P[kNumParams]; setupMono(e, P); P[pFreeRun] = 1; auto r = run(e, P, 120, 2, 0, noHook, 44100.0, 0.0, false); CHECK(ons(r).size() == 32, "free run toca com host parado (%zu)", ons(r).size()); }
    { Engine e; float P[kNumParams]; setupMono(e, P); auto r = run(e, P, 120, 2, 0, noHook, 44100.0, 0.0, false); CHECK(ons(r).empty(), "parado sem free run = silêncio"); }
    // início no meio da música
    { Engine e; float P[kNumParams]; setupMono(e, P); auto r = run(e, P, 120, 1, 0, noHook, 44100.0, 37.5); CHECK(ons(r).size() == 16 && ons(r)[0].ppq >= 37.5 - 1e-3 && ons(r)[0].ppq < 37.76, "play a partir de PPQ arbitrário (1a nota em %.3f)", ons(r).empty() ? -1.0 : ons(r)[0].ppq); }
    // taxas de amostragem
    for (double sr : {44100.0, 48000.0, 88200.0, 96000.0}) { Engine e; float P[kNumParams]; setupMono(e, P); CHECK(ons(run(e, P, 128, 4, 0, noHook, sr)).size() == 64, "sr %g", sr); }
}
static void testPatterns() {
    std::printf("[patterns / state]\n");
    Pattern p; Rng r(9); p.randomFill(r, 0.6f, true); p.s[3].pitch = -7; p.s[3].pan = -0.5f; p.s[4].ratchet = 3;
    std::string s = p.toString(); Pattern q; CHECK(q.fromString(s), "parse"); CHECK(q.toString() == s, "roundtrip idêntico");
    CHECK(!q.fromString("lixo;"), "rejeita lixo");
    Pattern eu; eu.euclid(16, 5, 0); int c = 0; for (int i = 0; i < 16; ++i) c += eu.s[i].mode; CHECK(c == 5, "euclid(16,5) = %d", c);
    Pattern ro; ro.euclid(16, 4, 0); Pattern ro2 = ro; ro2.rotate(16, 1); CHECK(ro2.s[1].mode == ro.s[0].mode, "rotate");
}
static void testSynth() {
    std::printf("[drum synth]\n");
    DrumSynth d; d.prepare(44100); std::vector<float> L(44100), R(44100);
    for (int l = 0; l < kNumLanes; ++l) { std::fill(L.begin(), L.end(), 0.f); std::fill(R.begin(), R.end(), 0.f); d.trigger(l, 110, l % 2 ? -0.7f : 0.7f, 0.f, 0.1f); d.render(L.data(), R.data(), 44100, 0.8f);
      float pk = 0; bool fin = true; for (int i = 0; i < 44100; ++i) { pk = std::max(pk, std::max(std::fabs(L[i]), std::fabs(R[i]))); fin = fin && std::isfinite(L[i]) && std::isfinite(R[i]); }
      CHECK(fin, "lane %d finito", l); CHECK(pk > 0.05f && pk < 2.5f, "lane %d pico %.3f", l, pk); CHECK(d.activeVoices() == 0, "lane %d terminou", l); }
    d.trigger(3, 100, 0, 0, 1.f); d.trigger(2, 100, 0, 0, 0.1f); std::fill(L.begin(), L.end(), 0.f); d.render(L.data(), R.data(), 4410, 1.f); CHECK(d.activeVoices() <= 1, "closed hat corta open hat (%d)", d.activeVoices());
    for (int i = 0; i < 100; ++i) d.trigger(i % 8, 127, 0, 0, 0.1f); CHECK(d.activeVoices() <= 32, "polifonia limitada");
}
static void testRealtime() {
    std::printf("[realtime: alocação e CPU]\n");
    Engine* e = new Engine; float P[kNumParams]; defaults(P); P[pMelodyProb] = 1; P[pMode] = 1; P[pChordBlend] = 0.5f; P[pGlitch] = 0.6f; P[pBrOn] = 1; P[pBrChance] = 0.7f; P[pLoopOn] = 0; P[pCc1Num] = 74; P[pPercVar] = 0.5f; P[pPercRandom] = 0.5f; P[pStackProb] = 0.5f; P[pHuman] = 0.5f; P[pSwing] = 0.3f; P[pRes] = 3;
    e->prepare(); e->seed(5); e->noteOn(60, P); e->noteOn(64, P); e->noteOn(67, P); Rng r(2); applyPsyGroove(e->bank, 3, r); e->bank.main.randomFill(r, 0.6f, true);
    OutBuffer* out = new OutBuffer; DrumSynth* ds = new DrumSynth; ds->prepare(48000); std::vector<float> L(512), R(512);
    gAllocs = 0; gCount = true; auto t0 = std::chrono::high_resolution_clock::now(); long samples = 0; double ppq = 0;
    const double secs = 120.0; // 2 minutos de áudio, 174 BPM
    while (samples < secs * 48000) { BlockInfo bi; bi.bpm = 174; bi.sampleRate = 48000; bi.numSamples = 512; bi.playing = true; bi.ppqStart = ppq; e->process(bi, P, *out);
      std::fill(L.begin(), L.end(), 0.f); std::fill(R.begin(), R.end(), 0.f); for (int i = 0; i < out->count; ++i) if (out->e[i].type == EvType::NoteOn && out->e[i].lane >= 0) ds->trigger(out->e[i].lane, out->e[i].d2, out->e[i].pan, out->e[i].pitchSemi, out->e[i].lenSec);
      ds->render(L.data(), R.data(), 512, 0.8f); samples += 512; ppq += 512.0 / 48000 * 174 / 60; }
    gCount = false; double el = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t0).count();
    std::printf("  %.0f s de áudio (48 kHz, bloco 512, 174 BPM, carga pesada) processados em %.3f s => ~%.2f%% de 1 núcleo\n", secs, el, 100.0 * el / secs);
    CHECK(gAllocs == 0, "alocações no caminho de áudio: %ld", gAllocs); CHECK(el / secs < 0.05, "CPU %.2f%%", 100.0 * el / secs);
    delete e; delete out; delete ds;
}

int main() {
    gDefs.resize(kNumParams); buildParamDefs(gDefs.data());
    testParams(); testScales(); testClock(); testGroove(); testSequencer(); testPoly(); testLatch(); testLooper(); testCC(); testPerc(); testBeatRepeat(); testTransport(); testPatterns(); testSynth(); testRealtime();
    std::printf("\n%d verificações OK, %d falhas\n", gPass, gFail);
    return gFail ? 1 : 0;
}
