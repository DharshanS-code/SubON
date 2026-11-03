#pragma once

// Windows-only adapter between the Windows Audio Processing Object (APO)
// connection buffers and the platform-neutral SubON real-time DSP processor.
// Include this header only in a WDK/Windows SDK APO target.
#include "SubOnApoRealtimeProcessor.h"

#include <Windows.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>
#include <audioenginebaseapo.h>

#include <cstddef>
#include <cstdint>

namespace subon::apo::windows {

inline bool describeFloat32Format(const WAVEFORMATEX* waveFormat,
                                  AudioFormat& result) noexcept {
    if (waveFormat == nullptr || waveFormat->wBitsPerSample != 32 ||
        waveFormat->nChannels == 0 || waveFormat->nChannels > 8 ||
        waveFormat->nBlockAlign != waveFormat->nChannels * sizeof(float)) {
        return false;
    }

    bool isFloat = waveFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
    if (waveFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        if (waveFormat->cbSize < sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) {
            return false;
        }
        const auto* extensible =
            reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(waveFormat);
        isFloat = extensible->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT &&
                  extensible->Samples.wValidBitsPerSample == 32;
    }

    if (!isFloat) {
        return false;
    }

    result.sampleFormat = SampleFormat::Float32;
    result.sampleRate = waveFormat->nSamplesPerSec;
    result.channels = waveFormat->nChannels;
    result.blockAlign = waveFormat->nBlockAlign;
    return validateFloat32Interleaved(result) == FormatStatus::Supported;
}

// Processes one Windows APO connection-buffer pair. Format negotiation,
// processor construction and buffer allocation must happen before this call.
// This function itself performs no allocation, locking, logging or OS calls.
inline HRESULT processConnectionBuffer(
    RealtimeProcessor& processor,
    const WAVEFORMATEX* negotiatedFormat,
    APO_CONNECTION_PROPERTY* input,
    APO_CONNECTION_PROPERTY* output,
    bool effectsEnabled) noexcept {

    if (negotiatedFormat == nullptr || input == nullptr || output == nullptr ||
        output->pBuffer == nullptr) {
        return E_POINTER;
    }

    AudioFormat format{};
    if (!describeFloat32Format(negotiatedFormat, format) ||
        !processor.isSupported()) {
        return APOERR_FORMAT_NOT_SUPPORTED;
    }

    if (input->u32ValidFrameCount != output->u32ValidFrameCount) {
        return E_INVALIDARG;
    }

    const auto frames = static_cast<std::size_t>(input->u32ValidFrameCount);
    BufferState state = BufferState::Invalid;
    if (input->u32BufferFlags == BUFFER_SILENT) {
        state = BufferState::Silent;
    } else if (input->u32BufferFlags == BUFFER_VALID) {
        state = BufferState::Valid;
    } else {
        return E_INVALIDARG;
    }

    const auto* source = reinterpret_cast<const float*>(input->pBuffer);
    auto* destination = reinterpret_cast<float*>(output->pBuffer);

    if (!processor.process(source, destination, frames, format, state,
                           effectsEnabled)) {
        return E_INVALIDARG;
    }

    output->u32ValidFrameCount = input->u32ValidFrameCount;
    output->u32BufferFlags = state == BufferState::Silent
        ? BUFFER_SILENT
        : BUFFER_VALID;
    return S_OK;
}

} // namespace subon::apo::windows
