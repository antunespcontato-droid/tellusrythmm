#pragma once
#include <cmath>
#include "Types.h"
namespace ps {

constexpr double kResSpb[6] = {1, 2, 4, 8, 3, 6};   // passos por semínima: 1/4, 1/8, 1/16, 1/32, 1/8T, 1/16T

// Percorre todos os passos cujo instante (com swing) cai em [start, end). Cada passo dispara exatamente uma vez
// quando os blocos são contíguos (o instante com swing é monotônico em g para swing <= 1).
template <class F>
inline void forEachStep(double start, double end, double spb, double swing, F&& f) {
    long g0 = (long)std::floor(start * spb) - 1;
    long g1 = (long)std::ceil(end * spb) + 1;
    for (long g = g0; g <= g1; ++g) {
        double pos = (double)g + ((g & 1) ? swing * 0.5 : 0.0);
        double t = pos / spb;
        if (t >= start && t < end) f(g, t);
    }
}

} // namespace ps
