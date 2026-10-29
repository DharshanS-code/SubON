#pragma once

#include "SubOnDsp.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace subon::dsp {

// Allocation-free, in-place adapter for interleaved IEEE-754 float audio.
// Construct/configure it before entering a real-time callback. Each channel
// owns independent filter history. The caller owns the buffer and its lifetime.
class InterleavedFloatProcessor {
public:
    static constexpr std::size_t kMaxChannels = 8;

    explicit InterleavedFloatProcessor(const ProcessorConfig& config,
                                       std::size_t channels)
        : channels_(channels),
          processors_{AudioProcessor(config), AudioProcessor(config),
                      AudioProcessor(config), AudioProcessor(config),
                      AudioProcessor(config), AudioProcessor(config),
                      AudioProcessor(config), AudioProcessor(config)} {
        if (channels == 0 || channels > kMaxChannels) {
            throw std::invalid_argument("Unsupported channel count");
        }
    }

    InterleavedFloatProcessor(const InterleavedFloatProcessor&) = delete;
    InterleavedFloatProcessor& operator=(const InterleavedFloatProcessor&) = delete;

    // Returns false for invalid arguments without touching the supplied buffer.
    // No allocations, locks, logging, or OS calls occur in this method.
    bool process(float* interleaved, std::size_t frames,
                 std::size_t channels) noexcept {
        if ((frames != 0 && interleaved == nullptr) || channels != channels_) {
            return false;
        }
        for (std::size_t frame = 0; frame < frames; ++frame) {
            for (std::size_t channel = 0; channel < channels_; ++channel) {
                const std::size_t index = frame * channels_ + channel;
                interleaved[index] = static_cast<float>(
                    processors_[channel].process(static_cast<double>(interleaved[index])));
            }
        }
        return true;
    }

    std::size_t channels() const noexcept { return channels_; }

    void reset() noexcept {
        for (std::size_t channel = 0; channel < channels_; ++channel) {
            processors_[channel].reset();
        }
    }

private:
    std::size_t channels_;
    std::array<AudioProcessor, kMaxChannels> processors_{};
};

} // namespace subon::dsp
