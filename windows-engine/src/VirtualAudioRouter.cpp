#include "SubOnWindowsApoConnection.h"

#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {
constexpr wchar_t kVirtualEndpointHint[] = L"SubON Virtual Audio";
volatile LONG stopRequested = 0;

BOOL WINAPI onConsoleControl(DWORD event) {
    if (event != CTRL_C_EVENT && event != CTRL_BREAK_EVENT) return FALSE;
    InterlockedExchange(&stopRequested, 1);
    return TRUE;
}

struct ConsoleHandler {
    ~ConsoleHandler() { SetConsoleCtrlHandler(onConsoleControl, FALSE); }
};
struct ComApartment {
    ~ComApartment() { CoUninitialize(); }
};
struct TaskMemoryDeleter {
    void operator()(void* memory) const noexcept { CoTaskMemFree(memory); }
};
using WaveFormatPtr = std::unique_ptr<WAVEFORMATEX, TaskMemoryDeleter>;

// Created after the COM clients, so streams stop before those clients release.
struct RunningStreams {
    IAudioClient* capture;
    IAudioClient* render;
    bool captureStarted = false;
    bool renderStarted = false;
    ~RunningStreams() {
        if (captureStarted) capture->Stop();
        if (renderStarted) render->Stop();
    }
};

int fail(const wchar_t* message, HRESULT hr) {
    std::wcerr << message << L" (HRESULT 0x" << std::hex
               << static_cast<unsigned long>(hr) << L")\n";
    return 1;
}

HRESULT friendlyName(IMMDevice* device, std::wstring& name) {
    ComPtr<IPropertyStore> props;
    HRESULT hr = device->OpenPropertyStore(STGM_READ, &props);
    if (FAILED(hr)) return hr;
    PROPVARIANT value;
    PropVariantInit(&value);
    hr = props->GetValue(PKEY_Device_FriendlyName, &value);
    try {
        if (SUCCEEDED(hr) && value.vt == VT_LPWSTR && value.pwszVal)
            name = value.pwszVal;
    } catch (...) {
        PropVariantClear(&value);
        throw;
    }
    PropVariantClear(&value);
    return hr;
}

HRESULT findVirtualRenderEndpoint(IMMDeviceEnumerator* enumerator, ComPtr<IMMDevice>& result) {
    ComPtr<IMMDeviceCollection> devices;
    HRESULT hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices);
    if (FAILED(hr)) return hr;
    UINT count = 0;
    hr = devices->GetCount(&count);
    if (FAILED(hr)) return hr;
    for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> device;
        hr = devices->Item(i, &device);
        if (FAILED(hr)) return hr;
        std::wstring name;
        hr = friendlyName(device.Get(), name);
        if (FAILED(hr)) return hr;
        if (name.find(kVirtualEndpointHint) != std::wstring::npos) {
            if (result) return HRESULT_FROM_WIN32(ERROR_DUP_NAME);
            result = device;
        }
    }
    return result ? S_OK : S_FALSE;
}

