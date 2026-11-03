#include "SubOnApoRealtimeProcessor.h"

#include <cstdlib>
#include <iostream>

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

    std::cout << "SubON APO real-time processor tests passed\n";
    return EXIT_SUCCESS;
}
