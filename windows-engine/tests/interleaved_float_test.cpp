#include "SubOnInterleavedFloat.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

int main() {
    using namespace subon::dsp;

    ProcessorConfig config;
    config.preampDb = 6.0;
    config.limiterEnabled = true;
    config.limiterCeiling = 0.75;

    InterleavedFloatProcessor stereo(config, 2);
    float samples[] = {0.5f, -0.5f, 1.0f, -1.0f, 0.25f, -0.25f};
    if (!stereo.process(samples, 3, 2)) {
        std::cerr << "Valid stereo buffer was rejected\n";
        return EXIT_FAILURE;
    }
    for (float sample : samples) {
        if (!std::isfinite(sample) || std::abs(sample) > 0.75001f) {
            std::cerr << "Processed float sample outside limiter ceiling\n";
            return EXIT_FAILURE;
        }
    }

    float untouched[] = {0.2f, 0.3f};
    if (stereo.process(untouched, 1, 1) || untouched[0] != 0.2f || untouched[1] != 0.3f) {
        std::cerr << "Mismatched channel count should reject without modifying buffer\n";
        return EXIT_FAILURE;
    }
    if (stereo.process(nullptr, 1, 2)) {
        std::cerr << "Null non-empty buffer should be rejected\n";
        return EXIT_FAILURE;
    }
    if (!stereo.process(nullptr, 0, 2)) {
        std::cerr << "Empty buffer should be accepted\n";
        return EXIT_FAILURE;
    }

    stereo.reset();
    std::cout << "SubON interleaved float adapter tests passed\n";
    return EXIT_SUCCESS;
}