int runRouter() {
    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&enumerator));
    if (FAILED(hr)) return fail(L"Cannot create audio enumerator", hr);

    ComPtr<IMMDevice> virtualEndpoint;
    hr = findVirtualRenderEndpoint(enumerator.Get(), virtualEndpoint);
    if (FAILED(hr)) return fail(L"Cannot uniquely identify the virtual endpoint", hr);
    if (!virtualEndpoint) {
        std::wcerr << L"SubON virtual render endpoint was not found. A compatible virtual audio driver "
                      L"must create an active playback endpoint named 'SubON Virtual Audio'.\n";
        return 2;
    }

    ComPtr<IMMDevice> physicalEndpoint;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &physicalEndpoint);
    if (FAILED(hr)) return fail(L"Cannot open default physical output", hr);
    wchar_t* rawId = nullptr;
    hr = virtualEndpoint->GetId(&rawId);
    std::unique_ptr<wchar_t, TaskMemoryDeleter> virtualId(rawId);
    if (FAILED(hr)) return fail(L"Cannot identify virtual endpoint", hr);
    rawId = nullptr;
    hr = physicalEndpoint->GetId(&rawId);
    std::unique_ptr<wchar_t, TaskMemoryDeleter> physicalId(rawId);
    if (FAILED(hr)) return fail(L"Cannot identify physical output", hr);
    if (!virtualId || !physicalId || std::wcscmp(virtualId.get(), physicalId.get()) == 0) {
        std::wcerr << L"The default output must be a different physical device, not SubON Virtual Audio. "
                      L"Select that output as default and route source apps to the virtual device.\n";
        return 4;
    }

    ComPtr<IAudioClient> captureClient;
    hr = virtualEndpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                   reinterpret_cast<void**>(captureClient.GetAddressOf()));
    if (FAILED(hr)) return fail(L"Cannot activate virtual endpoint", hr);
    WAVEFORMATEX* rawFormat = nullptr;
    hr = captureClient->GetMixFormat(&rawFormat);
    WaveFormatPtr captureFormat(rawFormat);
    if (FAILED(hr)) return fail(L"Cannot read virtual endpoint format", hr);
    subon::apo::AudioFormat format{};
    if (!subon::apo::windows::describeFloat32Format(captureFormat.get(), format)) {
        std::wcerr << L"Unsupported format: float32, 1-8 channels and a sample rate above 32 kHz "
                      L"through 384 kHz are required by the ten-band DSP.\n";
        return 3;
    }

    ComPtr<IAudioClient> renderClient;
    hr = physicalEndpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                    reinterpret_cast<void**>(renderClient.GetAddressOf()));
    if (FAILED(hr)) return fail(L"Cannot activate physical output", hr);
    rawFormat = nullptr;
    hr = renderClient->GetMixFormat(&rawFormat);
    WaveFormatPtr renderMix(rawFormat);
    if (FAILED(hr)) return fail(L"Cannot read physical output format", hr);
    if (!subon::apo::windows::matchingFloat32Formats(captureFormat.get(), renderMix.get())) {
        std::wcerr << L"Virtual and physical endpoint formats or channel layouts differ; "
                      L"format conversion is not implemented.\n";
        return 4;
    }

    constexpr REFERENCE_TIME bufferDuration = 200000;
    hr = captureClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK, bufferDuration, 0, captureFormat.get(), nullptr);
    if (FAILED(hr)) return fail(L"Cannot initialize loopback capture", hr);
    hr = renderClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, bufferDuration, 0, renderMix.get(), nullptr);
    if (FAILED(hr)) return fail(L"Cannot initialize physical output", hr);

    ComPtr<IAudioCaptureClient> capture;
    hr = captureClient->GetService(IID_PPV_ARGS(&capture));
    if (FAILED(hr)) return fail(L"Cannot get capture service", hr);
    ComPtr<IAudioRenderClient> render;
    hr = renderClient->GetService(IID_PPV_ARGS(&render));
    if (FAILED(hr)) return fail(L"Cannot get render service", hr);
    UINT32 renderBufferFrames = 0;
    hr = renderClient->GetBufferSize(&renderBufferFrames);
    if (FAILED(hr) || renderBufferFrames == 0) return fail(L"Cannot read render buffer size", FAILED(hr) ? hr : E_UNEXPECTED);

    subon::dsp::ProcessorConfig config;
    config.sampleRate = format.sampleRate;
    subon::apo::RealtimeProcessor dsp(config, format);
    // A bounded one-second FIFO preserves packet tails when render is full.
    // This prototype does not compensate independent device clocks; overruns
    // are reported rather than silently discarding captured audio.
    const std::size_t capacity = format.sampleRate;
    const std::size_t channels = format.channels;
    std::vector<float> fifo(capacity * channels);
    std::size_t readFrame = 0, queuedFrames = 0;

    hr = renderClient->Start();
    if (FAILED(hr)) return fail(L"Cannot start output stream", hr);
    RunningStreams streams{captureClient.Get(), renderClient.Get(), false, true};
    hr = captureClient->Start();
    if (FAILED(hr)) return fail(L"Cannot start capture stream", hr);
    streams.captureStarted = true;
    if (!SetConsoleCtrlHandler(onConsoleControl, TRUE))
        return fail(L"Cannot install stop handler", HRESULT_FROM_WIN32(GetLastError()));
    ConsoleHandler handler;

    std::wcout << L"SubON virtual-audio router running. Press Ctrl+C to stop. "
                  L"Device-clock drift compensation and live effect controls are not implemented.\n";
    bool receivedPacket = false;
    while (InterlockedCompareExchange(&stopRequested, 0, 0) == 0) {
        UINT32 packetFrames = 0;
        hr = capture->GetNextPacketSize(&packetFrames);
        if (FAILED(hr)) return fail(L"Cannot query capture packet", hr);
        if (packetFrames > 0) {
            BYTE* inputBytes = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;
            hr = capture->GetBuffer(&inputBytes, &frames, &flags, nullptr, nullptr);
            if (FAILED(hr)) return fail(L"Cannot acquire capture packet", hr);
            HRESULT processing = S_OK;
            if (frames > capacity - queuedFrames) {
                processing = HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
            } else if (receivedPacket && (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)) {
                processing = HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
            } else {
                const auto state = (flags & AUDCLNT_BUFFERFLAGS_SILENT)
                    ? subon::apo::BufferState::Silent : subon::apo::BufferState::Valid;
                const auto* input = reinterpret_cast<const float*>(inputBytes);
                const std::size_t writeFrame = (readFrame + queuedFrames) % capacity;
                const std::size_t first = (std::min)(static_cast<std::size_t>(frames), capacity - writeFrame);
                if (!dsp.process(input, fifo.data() + writeFrame * channels, first, format, state, true) ||
                    (frames > first && !dsp.process(input ? input + first * channels : nullptr,
                        fifo.data(), frames - first, format, state, true))) {
                    processing = E_INVALIDARG;
                } else {
                    queuedFrames += frames;
                }
            }
            // Release every successfully acquired packet, including error paths.
            hr = capture->ReleaseBuffer(frames);
            if (FAILED(hr)) return fail(L"Cannot release capture packet", hr);
            if (FAILED(processing)) return fail(L"Capture discontinuity, FIFO overrun, or invalid packet", processing);
            receivedPacket = true;
        }

        UINT32 padding = 0;
        hr = renderClient->GetCurrentPadding(&padding);
        if (FAILED(hr)) return fail(L"Cannot read render padding", hr);
        if (padding > renderBufferFrames) return fail(L"Invalid render padding", E_UNEXPECTED);
        const auto count = static_cast<UINT32>((std::min)(queuedFrames,
            static_cast<std::size_t>(renderBufferFrames - padding)));
        if (count > 0) {
            BYTE* outputBytes = nullptr;
            hr = render->GetBuffer(count, &outputBytes);
            if (FAILED(hr)) return fail(L"Cannot acquire render buffer", hr);
            if (!outputBytes) {
                render->ReleaseBuffer(count, AUDCLNT_BUFFERFLAGS_SILENT);
                return fail(L"Render returned an invalid buffer", E_POINTER);
            }
            const std::size_t first = (std::min)(static_cast<std::size_t>(count), capacity - readFrame);
            std::memcpy(outputBytes, fifo.data() + readFrame * channels, first * format.blockAlign);
            if (count > first)
                std::memcpy(outputBytes + first * format.blockAlign, fifo.data(), (count - first) * format.blockAlign);
            hr = render->ReleaseBuffer(count, 0);
            if (FAILED(hr)) return fail(L"Cannot release render buffer", hr);
            readFrame = (readFrame + count) % capacity;
            queuedFrames -= count;
        }
        // Avoid busy spinning while waiting for either endpoint.
        if (packetFrames == 0 || queuedFrames == capacity) Sleep(2);
    }
    hr = captureClient->Stop();
    streams.captureStarted = FAILED(hr);
    if (FAILED(hr)) return fail(L"Cannot stop capture stream", hr);
    hr = renderClient->Stop();
    streams.renderStarted = FAILED(hr);
    return FAILED(hr) ? fail(L"Cannot stop output stream", hr) : 0;
}
} // namespace

int wmain() {
    const HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(init)) return fail(L"COM initialization failed", init);
    ComApartment apartment; // COM outlives every endpoint/client, on every exit path.
    try {
        return runRouter();
    } catch (const std::exception& error) {
        std::cerr << "Audio router failed: " << error.what() << '\n';
        return 1;
    }
}
