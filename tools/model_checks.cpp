#include "Model.h"
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

using Audio = std::array<std::vector<float>, 2>;
static void require(bool pass, const char *message) {
    if (!pass)
        throw std::runtime_error(message);
}
static Audio signal(int length) {
    Audio x;
    for (auto &c : x)
        c.resize(length);
    uint32_t rng = 0x554241;
    for (int i = 0; i < length; ++i) {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        x[0][i] = float(rng >> 8) / 16777216.f - .5f;
        x[1][i] = -.73f * x[0][i];
    }
    return x;
}
static void render(Model &model, Audio &x, int block) {
    for (int at = 0; at < static_cast<int>(x[0].size()); at += block) {
        float *p[2] = {x[0].data() + at, x[1].data() + at};
        model.process(p, p, 2, std::min(block, static_cast<int>(x[0].size()) - at));
    }
}
int main() try {
    std::array<stereoFir::Frame, 258> history{};
    std::array<double, 129> coefficients{};
    for (size_t i = 0; i < history.size(); ++i)
        history[i] = {{std::sin(static_cast<double>(i) * .17),
                       std::cos(static_cast<double>(i) * .31)}};
    for (size_t i = 0; i < coefficients.size(); ++i)
        coefficients[i] = std::sin(static_cast<double>(i) * .23) / 129.0;
    for (int taps : {33, 65, 129})
        for (int newest = taps - 1; newest < taps * 2; ++newest) {
            stereoFir::Frame expected{};
            for (int tap = 0; tap < taps; ++tap)
                for (size_t ch = 0; ch < 2; ++ch)
                    expected[ch] += coefficients[static_cast<size_t>(tap)] *
                                    history[static_cast<size_t>(newest - tap)][ch];
            const auto actual = stereoFir::convolve(coefficients.data(), history.data() + newest,
                                                     taps);
            require(std::memcmp(expected.data(), actual.data(), sizeof(expected)) == 0,
                    "Stereo FIR differs from scalar tap order");
        }
    const auto input = signal(8192);
    for (double rate : {8000., 22050., 32000., 44100., 48000., 50000., 88200., 96000., 100000.,
                        176400., 192000., 384000.}) {
        Model whole, chunked;
        whole.prepare(rate, 2);
        chunked.prepare(rate, 2);
        for (auto *m : {&whole, &chunked}) {
            m->setParameter(6, .6f);
            m->setParameter(4, .5f);
        }
        auto a = input, b = input;
        render(whole, a, 8192);
        render(chunked, b, 127);
        for (int c = 0; c < 2; ++c) {
            require(std::memcmp(a[c].data(), b[c].data(), a[c].size() * sizeof(float)) == 0,
                    "Processing depends on block partition");
            for (float value : a[c])
                require(std::isfinite(value), "Non-finite audio");
        }
        whole.reset();
        auto recalled = input;
        render(whole, recalled, 511);
        for (int c = 0; c < 2; ++c)
            require(std::memcmp(a[c].data(), recalled[c].data(), a[c].size() * sizeof(float)) == 0,
                    "Reset does not reproduce prepared state");
        Model dry;
        dry.prepare(rate, 2);
        dry.setParameter(8, 0);
        auto bypassed = input;
        render(dry, bypassed, 256);
        for (int c = 0; c < 2; ++c)
            for (size_t i = 0; i < bypassed[c].size(); ++i)
                require(bypassed[c][i] == (i < 32 ? 0.f : input[c][i - 32]),
                        "Dry mix is not delayed by 32 samples");
        Model off;
        off.prepare(rate, 2);
        off.setParameter(0, 0);
        auto poweredOff = input;
        render(off, poweredOff, 256);
        for (int c = 0; c < 2; ++c)
            for (size_t i = 0; i < poweredOff[c].size(); ++i)
                require(poweredOff[c][i] == (i < 32 ? 0.f : input[c][i - 32]),
                        "Power bypass is not delayed by 32 samples");
    }
    Model separate, inplace;
    separate.prepare(48000, 2);
    inplace.prepare(48000, 2);
    auto a = input, b = input, out = signal(8192);
    float *src[2] = {a[0].data(), a[1].data()};
    float *dest[2] = {out[0].data(), out[1].data()};
    separate.process(src, dest, 2, 8192);
    render(inplace, b, 256);
    for (int c = 0; c < 2; ++c)
        require(std::memcmp(out[c].data(), b[c].data(), out[c].size() * sizeof(float)) == 0,
                "In-place processing differs from separate buffers");
    std::cout << "Stereo FIR equivalence and 12 sample rates: finite audio, block partition, "
                 "reset, bypass latency and in-place checks passed\n";
    return 0;
} catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
}
