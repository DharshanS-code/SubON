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
          supported_(validateFloat32Interleaved(format) == FormatStatus::Supported &&
                     subon::dsp::isValidProcessorConfig(config) &&
                     config.sampleRate == format.sampleRate),
          bridge_(supported_ ? config : subon::dsp::ProcessorConfig{},
                  supported_ ? format.channels : 1) {}

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
            (state != BufferState::Valid && state != BufferState::Silent)) {
            return false;
        }

        if (frames == 0) {
            return true;
        }

        const std::size_t channels = format_.channels;
        if (frames > (std::numeric_limits<std::size_t>::max)() / sizeof(float) / channels ||
            output == nullptr) {
            return false;
        }

        if (state == BufferState::Silent) {
            for (std::size_t i = 0; i < frames * channels; ++i) {
                output[i] = 0.0f;
            }
            // Silence is zero-valued input, not frozen time. Run it through
            // the filters so tails and limiter release advance normally.
            return bridge_.process(output, output, frames, channels, effectsEnabled);
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
    bool supported_;
    subon::dsp::ApoDspBridge bridge_;
};

} // namespace subon::apo
