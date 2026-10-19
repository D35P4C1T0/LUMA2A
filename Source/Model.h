#pragma once
#include "AudioTables.h"
#include "CircuitInitial.h"
#include "CircuitSetup.h"
#include "ControlMapping.h"
#include "Oversampling.h"
#include "StereoFir.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

// Nonlinear optical attenuation and tube circuit model.
class Model {
  public:
    void prepare(double rate, int channels) {
        sampleRate = rate;
        channelCount = std::clamp(channels, 1, 2);
        reset();
    }
    void reset();
    void setParameter(int index, double normalized);
    void process(float **in, float **out, int channels, int frames) {
        processTyped(in, out, channels, frames);
    }
    void process(double **in, double **out, int channels, int frames) {
        processTyped(in, out, channels, frames);
    }
    double meterGainReduction() const noexcept { return reduction; }
    double meterGainReductionNormalized() const noexcept { return 1.0 - lastGain; }
    double meterVuNormalized() const noexcept {
        constexpr std::array<double, 12> db{{-24, -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3}};
        constexpr std::array<double, 12> values{
            {0, .04, .165, .262, .341, .45, .517, .597, .67, .77, .87, 1}};
        const double decibels = 20.0 * std::log10(std::max(2.220446049250313e-16,
                                                           memory[0xcc0 / 8] / 0.7071067811865476));
        double position = 1;
        if (decibels <= -24)
            position = 0;
        else
            for (unsigned i = 0; i < 11; ++i)
                if (db[i] <= decibels && decibels < db[i + 1]) {
                    position = (decibels - db[i]) * ((1.0 / 11.0) / (db[i + 1] - db[i]));
                    position += static_cast<double>(i) * (1.0 / 11.0);
                    break;
                }
        position = std::clamp(position, 0.0, 1.0) * 11;
        const unsigned i = static_cast<unsigned>(position);
        return (values[std::min(i + 1, 11U)] - values[i]) * (position - i) + values[i];
    }

