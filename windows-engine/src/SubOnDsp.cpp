#include "SubOnDsp.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace subon::dsp {
namespace {
constexpr double kPi = 3.14159265358979323846;

bool validFilterArgs(double sampleRate, double frequency) noexcept {
    return std::isfinite(sampleRate) && std::isfinite(frequency) &&
           sampleRate > 0.0 && frequency > 0.0 && frequency < sampleRate * 0.5;
}
double linearGain(double db) noexcept { return std::pow(10.0, db / 20.0); }
} // namespace

Biquad::Biquad(double b0, double b1, double b2, double a1, double a2) noexcept
    : b0_(b0), b1_(b1), b2_(b2), a1_(a1), a2_(a2) {}

Biquad Biquad::lowPass(double sampleRate, double cutoffHz, double q) {
    if (!validFilterArgs(sampleRate, cutoffHz) || !(q > 0.0) || !std::isfinite(q))
        throw std::invalid_argument("Invalid low-pass filter parameters");
    const double w0 = 2.0 * kPi * cutoffHz / sampleRate;
    const double c = std::cos(w0), s = std::sin(w0), alpha = s / (2.0 * q);
    const double a0 = 1.0 + alpha;
    return Biquad((1.0 - c) / (2.0 * a0), (1.0 - c) / a0,
                  (1.0 - c) / (2.0 * a0), (-2.0 * c) / a0, (1.0 - alpha) / a0);
}

Biquad Biquad::highPass(double sampleRate, double cutoffHz, double q) {
    if (!validFilterArgs(sampleRate, cutoffHz) || !(q > 0.0) || !std::isfinite(q))
        throw std::invalid_argument("Invalid high-pass filter parameters");
    const double w0 = 2.0 * kPi * cutoffHz / sampleRate;
    const double c = std::cos(w0), s = std::sin(w0), alpha = s / (2.0 * q);
    const double a0 = 1.0 + alpha;
    return Biquad((1.0 + c) / (2.0 * a0), -(1.0 + c) / a0,
                  (1.0 + c) / (2.0 * a0), (-2.0 * c) / a0, (1.0 - alpha) / a0);
}

Biquad Biquad::peakingEq(double sampleRate, double centerHz, double gainDb, double q) {
    if (!validFilterArgs(sampleRate, centerHz) || !(q > 0.0) || !std::isfinite(q) ||
        !std::isfinite(gainDb))
        throw std::invalid_argument("Invalid peaking EQ parameters");
    const double a = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * kPi * centerHz / sampleRate;
    const double c = std::cos(w0), alpha = std::sin(w0) / (2.0 * q);
    const double a0 = 1.0 + alpha / a;
    return Biquad((1.0 + alpha * a) / a0, (-2.0 * c) / a0,
                  (1.0 - alpha * a) / a0, (-2.0 * c) / a0,
                  (1.0 - alpha / a) / a0);
}

Biquad Biquad::lowShelf(double sampleRate, double cutoffHz, double gainDb, double slope) {
    if (!validFilterArgs(sampleRate, cutoffHz) || !(slope > 0.0) || !std::isfinite(slope) ||
        !std::isfinite(gainDb))
        throw std::invalid_argument("Invalid low-shelf parameters");
    const double a = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * kPi * cutoffHz / sampleRate;
    const double c = std::cos(w0), s = std::sin(w0);
    const double alpha = s * 0.5 * std::sqrt((a + 1.0 / a) * (1.0 / slope - 1.0) + 2.0);
    const double beta = 2.0 * std::sqrt(a) * alpha;
    const double a0 = (a + 1.0) + (a - 1.0) * c + beta;
    return Biquad(a * ((a + 1.0) - (a - 1.0) * c + beta) / a0,
                  2.0 * a * ((a - 1.0) - (a + 1.0) * c) / a0,
                  a * ((a + 1.0) - (a - 1.0) * c - beta) / a0,
                  -2.0 * ((a - 1.0) + (a + 1.0) * c) / a0,
                  ((a + 1.0) + (a - 1.0) * c - beta) / a0);
}

Biquad Biquad::highShelf(double sampleRate, double cutoffHz, double gainDb, double slope) {
    if (!validFilterArgs(sampleRate, cutoffHz) || !(slope > 0.0) || !std::isfinite(slope) ||
        !std::isfinite(gainDb))
        throw std::invalid_argument("Invalid high-shelf parameters");
    const double a = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * kPi * cutoffHz / sampleRate;
    const double c = std::cos(w0), s = std::sin(w0);
    const double alpha = s * 0.5 * std::sqrt((a + 1.0 / a) * (1.0 / slope - 1.0) + 2.0);
    const double beta = 2.0 * std::sqrt(a) * alpha;
    const double a0 = (a + 1.0) - (a - 1.0) * c + beta;
    return Biquad(a * ((a + 1.0) + (a - 1.0) * c + beta) / a0,
                  -2.0 * a * ((a - 1.0) + (a + 1.0) * c) / a0,
                  a * ((a + 1.0) + (a - 1.0) * c - beta) / a0,
                  2.0 * ((a - 1.0) - (a + 1.0) * c) / a0,
                  ((a + 1.0) - (a - 1.0) * c - beta) / a0);
}

