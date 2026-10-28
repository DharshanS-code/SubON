#pragma once

#include <cstddef>
#include <cstdint>

namespace subon::apo {

// The first SubON APO integration will intentionally accept only interleaved
// IEEE float32 audio. Rejecting other formats is safer than interpreting an
// unsupported buffer incorrectly. The Windows APO adapter must convert or
// negotiate formats before calling the DSP bridge.
enum class SampleFormat : std::uint8_t {
    Float32,
    Pcm16,
    Pcm24,
    Pcm32,
    Unknown
};

struct AudioFormat {
    SampleFormat sampleFormat = SampleFormat::Unknown;
    std::uint32_t sampleRate = 0;
    std::uint16_t channels = 0;
    std::uint16_t blockAlign = 0;
};

enum class FormatStatus : std::uint8_t {
    Supported,
    UnsupportedSampleFormat,
    InvalidSampleRate,
    InvalidChannelCount,
    InvalidBlockAlign
};

inline FormatStatus validateFloat32Interleaved(const AudioFormat& format) noexcept {
    if (format.sampleFormat != SampleFormat::Float32) {
        return FormatStatus::UnsupportedSampleFormat;
    }
    if (format.sampleRate < 8000 || format.sampleRate > 384000) {
        return FormatStatus::InvalidSampleRate;
    }
    if (format.channels == 0 || format.channels > 8) {
        return FormatStatus::InvalidChannelCount;
    }
    const auto expectedAlign =
        static_cast<std::uint32_t>(format.channels) * sizeof(float);
    if (format.blockAlign != expectedAlign) {
        return FormatStatus::InvalidBlockAlign;
    }
    return FormatStatus::Supported;
}

} // namespace subon::apo
