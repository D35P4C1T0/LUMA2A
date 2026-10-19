#pragma once
#include <array>
#include <cmath>
#include <limits>

// 33-tap polyphase interpolation, a 32*factor+1 tap decimator and
// a -72 dB Dolph-Chebyshev window. Coefficients are made off the audio thread.
namespace oversampling {
inline constexpr double pi = 3.141592653589793238462643383279502884;
inline double window(double index, int length) {
    if (index < 0.0 || index > static_cast<double>(length - 1))
        return 0.0;
    constexpr double ripple = 0.00025118864315095795;
    const double order = static_cast<double>(length - 1);
    const double half = order * 0.5;
    const double alpha = std::cosh(std::acosh(1.0 / ripple) / order);
    const double inverseLength = 1.0 / static_cast<double>(length);
    const double position = (index - half) * (2.0 * pi);
    double sum = 0.0, weighted = 0.0;
    for (int k = 1; static_cast<double>(k) <= half; ++k) {
        const double argument = alpha * std::cos((static_cast<double>(k) * pi) * inverseLength);
        const double value = std::abs(argument) <= 1.0 ? std::cos(order * std::acos(argument))
                                                       : std::cosh(order * std::acosh(argument));
        // Preserve multiplication order; do not contract into FMA.
        double angle = static_cast<double>(k) * position;
        angle *= inverseLength;
        weighted += std::cos(angle) * value;
        sum += value;
    }
    const double twiceRipple = ripple + ripple;
    return (twiceRipple * weighted + 1.0) / (twiceRipple * sum + 1.0);
}
inline double sinc(double value) {
    return std::abs(value) > std::numeric_limits<double>::epsilon() ? std::sin(value) / value : 1.0;
}
struct Coefficients {
    int factor = 1;
    std::array<std::array<double, 33>, 4> up{};
    std::array<double, 129> down{};
    void prepare(int multiple) {
        factor = multiple;
        const int total = 32 * factor + 1;
        const double centre = static_cast<double>(32 * factor) * 0.5;
        double cutoffUp = (0.995 * pi) / static_cast<double>(factor);
        if (factor == 1)
            cutoffUp = pi;
        const double upGain = (static_cast<double>(factor) * cutoffUp) * (1.0 / pi);
        for (int phase = 0; phase < factor; ++phase) {
            int index = phase;
            for (int tap = 0; tap < 33; ++tap, index += factor) {
                const double argument = (static_cast<double>(index) - centre) * cutoffUp;
                const double scaled = upGain * sinc(argument);
                up[static_cast<size_t>(phase)][static_cast<size_t>(tap)] =
                    window(static_cast<double>(index), total) * scaled;
            }
        }
        double cutoffDown = (0.95 * pi) / static_cast<double>(factor);
        if (factor == 1)
            cutoffDown = pi;
        const double downGain = cutoffDown * (1.0 / pi);
        for (int index = 0; index < total; ++index) {
            const double argument = (static_cast<double>(index) - centre) * cutoffDown;
            const double scaled = downGain * sinc(argument);
            down[static_cast<size_t>(index)] = window(static_cast<double>(index), total) * scaled;
        }
    }
};
} // namespace oversampling
