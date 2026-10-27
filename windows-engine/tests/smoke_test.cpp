#include "SubOnDsp.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
bool finiteFilter(subon::dsp::Biquad filter) {
    for (int i = 0; i < 48000; ++i) {
        const double input = (i == 0) ? 1.0 : 0.0;
        if (!std::isfinite(filter.process(input))) return false;
    }
    return true;
}
}

int main() {
    try {
        using namespace subon::dsp;
        if (!isValidConfiguration(48000.0, 30.0, 120.0) ||
            isValidConfiguration(48000.0, 120.0, 30.0) ||
            isValidConfiguration(48000.0, 30.0, std::numeric_limits<double>::infinity())) {
            std::cerr << "Filter configuration validation failed\n";
            return EXIT_FAILURE;
        }

        if (!finiteFilter(Biquad::lowPass(48000.0, 120.0)) ||
            !finiteFilter(Biquad::highPass(48000.0, 120.0)) ||
            !finiteFilter(Biquad::peakingEq(48000.0, 1000.0, 6.0)) ||
            !finiteFilter(Biquad::lowShelf(48000.0, 120.0, 6.0)) ||
            !finiteFilter(Biquad::highShelf(48000.0, 8000.0, -6.0))) {
            std::cerr << "A biquad filter produced non-finite output\n";
            return EXIT_FAILURE;
        }

        ProcessorConfig config;
        config.preampDb = 6.0;
        config.bassDb = 5.0;
        config.trebleDb = 2.0;
        config.equalizerDb[2] = 3.0;
        config.limiterEnabled = true;
        config.limiterCeiling = 0.8;
        if (!isValidProcessorConfig(config)) {
            std::cerr << "Expected valid processor configuration\n";
            return EXIT_FAILURE;
        }

        AudioProcessor processor(config);
        for (int i = 0; i < 48000; ++i) {
            const double input = (i % 2 == 0) ? 1.0 : -1.0;
            const double output = processor.process(input);
            if (!std::isfinite(output) || std::abs(output) > config.limiterCeiling) {
                std::cerr << "Limiter or processor output validation failed\n";
                return EXIT_FAILURE;
            }
        }
        processor.reset();
        if (!std::isfinite(processor.process(0.0)) ||
            processor.process(std::numeric_limits<double>::quiet_NaN()) != 0.0) {
            std::cerr << "Processor reset/non-finite input handling failed\n";
            return EXIT_FAILURE;
        }

        // Disabling the limiter must disable its ceiling rather than silently
        // clamping the signal. This also verifies that preamp gain is applied.
        ProcessorConfig noLimiter;
        noLimiter.preampDb = 12.0;
        noLimiter.limiterEnabled = false;
        noLimiter.limiterCeiling = 0.5;
        AudioProcessor unprotected(noLimiter);
        const double boosted = unprotected.process(0.5);
        if (!std::isfinite(boosted) || boosted <= noLimiter.limiterCeiling) {
            std::cerr << "Disabled limiter unexpectedly applied its ceiling\\n";
            return EXIT_FAILURE;
        }

        // A strong transient must remain inside the configured ceiling.
        ProcessorConfig limiterConfig;
        limiterConfig.limiterEnabled = true;
        limiterConfig.limiterCeiling = 0.7;
        AudioProcessor limited(limiterConfig);
        for (int i = 0; i < 2000; ++i) {
            const double input = i == 0 ? 4.0 : 0.05;
            const double output = limited.process(input);
            if (!std::isfinite(output) || std::abs(output) > limiterConfig.limiterCeiling) {
                std::cerr << "Limiter transient/release validation failed\\n";
                return EXIT_FAILURE;
            }
        }

        config.sampleRate = 32000.0;
        if (isValidProcessorConfig(config)) {
            std::cerr << "Expected invalid EQ configuration below the highest band Nyquist limit\n";
            return EXIT_FAILURE;
        }

        BassBandFilter band(48000.0, 30.0, 120.0);
        band.reset();
        if (!std::isfinite(band.process(0.25))) {
            std::cerr << "Bass band filter produced non-finite output\n";
            return EXIT_FAILURE;
        }

        std::cout << "SubON DSP tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "SubON DSP test failed: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
