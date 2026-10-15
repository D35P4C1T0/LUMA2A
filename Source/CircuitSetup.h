#pragma once
#include "ControlMapping.h"

// Rate-dependent circuit coefficients and DC equilibrium initialization.
namespace circuitSetup {
inline controlMapping::Biquad highpass1(double rate, double frequency) {
    const double omega = (std::min(frequency, rate * 0.4975) * 6.283185307179586) / rate;
    const double sine = std::sin(omega), cosine = std::cos(omega);
    double a0 = sine + 1.0;
    a0 += cosine;
    double a1 = sine - 1.0;
    a1 -= cosine;
    const double reciprocal = 1.0 / a0;
    return {{(cosine + 1.0) * reciprocal, (-1.0 - cosine) * reciprocal, a1 * reciprocal, 0, 0}};
}
inline controlMapping::Biquad highpass2(double rate, double frequency, double q) {
    const double omega = (std::min(frequency, rate * 0.4975) * 6.283185307179586) / rate;
    const double sine = std::sin(omega), cosine = std::cos(omega);
    double alpha = .5 / q;
    alpha *= sine;
    const double reciprocal = 1.0 / (alpha + 1.0);
    const double b0 = (cosine + 1.0) * .5;
    return {{b0 * reciprocal, (-cosine - 1.0) * reciprocal, (cosine * -2.0) * reciprocal,
             b0 * reciprocal, (1.0 - alpha) * reciprocal}};
}
template <class Memory>
inline void configure(Memory &memory, double rate, int factor,
                      const std::array<double, 10> &parameters) {
    auto put = [&](unsigned offset, double value) { memory[offset / 8] = value; };
    const double internalRate = static_cast<double>(factor) * rate;
    const double inverseRate = 1.0 / rate;
    put(0x938, inverseRate);
    controlMapping::write(memory, 0x940, highpass2(rate, 5.3, 1.5));
    controlMapping::write(memory, 0x968, highpass1(rate, 2.3));
    put(0x990, 1.0 - std::exp(-inverseRate));
    double tau = memory[0xb50 / 8] * memory[0xb48 / 8];
    tau *= internalRate;
    put(0xb58, 1.0 - std::exp(-1.0 / tau));
    tau = memory[0xb80 / 8] * memory[0xb78 / 8];
    tau *= internalRate;
    put(0xb88, 1.0 - std::exp(-1.0 / tau));
    controlMapping::write(memory, 0xc20, highpass1(internalRate, 3.3862753849339433));
    controlMapping::write(memory, 0xc58, highpass1(internalRate, 3.3862753849339433));
    put(0xcc8, std::sin(inverseRate * 6.283185307179586) * 2.7);
    put(0xcf0, 1.0 - std::exp(-20.0 / rate));
    for (int index = 0; index < 10; ++index)
        controlMapping::set(memory, index, parameters[static_cast<size_t>(index)], rate, factor);
    for (unsigned offset = 0xcf8; offset <= 0xdc0; offset += 8)
        memory[offset / 8] = memory[(offset + 0xd0) / 8];
}
} // namespace circuitSetup
