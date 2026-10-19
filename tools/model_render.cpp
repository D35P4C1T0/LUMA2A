// Raw interleaved float32 renderer for offline audio tests.
#include "Model.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
int main(int argc, char **argv) try {
    if (argc != 7 && argc != 17)
        throw std::runtime_error("Usage: model_render input.f32 output.f32 rate channels block "
                                 "float|double [10 normalized parameters]");
    const double rate = std::stod(argv[3]);
    const int channels = std::stoi(argv[4]), block = std::stoi(argv[5]);
    if (rate < 8000 || rate > 384000 || channels < 1 || channels > 2 || block < 1 || block > 65536)
        throw std::runtime_error("Invalid processing configuration");
    const std::string precision = argv[6];
    if (precision != "float" && precision != "double")
        throw std::runtime_error("Invalid precision");
    std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
    if (!input)
        throw std::runtime_error("Cannot read input");
    const auto bytes = input.tellg();
    if (bytes < 0 || bytes % (sizeof(float) * channels) != 0)
        throw std::runtime_error("Invalid raw input length");
    std::vector<float> audio(static_cast<size_t>(bytes) / sizeof(float));
    input.seekg(0);
    input.read(reinterpret_cast<char *>(audio.data()), bytes);
    Model model;
    model.prepare(rate, channels);
    if (argc == 17)
        for (int i = 0; i < 10; ++i)
            model.setParameter(i, static_cast<float>(std::stod(argv[7 + i])));
    const auto frames = audio.size() / static_cast<size_t>(channels);
    auto render = [&](auto sample) {
        using T = decltype(sample);
        std::array<std::vector<T>, 2> planes;
        std::array<T *, 2> pointers{};
        for (int c = 0; c < channels; ++c) {
            planes[c].resize(block);
            pointers[c] = planes[c].data();
        }
        for (size_t offset = 0; offset < frames; offset += static_cast<size_t>(block)) {
            const int count =
                static_cast<int>(std::min(frames - offset, static_cast<size_t>(block)));
            for (int c = 0; c < channels; ++c)
                for (int i = 0; i < count; ++i)
                    planes[c][i] = static_cast<T>(audio[(offset + i) * channels + c]);
            model.process(pointers.data(), pointers.data(), channels, count);
            for (int c = 0; c < channels; ++c)
                for (int i = 0; i < count; ++i)
                    audio[(offset + i) * channels + c] = static_cast<float>(planes[c][i]);
        }
    };
    if (precision == "float")
        render(float{});
    else
        render(double{});
    std::ofstream output(argv[2], std::ios::binary);
    if (!output)
        throw std::runtime_error("Cannot create output");
    output.write(reinterpret_cast<const char *>(audio.data()),
                 static_cast<std::streamsize>(audio.size() * sizeof(float)));
    if (!output)
        throw std::runtime_error("Output write failed");
    return 0;
} catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
}
