#include "AudioCurves.h"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

static void require(bool pass, const char *message) {
    if (!pass)
        throw std::runtime_error(message);
}
static void close(double actual, double expected) {
    require(std::isfinite(actual) && std::abs(actual - expected) <= .03 * std::abs(expected),
            "Transfer checkpoint exceeds 3% relative error");
}
int main() try {
    // Original calibration checkpoints, including the small offsets at x = .5.
    close(audioCurves::stage1(0), 0x1.75508bd8b989ep+7);
    close(audioCurves::stage1(0.25), 0x1.73455ca9bc3a0p+7);
    close(audioCurves::stage1(0.5), 0x1.a79b8d04a8568p+5);
    close(audioCurves::stage1(0.75), 0x1.3f7b40add9b6fp+0);
    close(audioCurves::stage1(1), 0x1.3d2fbe1dbaa55p-1);
    close(audioCurves::stage2(0), 0x1.145ff7b1246bdp+8);
    close(audioCurves::stage2(0.25), 0x1.109b8d902b102p+8);
    close(audioCurves::stage2(0.5), 0x1.4bfc7b175b2c5p+3);
    close(audioCurves::stage2(0.75), 0x1.886ff995c9624p+1);
    close(audioCurves::stage2(1), 0x1.b3fc69378d7a2p+0);
    close(audioCurves::stage3(0), 0x1.d70e7535b9e49p+4);
    close(audioCurves::stage3(0.25), 0x1.d8d397a8d9dd6p+4);
    close(audioCurves::stage3(0.5), 0x1.65c2af15cf000p-14);
    close(audioCurves::stage3(0.75), -0x1.a6873ebab5e81p+6);
    close(audioCurves::stage3(1), -0x1.ab478ddd473e0p+6);
    close(audioCurves::stage4(0), 0x1.2f87b6c1bfe42p+5);
    close(audioCurves::stage4(0.25), 0x1.2e49a3587d9c2p+5);
    close(audioCurves::stage4(0.5), 0x1.ae856e2a42000p-15);
    close(audioCurves::stage4(0.75), -0x1.86586b528f963p+6);
    close(audioCurves::stage4(1), -0x1.8c83be7291e7dp+6);
    const std::array<double (*)(double), 4> curves{
        audioCurves::stage1, audioCurves::stage2, audioCurves::stage3, audioCurves::stage4};
    for (auto curve : curves) {
        for (int i = 0; i <= 2047 * 256; ++i)
            require(std::isfinite(curve(static_cast<double>(i) / (2047 * 256))),
                    "Non-finite transfer output");
        for (double x : {-1., 2., -std::numeric_limits<double>::infinity(),
                          std::numeric_limits<double>::infinity()})
            require(curve(x) == curve(std::clamp(x, 0., 1.)), "Transfer clamp changed");
        for (double x : {1023. / 2047, 1024. / 2047}) {
            const double left = curve(std::nextafter(x, 0.));
            const double right = curve(std::nextafter(x, 1.));
            require(std::abs(left - right) < 1.e-5, "Discontinuous central branch");
        }
    }
    std::cout << "Transfer calibration, finite domain, clamping and continuity checks passed\n";
    return 0;
} catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
}
