#pragma once
// Voz de percussão interna simples (síntese própria) para o módulo PERC. Sem alocação, polifonia fixa.
#include <cmath>
#include <cstdint>
#include "Types.h"

namespace ps {

class DrumSynth {
public:
    void prepare(double sr) { sampleRate = sr; for (auto& v : voices) v.on = false; }
    void reset() { for (auto& v : voices) v.on = false; }

    void trigger(int lane, float vel127, float pan, float semi, float lenSec) {
        if (lane < 0 || lane >= kNumLanes) return;
        if (lane == 2) for (auto& v : voices) if (v.on && v.lane == 3) v.choke = true;   // closed hat corta open hat
        Voice* slot = nullptr; double oldest = -1;
        for (auto& v : voices) { if (!v.on) { slot = &v; break; } if (v.t > oldest) { oldest = v.t; slot = &v; } }
        Voice& v = *slot;
        v = Voice(); v.on = true; v.lane = lane; v.vel = clampf(vel127 / 127.f, 0.f, 1.f); v.vel *= v.vel * 0.5f + 0.5f;
        float a = (clampf(pan, -1.f, 1.f) + 1.f) * 0.78539816f; v.gl = std::cos(a) * 1.41421356f; v.gr = std::sin(a) * 1.41421356f;
        v.ratio = std::pow(2.0, semi / 12.0); v.lenSec = lenSec; v.noise = 0x1234567u + (uint32_t)(lane * 7919) + counter++ * 2654435761u;
    }

    // soma no buffer (não zera)
    void render(float* L, float* R, int n, float gain) {
        const double dt = 1.0 / sampleRate;
        for (auto& v : voices) {
            if (!v.on) continue;
            for (int i = 0; i < n; ++i) {
                float s = sample(v, dt);
                if (v.choke) { v.chokeEnv *= 0.998f; s *= v.chokeEnv; }
                v.t += dt;
                if (!std::isfinite(s)) s = 0.f;
                s *= v.vel * gain;
                L[i] += s * v.gl; R[i] += s * v.gr;
                if (v.t > kLife[v.lane] || v.chokeEnv < 0.001f) { v.on = false; break; }
            }
        }
    }
    int activeVoices() const { int c = 0; for (auto& v : voices) c += v.on; return c; }

private:
    struct Voice {
        bool on = false, choke = false; int lane = 0; double t = 0, ph = 0, ph2 = 0; float vel = 1, gl = 1, gr = 1;
        double ratio = 1; float lenSec = 0.1f; uint32_t noise = 1; float lp1 = 0, lp2 = 0, hp1 = 0, chokeEnv = 1;
        double hph[6] = {0, 0, 0, 0, 0, 0};
    };
    static constexpr double kLife[kNumLanes] = {0.9, 0.5, 0.2, 1.0, 0.35, 0.12, 0.8, 0.7};
    Voice voices[32]; double sampleRate = 44100; uint32_t counter = 1;

    static float rnd(Voice& v) { v.noise ^= v.noise << 13; v.noise ^= v.noise >> 17; v.noise ^= v.noise << 5; return (float)(v.noise & 0xFFFFFF) / 8388608.f - 1.f; }
    static float hpf(float x, float& st, float a) { st += a * (x - st); return x - st; }   // passa-altas 1 polo
    static float lpf(float x, float& st, float a) { st += a * (x - st); return st; }
    static float tau(double t, double k) { return (float)std::exp(-t / k); }

    float sample(Voice& v, double dt) {
        const double t = v.t; const double TWO_PI = 6.283185307179586;
        switch (v.lane) {
            case 0: { // KICK
                double f = 48.0 * v.ratio * (1.0 + 5.0 * std::exp(-t / 0.025));
                v.ph += TWO_PI * f * dt;
                float env = tau(t, 0.22) * (t < 0.002 ? (float)(t / 0.002) : 1.f);
                float click = rnd(v) * tau(t, 0.003) * 0.25f;
                return std::tanh((float)std::sin(v.ph) * env * 1.6f + click);
            }
            case 1: { // CLAP
                float n = rnd(v); n = hpf(n, v.hp1, 0.09f); n = lpf(n, v.lp1, 0.45f);
                float e = 0.f; for (int k = 0; k < 3; ++k) if (t >= k * 0.012) e += tau(t - k * 0.012, 0.004);
                e += 0.8f * tau(t, 0.12);
                return n * e * 1.8f;
            }
            case 2: case 3: { // HATS (6 osciladores quadrados + passa-altas)
                static const double fr[6] = {205.3, 304.4, 369.6, 522.7, 540.0, 800.0};
                float s = 0.f; for (int k = 0; k < 6; ++k) { v.hph[k] += fr[k] * 2.7 * v.ratio * dt; if (v.hph[k] >= 1.0) v.hph[k] -= 1.0; s += v.hph[k] < 0.5 ? 1.f : -1.f; }
                s = hpf(s / 6.f + rnd(v) * 0.2f, v.hp1, 0.45f);
                float e = v.lane == 2 ? tau(t, 0.035) : tau(t, 0.22);
                if (v.lane == 3 && t > v.lenSec) e *= tau(t - v.lenSec, 0.03);
                return s * e * 1.2f;
            }
            case 4: { // SHAKER
                float n = hpf(rnd(v), v.hp1, 0.35f); n = lpf(n, v.lp1, 0.8f);
                float e = (t < 0.008 ? (float)(t / 0.008) : 1.f) * tau(t, 0.05);
                return n * e * 1.4f;
            }
            case 5: { // RIM
                v.ph += TWO_PI * 1700.0 * v.ratio * dt; v.ph2 += TWO_PI * 1000.0 * v.ratio * dt;
                float e = tau(t, 0.012);
                return ((float)std::sin(v.ph) * 0.6f + (float)std::sin(v.ph2) * 0.5f) * e + rnd(v) * tau(t, 0.002) * 0.3f;
            }
            case 6: { // TOM
                double f = 110.0 * v.ratio * (1.0 + 1.5 * std::exp(-t / 0.04));
                v.ph += TWO_PI * f * dt;
                return (float)std::sin(v.ph) * tau(t, 0.2);
            }
            default: { // PERC (FM metálico)
                double fc = 540.0 * v.ratio; double idx = 3.0 * std::exp(-t / 0.08);
                v.ph2 += TWO_PI * fc * 1.41 * dt;
                v.ph += TWO_PI * fc * dt;
                return (float)std::sin(v.ph + idx * std::sin(v.ph2)) * tau(t, 0.15) * 0.8f;
            }
        }
    }
};

} // namespace ps
