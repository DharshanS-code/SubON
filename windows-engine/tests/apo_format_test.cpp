#include "SubOnApoFormat.h"

#include <cassert>

using subon::apo::AudioFormat;
using subon::apo::FormatStatus;
using subon::apo::SampleFormat;

int main() {
    const AudioFormat stereo48k{SampleFormat::Float32, 48000, 2, 8};
    assert(subon::apo::validateFloat32Interleaved(stereo48k) ==
           FormatStatus::Supported);

    const AudioFormat mono44k{SampleFormat::Float32, 44100, 1, 4};
    assert(subon::apo::validateFloat32Interleaved(mono44k) ==
           FormatStatus::Supported);

    const AudioFormat pcm16{SampleFormat::Pcm16, 48000, 2, 4};
    assert(subon::apo::validateFloat32Interleaved(pcm16) ==
           FormatStatus::UnsupportedSampleFormat);

    const AudioFormat badRate{SampleFormat::Float32, 1000, 2, 8};
    assert(subon::apo::validateFloat32Interleaved(badRate) ==
           FormatStatus::InvalidSampleRate);

    const AudioFormat tooManyChannels{SampleFormat::Float32, 48000, 9, 36};
    assert(subon::apo::validateFloat32Interleaved(tooManyChannels) ==
           FormatStatus::InvalidChannelCount);

    const AudioFormat badAlign{SampleFormat::Float32, 48000, 2, 4};
    assert(subon::apo::validateFloat32Interleaved(badAlign) ==
           FormatStatus::InvalidBlockAlign);

    const AudioFormat unknown{SampleFormat::Unknown, 48000, 2, 8};
    assert(subon::apo::validateFloat32Interleaved(unknown) ==
           FormatStatus::UnsupportedSampleFormat);
    return 0;
}
