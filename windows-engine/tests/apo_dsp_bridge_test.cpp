#include "SubOnApoDspBridge.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <cstring>
#include <cstdint>

namespace {
bool nearlyEqual(float a, float b, float epsilon = 1.0e-6f) {
    return std::abs(a - b) <= epsilon;
}
}

int main() {
    using namespace subon::dsp;

    ProcessorConfig config;
    config.preampDb = 6.0;
    config.limiterEnabled = false;
    ApoDspBridge bridge(config, 2);

    // Bypass copies a separate input buffer unchanged.
    const float input[] = {0.25f, -0.25f, 0.5f, -0.5f};
    float bypassOutput[] = {9.0f, 9.0f, 9.0f, 9.0f};
    if (!bridge.process(input, bypassOutput, 2, 2, false)) {
        std::cerr << "Valid bypass buffer rejected\n";
        return EXIT_FAILURE;
    }
    for (std::size_t i = 0; i < 4; ++i) {
        if (!nearlyEqual(bypassOutput[i], input[i])) {
            std::cerr << "Bypass changed audio samples\n";
            return EXIT_FAILURE;
        }
    }

    // Enabled processing must apply gain to the signal.
    float processed[] = {0.25f, -0.25f, 0.5f, -0.5f};
    if (!bridge.process(processed, processed, 2, 2, true)) {
        std::cerr << "Valid in-place buffer rejected\n";
        return EXIT_FAILURE;
    }
    if (!(std::abs(processed[0]) > std::abs(input[0]) &&
          std::abs(processed[1]) > std::abs(input[1]))) {
        std::cerr << "DSP bridge did not apply configured preamp\n";
        return EXIT_FAILURE;
    }

    // Invalid channel count must not alter output.
    float untouched[] = {0.1f, 0.2f};
    const float original[] = {0.1f, 0.2f};
    if (bridge.process(input, untouched, 1, 1, true) ||
        untouched[0] != original[0] || untouched[1] != original[1]) {
        std::cerr << "Invalid format changed output buffer\n";
        return EXIT_FAILURE;
    }

    if (bridge.process(nullptr, untouched, 1, 2, true)) {
        std::cerr << "Null input should be rejected\n";
        return EXIT_FAILURE;
    }
    if (!bridge.process(nullptr, nullptr, 0, 2, true)) {
        std::cerr << "Empty buffer should be accepted\n";
        return EXIT_FAILURE;
    }

    if (bridge.process(input, untouched,
            (std::numeric_limits<std::size_t>::max)() / sizeof(float) / 2 + 1, 2, false) ||
        untouched[0] != original[0] || untouched[1] != original[1]) {
        std::cerr << "Overflowing bypass changed output\n";
        return EXIT_FAILURE;
    }
    float overlapping[] = {1, 2, 3, 4, 5, 6};
    if (!bridge.process(overlapping, overlapping + 2, 2, 2, false) ||
        overlapping[2] != 1 || overlapping[3] != 2 || overlapping[4] != 3 || overlapping[5] != 4) {
        std::cerr << "Overlapping bypass corrupted samples\n";
        return EXIT_FAILURE;
    }
    const std::uint32_t bits[] = {0x80000000u, 0x7fc01234u, 0x7f800000u, 0xff800000u};
    float special[4], copied[4];
    std::memcpy(special, bits, sizeof(bits));
    if (!bridge.process(special, copied, 2, 2, false) ||
        std::memcmp(special, copied, sizeof(special)) != 0) {
        std::cerr << "Bypass did not preserve exact sample bits\n";
        return EXIT_FAILURE;
    }

    std::cout << "SubON APO DSP bridge tests passed\n";
    return EXIT_SUCCESS;
}
