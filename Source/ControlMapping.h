#pragma once
#include <algorithm>
#include <array>
#include <cmath>

// Normalized controls, analog filter transforms and coefficient targets.
namespace controlMapping {
using Biquad = std::array<double, 5>; // b0,b1,a1,b2,a2
inline double taper(double normalized, bool drive = false) {
    constexpr std::array<double, 21> law{{0,     .0031, .0035, .0064, .0112, .0184, .0282,
                                          .0411, .0562, .0721, .0891, .1057, .1259, .1564,
                                          .1995, .2618, .3548, .5419, .7499, .891,  1}};
    const double position = std::clamp(normalized, 0.0, 1.0) * 20.0;
    const int i = static_cast<int>(position);
    const double first = (drive && i == 0) ? 0.0021 : law[static_cast<size_t>(i)];
    return (law[static_cast<size_t>(std::min(i + 1, 20))] - first) * (position - i) + first;
}
inline Biquad analog(double rate, double frequency, const std::array<double, 3> &numerator,
                     const std::array<double, 3> &denominator) {
    constexpr double pi = 3.141592653589793;
    frequency = std::min(rate * 0.4975, frequency);
    const double warp = (frequency * (2.0 * pi)) / std::tan((frequency * pi) / rate);
    double b0 = numerator[0], b1 = 0, b2 = 0, a0 = denominator[0], a1 = 0, a2 = 0;
    if (std::abs(numerator[2]) > 0 || std::abs(denominator[2]) > 0) {
        const double squared = warp * warp;
        const double n2 = numerator[2] * squared, d2 = denominator[2] * squared;
        const double n1 = warp * numerator[1], d1 = warp * denominator[1];
        b1 = (numerator[0] - n2);
        b1 += b1;
        a1 = (denominator[0] - d2);
        a1 += a1;
        b2 = (numerator[0] - n1) + n2;
        a2 = (denominator[0] - d1) + d2;
        b0 = (n1 + numerator[0]) + n2;
        a0 = (d1 + denominator[0]) + d2;
    } else if (std::abs(numerator[1]) > 0 || std::abs(denominator[1]) > 0) {
        const double n1 = numerator[1] * warp, d1 = warp * denominator[1];
        b1 = numerator[0] - n1;
        a1 = denominator[0] - d1;
        b0 = n1 + numerator[0];
        a0 = d1 + denominator[0];
    }
    const double reciprocal = 1.0 / a0;
    return {{reciprocal * b0, reciprocal * b1, reciprocal * a1, reciprocal * b2, reciprocal * a2}};
}
inline Biquad lowpass1(double rate, double frequency) {
    constexpr double twoPi = 6.283185307179586;
    const double omega = (std::min(frequency, rate * 0.4975) * twoPi) / rate;
    const double sine = std::sin(omega), cosine = std::cos(omega);
    double a0 = sine + 1.0;
    a0 += cosine;
    double a1 = sine - 1.0;
    a1 -= cosine;
    const double reciprocal = 1.0 / a0;
    return {{sine * reciprocal, sine * reciprocal, a1 * reciprocal, 0, 0}};
}
template <class Memory> inline void write(Memory &memory, unsigned offset, const Biquad &values) {
    for (unsigned i = 0; i < 5; ++i)
        memory[(offset + i * 8) / 8] = values[i];
}
template <class Memory>
inline void set(Memory &memory, int index, double value, double rate, int factor,
                bool masterBypass = false) {
    value = std::clamp(value, 0.0, 1.0);
    const double internalRate = static_cast<double>(factor) * rate;
    switch (index) {
    case 0:
        memory[0xdc8 / 8] = (!masterBypass && value > .5) ? 1.0 : 0.0;
        break;
    case 1: {
        const double amount = 1.0 - value;
        write(memory, 0xe08,
              analog(internalRate, 1000.0, {{1.0, amount * 0.020999999999999998, 0}},
                     {{1000000.0, 481.75000000000006, 0}}));
        write(memory, 0xe30,
              analog(internalRate, 1000.0, {{1.0, 0.01048175, 1.1750000000000001e-7}},
                     {{0, 2e-8, amount * 2.0000000000000002e-11}}));
        break;
    }
    case 2:
        memory[0xe80 / 8] = value > .5 ? 650.0 : 0.0;
        break;
    case 3:
        memory[0xbf8 / 8] =
            std::exp(value * 4.199705077879926 + 1.791759469228055) * (1.0 / internalRate);
        memory[0xc00 / 8] =
            std::exp(value * 2.4361164856185678 + 5.1647859739235145) * (1.0 / internalRate);
        break;
    case 4: {
        write(memory, 0xde0,
              analog(internalRate, 23.149809904275685, {{0, 0.006875, 0}},
                     {{1.0, 0.008249999999999999 * value + 0.0090375,
                       8.249999999999998e-7 * value + 2.0624999999999998e-7}}));
        const auto a = lowpass1(internalRate, 42762.84329539815);
        const auto b =
            lowpass1(internalRate, 1.0 / (value * 9.801769079200156e-5 + 2.450442269800039e-5));
        const double c0 = 2.0 - a[0], c1 = 2.0 * a[2] - a[1];
        write(memory, 0xe58,
              {{b[0] * c0, b[0] * c1 + b[1] * c0, a[2] + b[2], b[1] * c1, a[2] * b[2]}});
        break;
    }
    case 5:
        memory[0xe90 / 8] = taper(value) * 79.43282347242817;
        break;
    case 6:
        memory[0xdd8 / 8] = taper(value);
        break;
    case 7:
        memory[0xdd0 / 8] = taper(value, true) * 0.5011872336272722;
        break;
    case 8:
        memory[0xe88 / 8] = value;
        break;
    default:
        break;
    }
}
} // namespace controlMapping
