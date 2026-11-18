#pragma once
#include <cstdint>
namespace ps {
// xorshift64* – rápido, determinístico (reprodutível por seed), sem alocação.
struct Rng {
    uint64_t s = 0x9E3779B97F4A7C15ull;
    explicit Rng(uint64_t seed = 0x9E3779B97F4A7C15ull) { reseed(seed); }
    void reseed(uint64_t seed) { s = seed ? seed : 0x9E3779B97F4A7C15ull; for (int i = 0; i < 4; ++i) next(); }
    uint32_t next() { s ^= s >> 12; s ^= s << 25; s ^= s >> 27; return (uint32_t)((s * 0x2545F4914F6CDD1Dull) >> 32); }
    float uni() { return (next() >> 8) * (1.0f / 16777216.0f); }           // [0,1)
    float bi() { return uni() * 2.f - 1.f; }                              // [-1,1)
    bool chance(float p) { return p >= 1.f ? true : (p <= 0.f ? false : uni() < p); }
    int range(int a, int b) { if (b <= a) return a; return a + (int)(next() % (uint32_t)(b - a + 1)); } // inclusivo
};
} // namespace ps