  private:
    double sampleRate = 48000.0, reduction = 0, lastGain = 1;
    int channelCount = 2;
    std::array<double, 10> parameters{1, 1, 0, .75, 0, .24, 0, .25, 1, .5};
    std::array<double, 0x1200 / 8> memory{};
    std::array<std::array<double, 32>, 2> dryDelay{};
    unsigned delayIndex = 0;
    int oversampling = 4, upIndex = 0, downIndex = 0;
    bool hasProcessed = false;
    oversampling::Coefficients fir;
    // Mirrored rings provide contiguous tap access without per-tap wrapping.
    std::array<stereoFir::Frame, 66> upHistory{};
    std::array<stereoFir::Frame, 258> downHistory{};
    double &d(unsigned byteOffset) { return memory[byteOffset / 8]; }
    static double table(const std::array<double, 2048> &t, double input) {
        const double x = std::clamp(input, 0.0, 1.0) * 2047.0;
        const int i = static_cast<int>(x);
        const double f = x - i;
        const double prev = t[static_cast<unsigned>(std::max(0, i - 1))];
        const double cur = t[static_cast<unsigned>(i)];
        const double next = t[static_cast<unsigned>(std::min(2047, i + 1))];
        return (((next + prev) * f + (next - prev)) * .5 - cur * f) * f + cur;
    }
    double biquad(double x, unsigned coeff, unsigned state) {
        const double y = x * d(coeff) + d(state);
        d(state) = x * d(coeff + 8) - y * d(coeff + 16) + d(state + 8);
        d(state + 8) = x * d(coeff + 24) - y * d(coeff + 32);
        return y;
    }
    void updateVu(double selected) {
        const double velocity = d(0xcd8), coefficient = d(0xcc8);
        const double position = velocity * coefficient + d(0xce0);
        const double acceleration = (std::abs(selected) - velocity * d(0xcd0)) - position;
        d(0xcd8) = acceleration * coefficient + velocity;
        d(0xce0) = std::clamp(position, 0.0, 1.0);
        d(0xcc0) = d(0xce0);
    }
    double core(double *audio, int channels) {
        double sum = 0;
        for (int ch = 0; ch < channels; ++ch)
            sum += audio[ch] / channels;
        double v =
            table(audioTables::stage1, (d(0xd08) * sum * d(0xc10) - d(0xb60)) * .0833333333 + .5);
        d(0xb60) += (((275 - v) / d(0xb38) - v / d(0xb40)) * d(0xb48) - d(0xb60)) * d(0xb58);
        double u = (v - d(0xb68) - (d(0xba8) * d(0xd60) + d(0xbb8))) / (1 + d(0xd60) * d(0xd38));
        const double f1 = biquad(u, 0xd38, 0xba8);
        (void)biquad(f1, 0xd60, 0xbb8);
        v = table(audioTables::stage2, (u * d(0xba0) - d(0xb90)) * .025 + .5);
        d(0xb90) += (((275 - v) / d(0xb70)) * d(0xb78) - d(0xb90)) * d(0xb88);
        const double sc = biquad(v - d(0xb98), 0xd10, 0xbc8);
        const double target =
            std::exp(-d(0xbd8) / (std::abs(sc) + 2.220446049250313e-16)) * d(0xbe0);
        const double delta = target - d(0xc08);
        d(0xc08) = std::max(0.0, d(0xc08) + std::sqrt(std::abs(delta)) * delta *
                                                (delta > 0 ? d(0xbf8) : d(0xc00)));
        const double resistance = 1 / (d(0xc08) + d(0xbe8));
        const double a = 100000 / (resistance + 100000) * resistance;
        const double b = a + d(0xdb0);
        double feedback = 100000 / (b + 100000) * b;
        feedback /= feedback + 68000;
        d(0xc10) = feedback;
        const double gain = a / b * feedback;
        reduction = std::max(0.0, -20 * std::log10(std::max(1e-12, gain * d(0xc18))));
        for (int ch = 0; ch < channels; ++ch) {
            double s = biquad(gain * d(0xd00) * audio[ch], 0xd88, 0xc90 + ch * 16);
            s = table(audioTables::stage3, s * .0625 + .5);
            const double y = s * d(0xc20) + d(0xc48 + ch * 8);
            d(0xc48 + ch * 8) = s * d(0xc28) - y * d(0xc30);
            s = table(audioTables::stage4, y * .05 + .5);
            audio[ch] = s * d(0xc58) + d(0xc80 + ch * 8);
            d(0xc80 + ch * 8) = s * d(0xc60) - audio[ch] * d(0xc68);
        }
        return gain * d(0xc18);
    }
    template <class T> void processTyped(T **inputs, T **outputs, int channels, int frames);
};

