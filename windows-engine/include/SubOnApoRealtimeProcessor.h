#pragma once

#include "SubOnApoDspBridge.h"
#include "SubOnApoFormat.h"

#include <cstddef>
#include <cstdint>

namespace subon::apo {

// Mirrors the Windows APO connection-buffer states without importing Windows
// headers, so the real-time adapter can be tested on every CI platform.
enum class BufferState : std::uint8_t {
    Valid,
    Silent,
    Invalid
};

class RealtimeProcessor final {
public:
    RealtimeProcessor(const subon::dsp::ProcessorConfig& config,
                      const AudioFormat& format)
        : format_(format),
          bridge_(config, format.channels),
          supported_(validateFloat32Interleaved(format) == FormatStatus::Supported) {}

    RealtimeProcessor(const RealtimeProcessor&) = delete;
    RealtimeProcessor& operator=(const RealtimeProcessor&) = delete;

    bool isSupported() const noexcept { return supported_; }

    // The caller owns both buffers. This method performs no allocation,
    // locking, logging, waiting, or OS calls and is suitable for an APOProcess
    // adapter after format negotiation and construction have completed.
    bool process(const float* input, float* output, std::size_t frames,
                 const AudioFormat& runtimeFormat, BufferState state,
                 bool effectsEnabled) noexcept {
        if (!supported_ || !sameFormat(runtimeFormat) ||
            state == BufferState::Invalid) {
            return false;
        }

        if (frames == 0) {
            return true;
        }

        const std::size_t channels = format_.channels;
        if (frames > static_cast<std::size_t>(-1) / channels || output == nullptr) {
            return false;
        }

        if (state == BufferState::Silent) {
            for (std::size_t i = 0; i < frames * channels; ++i) {
                output[i] = 0.0f;
            }
            return true;
        }

        if (input == nullptr) {
            return false;
        }
        return bridge_.process(input, output, frames, channels, effectsEnabled);
    }

    void reset() noexcept { bridge_.reset(); }

private:
    bool sameFormat(const AudioFormat& other) const noexcept {
        return other.sampleFormat == format_.sampleFormat &&
               other.sampleRate == format_.sampleRate &&
               other.channels == format_.channels &&
               other.blockAlign == format_.blockAlign;
    }

    AudioFormat format_;
    subon::dsp::ApoDspBridge bridge_;
    bool supported_;
};

} // namespace subon::apo
