#include "SubOnInterleavedFloat.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

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

    // Reject both sample-count and byte-count overflow before accessing data.
    const auto maxSize = (std::numeric_limits<std::size_t>::max)();
    if (stereo.process(untouched, maxSize / 2 + 1, 2) ||
        stereo.process(untouched, maxSize / sizeof(float) / 2 + 1, 2) ||
        untouched[0] != 0.2f || untouched[1] != 0.3f) {
        std::cerr << "Overflowing frame count was not safely rejected\n";
        return EXIT_FAILURE;
    }
    bool rejected = false;
    try { InterleavedFloatProcessor invalid(config, 0); }
    catch (const std::invalid_argument&) { rejected = true; }
    if (!rejected) return EXIT_FAILURE;

    ProcessorConfig boostedConfig;
    boostedConfig.preampDb = 24.0;
    boostedConfig.limiterEnabled = false;
    InterleavedFloatProcessor boosted(boostedConfig, 2);
    float extremes[] = {(std::numeric_limits<float>::max)(), 0.0f};
    if (!boosted.process(extremes, 1, 2) || !std::isfinite(extremes[0]) || extremes[1] != 0.0f) {
        std::cerr << "Float overflow or cross-channel contamination\n";
        return EXIT_FAILURE;
    }

    // Chunk boundaries must not reset history or couple independent channels.
    config.bassDb = 8.0;
    InterleavedFloatProcessor whole(config, 2), chunked(config, 2);
    float all[20] = {0.5f, 0.0f};
    float chunks[20] = {0.5f, 0.0f};
    whole.process(all, 10, 2);
    chunked.process(chunks, 3, 2);
    chunked.process(chunks + 6, 7, 2);
    for (int i = 0; i < 20; ++i) {
        if (all[i] != chunks[i] || (i % 2 && all[i] != 0.0f)) return EXIT_FAILURE;
    }
    stereo.reset();
    std::cout << "SubON interleaved float adapter tests passed\n";
    return EXIT_SUCCESS;
}
