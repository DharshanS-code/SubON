#pragma once

#include <array>
#include <cstddef>
#include <vector>

namespace subon::dsp {

// Second-order IIR section. Keep one instance per channel and stream.
// Recreate a filter when its sample rate or parameters change.
class Biquad {
public:
    Biquad() = default;

    static Biquad lowPass(double sampleRate, double cutoffHz, double q = 0.7071067811865476);
    static Biquad highPass(double sampleRate, double cutoffHz, double q = 0.7071067811865476);
    static Biquad peakingEq(double sampleRate, double centerHz, double gainDb,
                            double q = 1.0);
    static Biquad lowShelf(double sampleRate, double cutoffHz, double gainDb,
                           double slope = 1.0);
    static Biquad highShelf(double sampleRate, double cutoffHz, double gainDb,
                            double slope = 1.0);

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

inline constexpr std::size_t kEqualizerBandCount = 10;
inline constexpr std::array<double, kEqualizerBandCount> kEqualizerFrequencies{
    31.25, 62.5, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0
};

struct ProcessorConfig {
    double sampleRate = 48000.0;
    double preampDb = 0.0;
    double bassDb = 0.0;
    double trebleDb = 0.0;
    std::array<double, kEqualizerBandCount> equalizerDb{};
    bool limiterEnabled = true;
    double limiterCeiling = 0.98;
};

// Mono processing chain. Create one processor per channel, outside the audio
// callback. Reconfigure outside the callback too; do not allocate in process().
class AudioProcessor {
public:
    explicit AudioProcessor(const ProcessorConfig& config = {});

    void configure(const ProcessorConfig& config);
    double process(double input) noexcept;
    void reset() noexcept;
    const ProcessorConfig& config() const noexcept { return config_; }

private:
    ProcessorConfig config_{};
    std::array<Biquad, kEqualizerBandCount> equalizer_{};
    Biquad bassShelf_{};
    Biquad trebleShelf_{};
    double limiterEnvelope_ = 0.0;
};

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
bool isValidProcessorConfig(const ProcessorConfig& config) noexcept;

} // namespace subon::dsp
