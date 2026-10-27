#include "SubOnDsp.h"

#include <cmath>
#include <stdexcept>

namespace subon::dsp {
namespace {
constexpr double kPi = 3.14159265358979323846;

Biquad makeBiquad(double b0, double b1, double b2,
                  double a0, double a1, double a2) {
    if (!std::isfinite(a0) || std::abs(a0) < 1e-12) {
        throw std::invalid_argument("Invalid biquad normalization coefficient");
    }
    // Normalize denominator so a0 = 1.
    return Biquad::lowPass(48000.0, 1000.0); // replaced by caller-specific construction below
}
} // namespace

Biquad::Biquad(double b0, double b1, double b2, double a1, double a2) noexcept
    : b0_(b0), b1_(b1), b2_(b2), a1_(a1), a2_(a2) {}

Biquad Biquad::lowPass(double sampleRate, double cutoffHz, double q) {
    if (!(sampleRate > 0.0) || !(cutoffHz > 0.0) || cutoffHz >= sampleRate * 0.5 ||
        !(q > 0.0) || !std::isfinite(sampleRate) || !std::isfinite(cutoffHz) || !std::isfinite(q)) {
        throw std::invalid_argument("Invalid low-pass filter parameters");
    }
    const double w0 = 2.0 * kPi * cutoffHz / sampleRate;
    const double c = std::cos(w0);
    const double s = std::sin(w0);
    const double alpha = s / (2.0 * q);
    const double a0 = 1.0 + alpha;
    return Biquad((1.0 - c) / (2.0 * a0),
                  (1.0 - c) / a0,
                  (1.0 - c) / (2.0 * a0),
                  (-2.0 * c) / a0,
                  (1.0 - alpha) / a0);
}

Biquad Biquad::highPass(double sampleRate, double cutoffHz, double q) {
    if (!(sampleRate > 0.0) || !(cutoffHz > 0.0) || cutoffHz >= sampleRate * 0.5 ||
        !(q > 0.0) || !std::isfinite(sampleRate) || !std::isfinite(cutoffHz) || !std::isfinite(q)) {
        throw std::invalid_argument("Invalid high-pass filter parameters");
    }
    const double w0 = 2.0 * kPi * cutoffHz / sampleRate;
    const double c = std::cos(w0);
    const double s = std::sin(w0);
    const double alpha = s / (2.0 * q);
    const double a0 = 1.0 + alpha;
    return Biquad((1.0 + c) / (2.0 * a0),
                  -(1.0 + c) / a0,
                  (1.0 + c) / (2.0 * a0),
                  (-2.0 * c) / a0,
                  (1.0 - alpha) / a0);
}

double Biquad::process(double input) noexcept {
    // Transposed Direct Form II.
    const double output = b0_ * input + z1_;
    z1_ = b1_ * input - a1_ * output + z2_;
    z2_ = b2_ * input - a2_ * output;
    return output;
}

void Biquad::reset() noexcept {
    z1_ = 0.0;
    z2_ = 0.0;
}

BassBandFilter::BassBandFilter(double sampleRate, double lowCutHz, double highCutHz)
    : highPass_(Biquad::highPass(sampleRate, lowCutHz)),
      lowPass_(Biquad::lowPass(sampleRate, highCutHz)) {
    if (!isValidConfiguration(sampleRate, lowCutHz, highCutHz)) {
        throw std::invalid_argument("Invalid bass-band filter configuration");
    }
}

double BassBandFilter::process(double input) noexcept {
    return lowPass_.process(highPass_.process(input));
}

void BassBandFilter::reset() noexcept {
    highPass_.reset();
    lowPass_.reset();
}

bool isValidConfiguration(double sampleRate, double lowCutHz, double highCutHz) noexcept {
    return std::isfinite(sampleRate) && std::isfinite(lowCutHz) && std::isfinite(highCutHz) &&
           sampleRate > 0.0 && lowCutHz > 0.0 && highCutHz > lowCutHz &&
           highCutHz < sampleRate * 0.5;
}

} // namespace subon::dsp
