#pragma once
#include <string>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include "Types.h"
#include "Rng.h"

namespace ps {

// mode: 0 = desligado, 1 = toca, 2 = "Groove" (o gerador de groove decide; só no sequenciador principal)
//       PERC: 2 = "Variação" (só toca em compassos de fill)
struct Step {
    uint8_t mode = 0;
    uint8_t vel = 100;
    uint8_t accent = 0;
    uint8_t ratchet = 1;   // 1..4
    float gate = 0.8f;     // multiplicador do gate
    float prob = 1.f;      // 0..1
    int8_t pitch = 0;      // semitons
    float pan = 0.f;       // -1..1 (PERC)
};

struct Pattern {
    Step s[kMaxSteps];

    void clear() { for (auto& x : s) { x = Step(); } }

    void randomFill(Rng& r, float amount, bool grooveAllowed) {
        for (int i = 0; i < kMaxSteps; ++i) {
            Step st; 
            bool on = r.chance(amount);
            st.mode = on ? (grooveAllowed && r.chance(0.15f) ? 2 : 1) : 0;
            st.vel = (uint8_t)(60 + r.range(0, 67));
            st.accent = (i % 4 == 0 && r.chance(0.5f)) ? 1 : 0;
            st.gate = 0.4f + r.uni() * 0.8f;
            st.prob = r.chance(0.7f) ? 1.f : 0.5f + r.uni() * 0.4f;
            s[i] = st;
        }
    }
    void mutate(Rng& r, float amount) {
        for (auto& st : s) if (r.chance(amount)) {
            int k = r.range(0, 3);
            if (k == 0) st.mode = st.mode ? 0 : 1;
            else if (k == 1) st.vel = (uint8_t)clampi(st.vel + r.range(-25, 25), 1, 127);
            else if (k == 2) st.prob = clampf(st.prob + r.bi() * 0.3f, 0.05f, 1.f);
            else st.gate = clampf(st.gate + r.bi() * 0.4f, 0.1f, 2.f);
        }
    }
    void euclid(int length, int pulses, int rotation) {
        length = clampi(length, 1, kMaxSteps); pulses = clampi(pulses, 0, length);
        for (int i = 0; i < kMaxSteps; ++i) s[i].mode = 0;
        if (pulses == 0) return;
        for (int i = 0; i < length; ++i) {
            // Bresenham/Bjorklund-equivalente para ritmos euclidianos
            int pos = posMod((long)i - rotation, length);
            if (((long)pos * pulses) % length < pulses) s[i].mode = 1;
        }
    }
    void rotate(int length, int by) {
        Step tmp[kMaxSteps]; length = clampi(length, 1, kMaxSteps);
        for (int i = 0; i < length; ++i) tmp[posMod((long)i + by, length)] = s[i];
        for (int i = 0; i < length; ++i) s[i] = tmp[i];
    }
    void reverse(int length) { length = clampi(length, 1, kMaxSteps); for (int i = 0; i < length / 2; ++i) std::swap(s[i], s[length - 1 - i]); }

    std::string toString() const {
        std::string o; char b[128];
        for (auto& st : s) {
            std::snprintf(b, sizeof b, "%d,%d,%d,%d,%.3f,%.3f,%d,%.3f;", st.mode, st.vel, st.accent, st.ratchet, st.gate, st.prob, st.pitch, st.pan);
            o += b;
        }
        return o;
    }
    bool fromString(const std::string& str) {
        Pattern tmp; size_t pos = 0; int i = 0;
        while (i < kMaxSteps && pos < str.size()) {
            size_t e = str.find(';', pos); if (e == std::string::npos) break;
            int m, v, a, rt, pi; float g, p, pn;
            if (std::sscanf(str.substr(pos, e - pos).c_str(), "%d,%d,%d,%d,%f,%f,%d,%f", &m, &v, &a, &rt, &g, &p, &pi, &pn) != 8) return false;
            Step st; st.mode = (uint8_t)clampi(m, 0, 2); st.vel = (uint8_t)clampi(v, 1, 127); st.accent = (uint8_t)(a != 0);
            st.ratchet = (uint8_t)clampi(rt, 1, 4); st.gate = clampf(g, 0.05f, 2.f); st.prob = clampf(p, 0.f, 1.f);
            st.pitch = (int8_t)clampi(pi, -48, 48); st.pan = clampf(pn, -1.f, 1.f);
            tmp.s[i++] = st; pos = e + 1;
        }
        if (i != kMaxSteps) return false;
        *this = tmp; return true;
    }
};

struct PatternBank {
    Pattern main;
    Pattern lane[kNumLanes];
};

} // namespace ps
