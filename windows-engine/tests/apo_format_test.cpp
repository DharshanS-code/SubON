#include "SubOnApoFormat.h"

#include <cstdlib>
#include <iostream>

#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << "Check failed at line " << __LINE__ << '\n'; return EXIT_FAILURE; \
} } while (false)

using subon::apo::AudioFormat;
using subon::apo::FormatStatus;
using subon::apo::SampleFormat;

int main() {
    const AudioFormat stereo48k{SampleFormat::Float32, 48000, 2, 8};
    CHECK(subon::apo::validateFloat32Interleaved(stereo48k) ==
           FormatStatus::Supported);

    const AudioFormat mono44k{SampleFormat::Float32, 44100, 1, 4};
    CHECK(subon::apo::validateFloat32Interleaved(mono44k) ==
           FormatStatus::Supported);

    const AudioFormat pcm16{SampleFormat::Pcm16, 48000, 2, 4};
    CHECK(subon::apo::validateFloat32Interleaved(pcm16) ==
           FormatStatus::UnsupportedSampleFormat);

    const AudioFormat badRate{SampleFormat::Float32, 1000, 2, 8};
    CHECK(subon::apo::validateFloat32Interleaved(badRate) ==
           FormatStatus::InvalidSampleRate);

    const AudioFormat tooManyChannels{SampleFormat::Float32, 48000, 9, 36};
    CHECK(subon::apo::validateFloat32Interleaved(tooManyChannels) ==
           FormatStatus::InvalidChannelCount);

    const AudioFormat badAlign{SampleFormat::Float32, 48000, 2, 4};
    CHECK(subon::apo::validateFloat32Interleaved(badAlign) ==
           FormatStatus::InvalidBlockAlign);

    const AudioFormat unknown{SampleFormat::Unknown, 48000, 2, 8};
    CHECK(subon::apo::validateFloat32Interleaved(unknown) ==
           FormatStatus::UnsupportedSampleFormat);
    for (std::uint32_t rate : {0u, 8000u, 32000u, 384001u}) {
        CHECK(subon::apo::validateFloat32Interleaved({SampleFormat::Float32, rate, 2, 8}) ==
              FormatStatus::InvalidSampleRate);
    }
    for (std::uint32_t rate : {32001u, 44100u, 384000u}) {
        CHECK(subon::apo::validateFloat32Interleaved({SampleFormat::Float32, rate, 8, 32}) ==
              FormatStatus::Supported);
    }
    CHECK(subon::apo::validateFloat32Interleaved({SampleFormat::Float32, 48000, 0, 0}) ==
          FormatStatus::InvalidChannelCount);
    return 0;
}
