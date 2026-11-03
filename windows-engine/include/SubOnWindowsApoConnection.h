#pragma once

// Windows-only adapter between Windows APO connection buffers and the
// platform-neutral SubON real-time DSP processor.
#include "SubOnApoRealtimeProcessor.h"

#include <Windows.h>
#include <mmreg.h>
#include <ks.h>
#include <initguid.h>
#include <audioenginebaseapo.h>

#include <cstddef>
#include <cstdint>

namespace subon::apo::windows {

inline bool isIeeeFloatSubFormat(const GUID& format) noexcept {
    // KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, expressed directly to avoid pulling
    // ksmedia.h's GUID declaration machinery into this adapter.
    return format.Data1 == 0x00000003 &&
           format.Data2 == 0x0000 &&
           format.Data3 == 0x0010 &&
           format.Data4[0] == 0x80 &&
           format.Data4[1] == 0x00 &&
           format.Data4[2] == 0x00 &&
           format.Data4[3] == 0xAA &&
           format.Data4[4] == 0x00 &&
           format.Data4[5] == 0x38 &&
           format.Data4[6] == 0x9B &&
           format.Data4[7] == 0x71;
}

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
        isFloat = isIeeeFloatSubFormat(extensible->SubFormat) &&
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
        output->pBuffer == 0) {
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
        if (input->pBuffer == 0) {
            return E_POINTER;
        }
        state = BufferState::Valid;
    } else {
        return E_INVALIDARG;
    }

    const auto* source = reinterpret_cast<const float*>(
        static_cast<std::uintptr_t>(input->pBuffer));
    auto* destination = reinterpret_cast<float*>(
        static_cast<std::uintptr_t>(output->pBuffer));

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
