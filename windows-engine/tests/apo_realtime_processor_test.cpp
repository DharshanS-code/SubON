#include "SubOnApoRealtimeProcessor.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <cmath>

namespace {
bool fail(const char* message) {
    std::cerr << message << '\n';
    return false;
}
}

int main() {
    using namespace subon::apo;
    using namespace subon::dsp;

    ProcessorConfig config;
    config.preampDb = 6.0;
    config.limiterEnabled = false;

    const AudioFormat stereo{SampleFormat::Float32, 48000, 2, 8};
    RealtimeProcessor processor(config, stereo);
    if (!processor.isSupported()) return fail("Valid stereo format rejected") ? 0 : EXIT_FAILURE;

    // Silent connection buffers must produce silence without reading input.
    float silence[4] = {1.0f, -1.0f, 0.5f, -0.5f};
    if (!processor.process(nullptr, silence, 2, stereo, BufferState::Silent, true))
        return fail("Silent buffer rejected"), EXIT_FAILURE;
    for (float sample : silence) {
        if (sample != 0.0f) return fail("Silent buffer was not zeroed"), EXIT_FAILURE;
    }

    // Disabled effects are a transparent copy.
    const float input[4] = {0.1f, -0.2f, 0.3f, -0.4f};
    float output[4] = {};
    if (!processor.process(input, output, 2, stereo, BufferState::Valid, false))
        return fail("Valid buffer rejected"), EXIT_FAILURE;
    for (int i = 0; i < 4; ++i) {
        if (output[i] != input[i]) return fail("Bypass was not bit-transparent"), EXIT_FAILURE;
    }

    // Unsupported runtime format and invalid connection state must not mutate output.
    float guarded[2] = {0.125f, -0.25f};
    const AudioFormat wrongRate{SampleFormat::Float32, 44100, 2, 8};
    if (processor.process(input, guarded, 1, wrongRate, BufferState::Valid, true))
        return fail("Mismatched runtime format accepted"), EXIT_FAILURE;
    if (guarded[0] != 0.125f || guarded[1] != -0.25f)
        return fail("Rejected buffer was modified"), EXIT_FAILURE;
    if (processor.process(input, guarded, 1, stereo, BufferState::Invalid, true))
        return fail("Invalid buffer state accepted"), EXIT_FAILURE;

    // PCM input is rejected at construction, before any callback can run.
    const AudioFormat pcm{SampleFormat::Pcm16, 48000, 2, 4};
    RealtimeProcessor unsupported(config, pcm);
    if (unsupported.isSupported()) return fail("PCM format should be rejected"), EXIT_FAILURE;

    for (std::uint32_t rate : {8000u, 16000u, 32000u, 384001u}) {
        AudioFormat invalid{SampleFormat::Float32, rate, 2, 8};
        auto invalidConfig = config;
        invalidConfig.sampleRate = rate;
        RealtimeProcessor rejected(invalidConfig, invalid);
        if (rejected.isSupported()) return fail("Unsupported rate accepted"), EXIT_FAILURE;
    }
    for (std::uint16_t channels : {0, 9}) {
        AudioFormat invalid{SampleFormat::Float32, 48000, channels,
                            static_cast<std::uint16_t>(channels * 4)};
        RealtimeProcessor rejected(config, invalid);
        if (rejected.isSupported()) return fail("Unsupported channels accepted"), EXIT_FAILURE;
    }
    auto mismatchedConfig = config;
    mismatchedConfig.sampleRate = 44100;
    RealtimeProcessor mismatch(mismatchedConfig, stereo);
    if (mismatch.isSupported()) return fail("DSP and stream sample rates can disagree"), EXIT_FAILURE;
    if (processor.process(input, guarded, 1, stereo, static_cast<BufferState>(99), true) ||
        processor.process(input, guarded,
            (std::numeric_limits<std::size_t>::max)() / sizeof(float) / 2 + 1,
            stereo, BufferState::Silent, true) ||
        guarded[0] != 0.125f || guarded[1] != -0.25f)
        return fail("Invalid state or byte overflow modified output"), EXIT_FAILURE;

    // Silent packets advance exactly the same DSP history as valid zero input.
    config.bassDb = 12.0;
    RealtimeProcessor silentTail(config, stereo), explicitZeros(config, stereo);
    const float impulse[] = {0.8f, 0.0f};
    float scratchA[2], scratchB[2];
    silentTail.process(impulse, scratchA, 1, stereo, BufferState::Valid, true);
    explicitZeros.process(impulse, scratchB, 1, stereo, BufferState::Valid, true);
    float actual[64], expected[64];
    const float zeros[64]{};
    silentTail.process(nullptr, actual, 32, stereo, BufferState::Silent, true);
    explicitZeros.process(zeros, expected, 32, stereo, BufferState::Valid, true);
    bool foundTail = false;
    for (int i = 0; i < 64; ++i) {
        if (actual[i] != expected[i]) return fail("Silent packet froze filter state"), EXIT_FAILURE;
        foundTail = foundTail || actual[i] != 0.0f;
    }
    if (!foundTail) return fail("Filter-tail regression did not exercise state"), EXIT_FAILURE;

    for (std::uint32_t rate : {32001u, 44100u, 48000u, 96000u, 384000u}) {
        auto supportedConfig = config;
        supportedConfig.sampleRate = rate;
        const AudioFormat valid{SampleFormat::Float32, rate, 8, 32};
        RealtimeProcessor accepted(supportedConfig, valid);
        if (!accepted.isSupported()) return fail("Negotiated rate is not constructible"), EXIT_FAILURE;
    }
    std::cout << "SubON APO real-time processor tests passed\n";
    return EXIT_SUCCESS;
}
