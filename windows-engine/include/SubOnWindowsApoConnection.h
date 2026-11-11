#pragma once

// Windows-only adapter between Windows APO connection buffers and the
// platform-neutral SubON real-time DSP processor.
#include "SubOnApoRealtimeProcessor.h"

#include <Windows.h>
#include <mmreg.h>
#include <ks.h>
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

    const AudioFormat parsed{SampleFormat::Float32, waveFormat->nSamplesPerSec,
                             waveFormat->nChannels, waveFormat->nBlockAlign};
    if (validateFloat32Interleaved(parsed) != FormatStatus::Supported ||
        waveFormat->nAvgBytesPerSec != parsed.sampleRate * parsed.blockAlign) {
        return false;
    }
    result = parsed;
    return true;
}

// Normalize the conventional mono/stereo layouts when no explicit mask is
// supplied. Do not treat different surround speaker layouts as interchangeable.
inline DWORD channelMask(const WAVEFORMATEX* format) noexcept {
    if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const auto mask = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format)->dwChannelMask;
        if (mask != 0) return mask;
    }
    return format->nChannels == 1 ? 0x4u : (format->nChannels == 2 ? 0x3u : 0u);
}

inline bool matchingFloat32Formats(const WAVEFORMATEX* input,
                                   const WAVEFORMATEX* output) noexcept {
    AudioFormat in{}, out{};
    return describeFloat32Format(input, in) && describeFloat32Format(output, out) &&
           in.sampleRate == out.sampleRate && in.channels == out.channels &&
           channelMask(input) == channelMask(output);
}

// Processes one Windows APO connection-buffer pair. Format negotiation,
// processor construction and buffer allocation must happen before this call.
// This function itself performs no allocation, locking, logging or OS calls.
inline HRESULT processConnectionBuffer(
    RealtimeProcessor& processor,
    const AudioFormat& format,
    APO_CONNECTION_PROPERTY* input,
    APO_CONNECTION_PROPERTY* output,
    bool effectsEnabled,
    std::size_t maxFrames) noexcept {

    if (output == nullptr) return E_POINTER;
    // Snapshot input before clearing output, since in-place connections may
    // share the property object as well as their sample buffer.
    const UINT32 frames = input ? input->u32ValidFrameCount : 0;
    const auto flags = input ? input->u32BufferFlags : BUFFER_INVALID;
    const auto sourceBuffer = input ? input->pBuffer : 0;
    output->u32ValidFrameCount = 0;
    output->u32BufferFlags = BUFFER_INVALID;
    if (input == nullptr) return E_POINTER;
    if (!processor.isSupported() ||
        validateFloat32Interleaved(format) != FormatStatus::Supported) {
        return APOERR_FORMAT_NOT_SUPPORTED;
    }
    if (flags == BUFFER_INVALID) return S_OK;
    if (flags != BUFFER_VALID && flags != BUFFER_SILENT) return E_INVALIDARG;
    if (frames > maxFrames) return E_INVALIDARG;

    const auto* source = reinterpret_cast<const float*>(sourceBuffer);
    auto* destination = reinterpret_cast<float*>(output->pBuffer);
    const BufferState state = flags == BUFFER_SILENT ? BufferState::Silent : BufferState::Valid;
    if (!processor.process(source, destination, frames, format, state, effectsEnabled)) {
        return E_INVALIDARG;
    }

    output->u32ValidFrameCount = frames;
    // A silent input can produce a filter tail. Never mark that output silent
    // or the audio engine may discard the samples we just generated.
    bool silent = state == BufferState::Silent;
    if (silent && effectsEnabled) {
        const std::size_t samples = static_cast<std::size_t>(frames) * format.channels;
        for (std::size_t i = 0; i < samples; ++i) {
            if (destination[i] != 0.0f) { silent = false; break; }
        }
    }
    output->u32BufferFlags = silent ? BUFFER_SILENT : BUFFER_VALID;
    return S_OK;
}

} // namespace subon::apo::windows
