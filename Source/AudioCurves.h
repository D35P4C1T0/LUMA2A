#pragma once
#include <algorithm>
#include <cstddef>

// Rational fits certified within 3% of the original transfer curves.
namespace audioCurves {
namespace detail {
template <std::size_t N> inline double polynomial(double x, const double (&c)[N]) {
    const double x2 = x * x;
    double even = c[(N - 1) / 2 * 2], odd = c[(N - 2) / 2 * 2 + 1];
    // Independent even/odd chains shorten the dependency path through each fit.
    for (std::size_t i = (N - 1) / 2; i > 0; --i)
        even = even * x2 + c[(i - 1) * 2];
    for (std::size_t i = (N - 2) / 2; i > 0; --i)
        odd = odd * x2 + c[(i - 1) * 2 + 1];
    return even + x * odd;
}

inline double curve0(double x) {
    const double t = (x - 0) * 2 - 1;
    constexpr double numerator[] = {
        0.0038682418604309987, -0.091697249050341156, 0.9793867314050928,
        -5.5216990174036553, 19.690681127831947, -38.095034503953457,
        48.724739539281998, -30.452109613424515, 8.2694695892123722,
    };
    constexpr double denominator[] = {
        7.3234052335502398e-05, -0.0010626640815016852, 0.008743207201664438,
        -0.02632959031347204, 0.23585683042696259, 0.69394267897047013,
        2.2862863249337186, 1.7534675525903478, 0.70452799778718778,
    };
    return polynomial(t, numerator) / polynomial(t, denominator);
}
inline double curve1(double x) {
    const double t = (x - 0) * 2 - 1;
    constexpr double numerator[] = {
        0.068059710562606845, 0.55747329393846756, 1.9523479831306227,
        2.7141838448295701, 4.8382619074829467, -3.3768479581226618,
        2.0095874126631936,
    };
    constexpr double denominator[] = {
        0.0065437696809709234, 0.082230743244149584, 0.43017680429749539,
        1.1680329624193673, 1.7373160494848374, 1.2966666083298297,
        0.40599779076309567,
    };
    return polynomial(t, numerator) / polynomial(t, denominator);
}
inline double curve2(double x) {
    const double t = (x - 0) * 4.0019550342130987 - 1;
    constexpr double numerator[] = {
        -2.892888959981434, 20.014513116958412, -57.809185947845044,
        89.242194861395603, -77.674928844898972, 36.13386539222298,
        -6.9630892628758145, -0.089204397896035487, 0.03870718892534257,
    };
    constexpr double denominator[] = {
        0.024364024590931121, -0.19329631290914739, 0.65839438599489442,
        -1.2473057264515972, 1.4184722001995145, -0.96768750039659435,
        0.36643746347777217, -0.059378501580183256,
    };
    return polynomial(t, numerator) / polynomial(t, denominator);
}
inline double curve3(double x) {
    const double t = (x - 0.50024425989252563) * 4.0019550342130987 - 1;
    constexpr double numerator[] = {
        -56.670883881621506, -226.21561996882065, -357.35064846156098,
        -127.76848410737526, -40.691958645466798, 0.17815769512618615,
    };
    constexpr double denominator[] = {
        0.13436582875497072, 0.6635560825953869, 1.3557262434288258,
        1.1301280520224928, 0.50072279874831049,
    };
    return polynomial(t, numerator) / polynomial(t, denominator);
}
inline double curve4(double x) {
    const double t = (x - 0) * 4.0019550342130987 - 1;
    constexpr double numerator[] = {
        -33.283442776887441, 101.40274972162871, -107.4447878634045,
        36.123506989892853, -1.1652845257363456, -0.22440298597492503,
    };
    constexpr double denominator[] = {
        0.21905333984874442, -0.88539261413162995, 1.3729258053661055,
        -0.94998163357964527, 0.25195668658187431,
    };
    return polynomial(t, numerator) / polynomial(t, denominator);
}
inline double curve5(double x) {
    const double t = (x - 0.50024425989252563) * 4.0019550342130987 - 1;
    constexpr double numerator[] = {
        -58.824610536157792, -222.30532540328744, -323.39408566304564,
        -102.99317074882678, -35.502269585401244, 0.45089515834139454,
    };
    constexpr double denominator[] = {
        0.15067045039705718, 0.71229343731199879, 1.3686728306534217,
        1.073860422187874, 0.43998169140328547,
    };
    return polynomial(t, numerator) / polynomial(t, denominator);
}
} // namespace detail
inline double stage1(double input) {
    const double x = std::clamp(input, 0.0, 1.0);
    return detail::curve0(x);
}
inline double stage2(double input) {
    const double x = std::clamp(input, 0.0, 1.0);
    return detail::curve1(x);
}
inline double stage3(double input) {
    const double x = std::clamp(input, 0.0, 1.0);
    const double position = x * 2047;
    if (position >= 1023 && position < 1024) {
        // Preserve the zero crossing exactly, where relative error is unbounded.
        const double f = position - 1023;
        constexpr double prev = 0x1.7f6659390c1fdp-2, cur = 0x1.0010d1b5ba0e0p-3,
                         next = -0x1.0010d1b5ba173p-3;
        return (((next + prev) * f + (next - prev)) * .5 - cur * f) * f + cur;
    }
    return (x - .5) * (x < .5 ? detail::curve2(x) : detail::curve3(x));
}
inline double stage4(double input) {
    const double x = std::clamp(input, 0.0, 1.0);
    const double position = x * 2047;
    if (position >= 1023 && position < 1024) {
        // Preserve the zero crossing exactly, where relative error is unbounded.
        const double f = position - 1023;
        constexpr double prev = 0x1.9202279bdc8b0p-2, cur = 0x1.0c4930a4eed86p-3,
                         next = -0x1.0c4930a4ee48ep-3;
        return (((next + prev) * f + (next - prev)) * .5 - cur * f) * f + cur;
    }
    return (x - .5) * (x < .5 ? detail::curve4(x) : detail::curve5(x));
}
} // namespace audioCurves