double Biquad::process(double input) noexcept {
    const double output = b0_ * input + z1_;
    z1_ = b1_ * input - a1_ * output + z2_;
    z2_ = b2_ * input - a2_ * output;
    return output;
}
void Biquad::reset() noexcept { z1_ = 0.0; z2_ = 0.0; }

AudioProcessor::AudioProcessor(const ProcessorConfig& config) { configure(config); }

void AudioProcessor::configure(const ProcessorConfig& config) {
    if (!isValidProcessorConfig(config))
        throw std::invalid_argument("Invalid audio processor configuration");
    config_ = config;
    bassShelf_ = Biquad::lowShelf(config.sampleRate, 120.0, config.bassDb);
    trebleShelf_ = Biquad::highShelf(config.sampleRate, 8000.0, config.trebleDb);
    for (std::size_t i = 0; i < kEqualizerBandCount; ++i)
        equalizer_[i] = Biquad::peakingEq(config.sampleRate, kEqualizerFrequencies[i],
                                          config.equalizerDb[i], 1.0);
    reset();
}

double AudioProcessor::process(double input) noexcept {
    if (!std::isfinite(input)) return 0.0;
    double output = input * linearGain(config_.preampDb);
    output = bassShelf_.process(output);
    for (auto& band : equalizer_) output = band.process(output);
    output = trebleShelf_.process(output);
    if (!std::isfinite(output)) return 0.0;
    if (config_.limiterEnabled) {
        // Instant attack prevents an over-ceiling peak from passing through;
        // a slower release avoids reopening gain immediately after each peak.
        const double peak = std::abs(output);
        constexpr double release = 0.9995;
        limiterEnvelope_ = peak >= limiterEnvelope_
            ? peak
            : (std::max)(peak, limiterEnvelope_ * release);

        if (limiterEnvelope_ > config_.limiterCeiling) {
            output *= config_.limiterCeiling / limiterEnvelope_;
        }

        // Safety ceiling for extreme transients and floating-point edge cases.
        output = std::clamp(output, -config_.limiterCeiling, config_.limiterCeiling);
    } else {
        limiterEnvelope_ = 0.0;
    }
    return output;
}

void AudioProcessor::reset() noexcept {
    bassShelf_.reset();
    trebleShelf_.reset();
    for (auto& band : equalizer_) band.reset();
    limiterEnvelope_ = 0.0;
}

BassBandFilter::BassBandFilter(double sampleRate, double lowCutHz, double highCutHz)
    : highPass_(Biquad::highPass(sampleRate, lowCutHz)),
      lowPass_(Biquad::lowPass(sampleRate, highCutHz)) {
    if (!isValidConfiguration(sampleRate, lowCutHz, highCutHz))
        throw std::invalid_argument("Invalid bass-band filter configuration");
}
double BassBandFilter::process(double input) noexcept {
    return lowPass_.process(highPass_.process(input));
}
void BassBandFilter::reset() noexcept { highPass_.reset(); lowPass_.reset(); }

bool isValidConfiguration(double sampleRate, double lowCutHz, double highCutHz) noexcept {
    return std::isfinite(sampleRate) && std::isfinite(lowCutHz) && std::isfinite(highCutHz) &&
           sampleRate > 0.0 && lowCutHz > 0.0 && highCutHz > lowCutHz &&
           highCutHz < sampleRate * 0.5;
}

bool isValidProcessorConfig(const ProcessorConfig& config) noexcept {
    if (!std::isfinite(config.sampleRate) || config.sampleRate < 32000.0 ||
        !std::isfinite(config.preampDb) || !std::isfinite(config.bassDb) ||
        !std::isfinite(config.trebleDb) || !std::isfinite(config.limiterCeiling) ||
        config.limiterCeiling <= 0.0 || config.limiterCeiling > 1.0 ||
        std::abs(config.preampDb) > 24.0 || std::abs(config.bassDb) > 18.0 ||
        std::abs(config.trebleDb) > 18.0)
        return false;
    for (std::size_t i = 0; i < kEqualizerBandCount; ++i) {
        if (!std::isfinite(config.equalizerDb[i]) || std::abs(config.equalizerDb[i]) > 18.0 ||
            kEqualizerFrequencies[i] >= config.sampleRate * 0.5)
            return false;
    }
    return 8000.0 < config.sampleRate * 0.5;
}

} // namespace subon::dsp
