#pragma once
#include <array>
#include <cstddef>

#if defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#elif defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <emmintrin.h>
#endif

namespace stereoFir {
using Frame = std::array<double, 2>;

// Each SIMD lane follows one channel in the original tap order.
inline Frame convolve(const double *coefficients, const Frame *newest, int taps) {
#if defined(__aarch64__) || defined(_M_ARM64)
    auto sum = vdupq_n_f64(0.0);
    for (int tap = 0; tap < taps; ++tap)
        sum = vaddq_f64(sum, vmulq_f64(vdupq_n_f64(coefficients[tap]),
                                      vld1q_f64((newest - tap)->data())));
    Frame result;
    vst1q_f64(result.data(), sum);
    return result;
#elif defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
    auto sum = _mm_setzero_pd();
    for (int tap = 0; tap < taps; ++tap)
        sum = _mm_add_pd(sum, _mm_mul_pd(_mm_set1_pd(coefficients[tap]),
                                        _mm_loadu_pd((newest - tap)->data())));
    Frame result;
    _mm_storeu_pd(result.data(), sum);
    return result;
#else
    Frame result{};
    for (int tap = 0; tap < taps; ++tap)
        for (std::size_t channel = 0; channel < 2; ++channel)
            result[channel] += coefficients[tap] * (*(newest - tap))[channel];
    return result;
#endif
}
} // namespace stereoFir