inline void Model::reset() {
    memory.fill(0);
    delayIndex = 0;
    hasProcessed = false;
    reduction = 0;
    lastGain = 1;
    for (auto &c : dryDelay)
        c.fill(0);
    const auto &initial = circuitInitial::rate48000;
    for (unsigned i = 0; i < initial.size(); ++i)
        memory[0x938 / 8 + i] = initial[i];
    oversampling = sampleRate < 50000 ? 4 : sampleRate < 100000 ? 2 : 1;
    circuitSetup::configure(memory, sampleRate, oversampling, parameters);
    fir.prepare(oversampling);
    upIndex = downIndex = 0;
    for (auto &c : upHistory)
        c.fill(0);
    for (auto &c : downHistory)
        c.fill(0);
}
inline void Model::setParameter(int index, double normalized) {
    if (index < 0 || index >= 10)
        return;
    normalized = std::clamp(normalized, 0.0, 1.0);
    if (parameters[static_cast<unsigned>(index)] == normalized)
        return;
    parameters[static_cast<unsigned>(index)] = normalized;
    controlMapping::set(memory, index, normalized, sampleRate, oversampling);
    if (!hasProcessed)
        for (unsigned offset = 0xcf8; offset <= 0xdc0; offset += 8)
            d(offset) = d(offset + 0xd0);
}
template <class T> void Model::processTyped(T **inputs, T **outputs, int channels, int frames) {
    channels = std::clamp(channels, 1, 2);
    hasProcessed = true;
    lastGain = 1;
    const bool sleeping = d(0xdc8) != 1.0 && d(0xcf8) <= 2.220446049250313e-16;
    for (int frame = 0; frame < frames; ++frame) {
        for (unsigned offset = 0xcf8; offset <= 0xdc0; offset += 8)
            d(offset) += (d(offset + 0xd0) - d(offset)) * d(0xcf0);
        if (sleeping) {
            for (int ch = 0; ch < channels; ++ch) {
                const T x = inputs[ch][frame];
                outputs[ch][frame] = static_cast<T>(dryDelay[ch][delayIndex]);
                dryDelay[ch][delayIndex] = x;
            }
            updateVu(0);
            delayIndex = (delayIndex + 1) % 32;
            continue;
        }
        double up[2][4]{}, processed[2][4]{}, dry[2]{};
        double inputPeak = 0, outputPeak = 0;
        for (int ch = 0; ch < channels; ++ch) {
            const double x = inputs[ch][frame];
            dry[ch] = dryDelay[ch][delayIndex];
            dryDelay[ch][delayIndex] = x;
            const double nonlin = d(0x9a8 + ch * 8);
            const double y =
                biquad((1 - nonlin * nonlin * nonlin * nonlin) * x, 0x940, 0xa28 + ch * 16);
            d(0x9a8 + ch * 8) =
                std::clamp(nonlin + y * d(0x938) * d(0x998) - nonlin * d(0x990), -1.0, 1.0);
            inputPeak = std::max(inputPeak, std::abs(y * d(0xb28) * d(0xcb8)));
            upHistory[upIndex][ch] = y * d(0xb28) * 8.70142984443495;
            upHistory[upIndex + 33][ch] = upHistory[upIndex][ch];
        }
        if (channels == 2) {
            for (int phase = 0; phase < oversampling; ++phase) {
                const auto value = stereoFir::convolve(fir.up[phase].data(),
                                                       upHistory.data() + upIndex + 33, 33);
                up[0][phase] = value[0];
                up[1][phase] = value[1];
            }
        } else {
            for (int phase = 0; phase < oversampling; ++phase)
                for (int tap = 0; tap < 33; ++tap)
                    up[0][phase] += fir.up[phase][tap] * upHistory[upIndex + 33 - tap][0];
        }
        for (int phase = 0; phase < oversampling; ++phase) {
            double values[2]{up[0][phase], up[1][phase]};
            lastGain = core(values, channels);
            for (int ch = 0; ch < channels; ++ch)
                processed[ch][phase] = values[ch];
        }
        const int downLength = 32 * oversampling + 1;
        stereoFir::Frame down{};
        if (channels == 2)
            down = stereoFir::convolve(fir.down.data(),
                                       downHistory.data() + downIndex + downLength, downLength);
        else
            for (int tap = 0; tap < downLength; ++tap)
                down[0] += fir.down[tap] * downHistory[downIndex + downLength - tap][0];
        for (int ch = 0; ch < channels; ++ch) {
            double y = down[ch];
            for (int phase = 0; phase < oversampling; ++phase) {
                const int slot = (downIndex + phase) % downLength;
                downHistory[slot][ch] = processed[ch][phase];
                downHistory[slot + downLength][ch] = processed[ch][phase];
            }
            const double nonlin = d(0x9e8 + ch * 8);
            y /= 8.70142984443495;
            y = biquad((1 - nonlin * nonlin * nonlin * nonlin) * y, 0x968, 0xaa8 + ch * 16);
            d(0x9e8 + ch * 8) =
                std::clamp(nonlin + y * d(0x938) * d(0x9a0) - nonlin * d(0x990), -1.0, 1.0);
            const double wet = y * d(0xb30) * (d(0xdc0) / d(0xd00) / 129.0);
            const double mixed = dry[ch] + (wet - dry[ch]) * d(0xdb8) * d(0xcf8);
            outputPeak = std::max(outputPeak, std::abs(mixed * d(0xcb8)));
            outputs[ch][frame] = static_cast<T>(mixed);
        }
        const int mode = std::min(2, static_cast<int>(parameters[9] * 3));
        const double selected = mode == 0   ? inputPeak
                                : mode == 1 ? lastGain * 0.7071067811865476
                                            : outputPeak;
        updateVu(d(0xcf8) * selected);
        delayIndex = (delayIndex + 1) % 32;
        upIndex = (upIndex + 1) % 33;
        downIndex = (downIndex + oversampling) % downLength;
    }
}
