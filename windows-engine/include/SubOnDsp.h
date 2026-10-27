#pragma once

#include <cstddef>
#include <vector>

namespace subon::dsp {

// A second-order IIR section. State is kept per channel/stream instance.
// Construct a new filter when the sample rate or cutoff changes.
class Biquad {
public:
    Biquad() = default;

    static Biquad lowPass(double sampleRate, double cutoffHz, double q = 0.7071067811865476);
    static Biquad highPass(double sampleRate, double cutoffHz, double q = 0.7071067811865476);

    double process(double input) noexcept;
    void reset() noexcept;

private:
    double b0_ = 1.0;
    double b1_ = 0.0;
    double b2_ = 0.0;
    double a1_ = 0.0;
    double a2_ = 0.0;
    double z1_ = 0.0;
    double z2_ = 0.0;

    Biquad(double b0, double b1, double b2, double a1, double a2) noexcept;
};

// Lightweight stereo-independent filter chain. Create one instance per channel.
// This is a DSP primitive only; it does not capture, route, or intercept Windows
// system audio by itself.
class BassBandFilter {
public:
    BassBandFilter(double sampleRate, double lowCutHz, double highCutHz);

    double process(double input) noexcept;
    void reset() noexcept;

private:
    Biquad highPass_;
    Biquad lowPass_;
};

bool isValidConfiguration(double sampleRate, double lowCutHz, double highCutHz) noexcept;

} // namespace subon::dsp
