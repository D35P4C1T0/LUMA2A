#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
namespace sessionState {
struct State {
    std::array<float, 10> parameters{};
    std::array<std::array<float, 10>, 4> banks{};
    float bypass = 0;
    int activeBank = 0;
};
inline uint32_t read32(const uint8_t *data) {
    return uint32_t(data[0]) << 24 | uint32_t(data[1]) << 16 | uint32_t(data[2]) << 8 |
           uint32_t(data[3]);
}
inline const std::array<uint32_t, 14> ids{
    {1296202096u, 1113681515u, 1349482354u, 1381970743u, 1299145829u, 1382248310u, 1097296238u,
     1197566318u, 1348821355u, 1148348790u, 1148344180u, 1298494578u, 1299466066u, 1299469909u}};
// Session format: a 128-byte version-1 FsCk with 14 BE ID/float pairs,
// optionally followed by a 336-byte version-1 NeCk containing four banks.
inline std::optional<State> parse(const void *input, size_t size) {
    if (input == nullptr || (size != 128 && size != 464))
        return {};
    auto *bytes = static_cast<const uint8_t *>(input);
    if (std::memcmp(bytes, "FsCk", 4) != 0 || read32(bytes + 4) != 1 || read32(bytes + 8) != 128 ||
        read32(bytes + 12) != 14)
        return {};
    State state;
    std::array<bool, 14> seen{};
    for (size_t n = 0; n < 14; ++n) {
        auto *entry = bytes + 16 + n * 8;
        auto id = read32(entry);
        size_t index = 0;
        while (index < ids.size() && ids[index] != id)
            ++index;
        if (index == ids.size() || seen[index])
            return {};
        seen[index] = true;
        auto bits = read32(entry + 4);
        float value;
        std::memcpy(&value, &bits, 4);
        if (!std::isfinite(value) || value < 0 || value > 1)
            return {};
        if (index == 0)
            state.bypass = value;
        else if (index == 1)
            state.activeBank = static_cast<int>(std::lround(value * 3));
        else if (index < 12)
            state.parameters[index - 2] = value;
    }
    for (auto &bank : state.banks)
        bank = state.parameters;
    if (size == 128)
        return state;
    if (std::memcmp(bytes + 128, "NeCk", 4) != 0 || read32(bytes + 132) != 1 ||
        read32(bytes + 136) != 336 || read32(bytes + 140) != 10)
        return {};
    for (size_t b = 0; b < 4; ++b) {
        std::array<bool, 10> bankSeen{};
        for (size_t n = 0; n < 10; ++n) {
            auto *entry = bytes + 144 + (b * 10 + n) * 8;
            auto id = read32(entry);
            size_t index = 0;
            while (index < 10 && ids[index + 2] != id)
                ++index;
            if (index == 10 || bankSeen[index])
                return {};
            bankSeen[index] = true;
            auto bits = read32(entry + 4);
            float value;
            std::memcpy(&value, &bits, 4);
            if (!std::isfinite(value) || value < 0 || value > 1)
                return {};
            state.banks[b][index] = value;
        }
    }
    return state;
}
} // namespace sessionState
