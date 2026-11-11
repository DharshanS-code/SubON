#pragma once

#include "SubOnInterleavedFloat.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace subon::dsp {

// APO-facing, allocation-free buffer bridge. Construct it and configure DSP
// parameters before the Windows audio engine enters its real-time callback.
// The APO callback should pass the current immutable enable-state snapshot;
// this class never reads UI state, locks, logs, allocates, or calls the OS.
class ApoDspBridge {
public:
    ApoDspBridge(const ProcessorConfig& config, std::size_t channels)
        : processor_(config, channels), channels_(channels) {}

    ApoDspBridge(const ApoDspBridge&) = delete;
    ApoDspBridge& operator=(const ApoDspBridge&) = delete;

    // Interleaved float32 input/output buffers. Supports separate or identical
    // input/output pointers. On invalid arguments, returns false and does not
    // modify the output buffer.
    bool process(const float* input, float* output, std::size_t frames,
                 std::size_t channels, bool enabled) noexcept {
        if (channels != channels_ ||
            (frames != 0 && (input == nullptr || output == nullptr))) {
            return false;
        }

        if (frames > (std::numeric_limits<std::size_t>::max)() / sizeof(float) / channels_) {
            return false;
        }
        const std::size_t sampleCount = frames * channels_;

        if (input != output && sampleCount != 0) {
            // Preserve sample bits during bypass, including NaN payloads, and
            // allow overlapping caller-owned buffers without corrupting input.
            std::memmove(output, input, sampleCount * sizeof(float));
        }

        // Bypass is a transparent copy and deliberately does not advance DSP
        // state. A host may choose to reset state when it re-enables effects.
        if (!enabled || frames == 0) {
            return true;
        }
        return processor_.process(output, frames, channels_);
    }

    void reset() noexcept { processor_.reset(); }
    std::size_t channels() const noexcept { return channels_; }

private:
    InterleavedFloatProcessor processor_;
    std::size_t channels_;
};

} // namespace subon::dsp
