// In-process Release benchmark: excludes file I/O, allocation and model preparation.
#include "Model.h"
#include <chrono>
#include <iostream>
#include <vector>

int main() {
    constexpr int frames = 131072;
    std::array<std::vector<float>, 2> input, output;
    for (int ch = 0; ch < 2; ++ch) {
        input[ch].resize(frames);
        output[ch].resize(frames);
        for (int i = 0; i < frames; ++i)
            input[ch][i] = static_cast<float>(.35 * std::sin(i * .071 + ch * .3) +
                                             .12 * std::sin(i * .197));
    }
    volatile double checksum = 0;
    std::cout << "rate,channels,block,median_ns_per_frame\n";
    for (double rate : {48000., 96000., 192000.})
        for (int channels : {1, 2})
            for (int block : {64, 256}) {
                std::array<double, 5> timings{};
                for (double &timing : timings) {
                    Model model;
                    model.prepare(rate, channels);
                    model.setParameter(6, .6);
                    model.setParameter(7, .7);
                    float *src[2] = {input[0].data(), input[1].data()};
                    float *dst[2] = {output[0].data(), output[1].data()};
                    model.process(src, dst, channels, 4096);
                    const auto start = std::chrono::steady_clock::now();
                    for (int i = 0; i < frames; i += block) {
                        float *in[2] = {src[0] + i, src[1] + i};
                        float *out[2] = {dst[0] + i, dst[1] + i};
                        model.process(in, out, channels, block);
                        checksum = checksum + model.meterGainReduction() +
                                   model.meterVuNormalized();
                    }
                    timing = std::chrono::duration<double, std::nano>(
                                 std::chrono::steady_clock::now() - start).count() / frames;
                    checksum = checksum + output[0].back();
                    if (!std::isfinite(checksum))
                        return 1;
                }
                std::sort(timings.begin(), timings.end());
                std::cout << rate << ',' << channels << ',' << block << ',' << timings[2] << '\n';
            }
}
