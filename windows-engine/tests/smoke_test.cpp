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

        // Non-finite samples must not poison a filter permanently.
        auto recoverable = Biquad::lowPass(48000.0, 120.0);
        recoverable.process(std::numeric_limits<double>::infinity());
        const double recovered = recoverable.process(1.0);
        if (!std::isfinite(recovered) || recovered == 0.0)
            throw std::runtime_error("Biquad did not recover after non-finite input");
        ProcessorConfig extremeConfig;
        extremeConfig.preampDb = 24.0;
        extremeConfig.limiterEnabled = false;
        AudioProcessor extreme(extremeConfig);
        extreme.process((std::numeric_limits<double>::max)());
        if (!(extreme.process(0.1) > 0.0))
            throw std::runtime_error("Overflow permanently poisoned processor state");

        // Previously accepted shelf slopes and huge finite gains could create
        // NaN coefficients during construction rather than rejecting the input.
        bool rejected = false;
        try { (void)Biquad::lowShelf(48000.0, 120.0, 18.0, 100.0); }
        catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) throw std::runtime_error("Invalid shelf radicand accepted");
        rejected = false;
        try { (void)Biquad::peakingEq(48000.0, 120.0, 1.0e308); }
        catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) throw std::runtime_error("Non-finite filter coefficients accepted");

        // Invalid configure calls must leave both coefficients and history alone.
        AudioProcessor unchanged(noLimiter), reference(noLimiter);
        unchanged.process(0.2);
        reference.process(0.2);
        auto invalidConfig = noLimiter;
        invalidConfig.sampleRate = 32000.0;
        rejected = false;
        try { unchanged.configure(invalidConfig); }
        catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected || unchanged.config().sampleRate != noLimiter.sampleRate ||
            unchanged.process(0.1) != reference.process(0.1))
            throw std::runtime_error("Rejected configuration changed processor state");
        invalidConfig.sampleRate = 384001.0;
        if (isValidProcessorConfig(invalidConfig))
            throw std::runtime_error("Excessive sample rate accepted");

        // Release time is measured in seconds rather than callback sample count.
        ProcessorConfig at48k, at96k;
        at96k.sampleRate = 96000.0;
        AudioProcessor release48(at48k), release96(at96k);
        release48.process(4.0);
        release96.process(4.0);
        double output48 = 0.0, output96 = 0.0;
        for (int i = 0; i < 480; ++i) output48 = release48.process(0.1);
        for (int i = 0; i < 960; ++i) output96 = release96.process(0.1);
        if (std::abs(output48 - output96) > 1.0e-10)
            throw std::runtime_error("Limiter release depends on sample rate");

        // Verify the bass-band filter actually separates low and high tones.
        auto rms = [](double frequency) {
            BassBandFilter filter(48000.0, 30.0, 120.0);
            double energy = 0.0;
            constexpr double pi = 3.14159265358979323846;
            for (int i = 0; i < 48000; ++i) {
                const double output = filter.process(std::sin(2.0 * pi * frequency * i / 48000.0));
                if (i >= 24000) energy += output * output;
            }
            return std::sqrt(energy / 24000.0);
        };
        if (!(rms(60.0) > 10.0 * rms(1000.0)))
            throw std::runtime_error("Bass-band frequency response is incorrect");

        std::cout << "SubON DSP tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "SubON DSP test failed: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
