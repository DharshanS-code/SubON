#include "SubOnApoDspBridge.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {
constexpr wchar_t kVirtualEndpointHint[] = L"SubON Virtual Audio";

std::wstring friendlyName(IMMDevice* device) {
    ComPtr<IPropertyStore> props;
    if (FAILED(device->OpenPropertyStore(STGM_READ, &props))) return {};
    PROPVARIANT value;
    PropVariantInit(&value);
    std::wstring result;
    if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &value)) &&
        value.vt == VT_LPWSTR && value.pwszVal) result = value.pwszVal;
    PropVariantClear(&value);
    return result;
}

ComPtr<IMMDevice> findVirtualRenderEndpoint(IMMDeviceEnumerator* enumerator) {
    ComPtr<IMMDeviceCollection> devices;
    if (FAILED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices))) return {};
    UINT count = 0;
    devices->GetCount(&count);
    for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> device;
        if (SUCCEEDED(devices->Item(i, &device)) &&
            friendlyName(device.Get()).find(kVirtualEndpointHint) != std::wstring::npos) {
            return device;
        }
    }
    return {};
}

bool isFloat32(const WAVEFORMATEX* format) {
    if (!format) return false;
    if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT && format->wBitsPerSample == 32) return true;
    if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
        format->wBitsPerSample == 32 &&
        format->cbSize >= sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) {
        const auto* extended = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
        return IsEqualGUID(extended->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
    }
    return false;
}

int fail(const wchar_t* message, HRESULT hr) {
    std::wcerr << message << L" (HRESULT 0x" << std::hex
               << static_cast<unsigned long>(hr) << L")\n";
    return 1;
}
}

int wmain() {
    const HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(init)) return fail(L"COM initialization failed", init);

    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&enumerator));
    if (FAILED(hr)) { CoUninitialize(); return fail(L"Cannot create audio enumerator", hr); }

    auto virtualEndpoint = findVirtualRenderEndpoint(enumerator.Get());
    if (!virtualEndpoint) {
        std::wcerr << L"SubON virtual render endpoint was not found. Install a supported "
                      L"virtual audio driver and name its playback endpoint 'SubON Virtual Audio'.\n";
        CoUninitialize();
        return 2;
    }

    ComPtr<IMMDevice> physicalEndpoint;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &physicalEndpoint);
    if (FAILED(hr)) { CoUninitialize(); return fail(L"Cannot open default physical output", hr); }

    ComPtr<IAudioClient> captureClient;
    hr = virtualEndpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                   reinterpret_cast<void**>(captureClient.GetAddressOf()));
    if (FAILED(hr)) { CoUninitialize(); return fail(L"Cannot activate virtual endpoint", hr); }

    WAVEFORMATEX* captureFormat = nullptr;
    hr = captureClient->GetMixFormat(&captureFormat);
    if (FAILED(hr)) { CoUninitialize(); return fail(L"Cannot read virtual endpoint format", hr); }
    if (!isFloat32(captureFormat) || captureFormat->nChannels == 0 ||
        captureFormat->nChannels > 8 || captureFormat->nSamplesPerSec < 8000 ||
        captureFormat->nSamplesPerSec > 384000 ||
        captureFormat->nBlockAlign != captureFormat->nChannels * sizeof(float)) {
        CoTaskMemFree(captureFormat);
        CoUninitialize();
        std::wcerr << L"Unsupported endpoint mix format. This first router accepts interleaved float32 only.\n";
        return 3;
    }

    ComPtr<IAudioClient> renderClient;
    hr = physicalEndpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                    reinterpret_cast<void**>(renderClient.GetAddressOf()));
    if (FAILED(hr)) { CoTaskMemFree(captureFormat); CoUninitialize(); return fail(L"Cannot activate physical output", hr); }

    WAVEFORMATEX* renderMix = nullptr;
    hr = renderClient->GetMixFormat(&renderMix);
    if (FAILED(hr)) { CoTaskMemFree(captureFormat); CoUninitialize(); return fail(L"Cannot read physical output format", hr); }
    if (!isFloat32(renderMix) || renderMix->nChannels != captureFormat->nChannels ||
        renderMix->nSamplesPerSec != captureFormat->nSamplesPerSec ||
        renderMix->nBlockAlign != captureFormat->nBlockAlign) {
        CoTaskMemFree(renderMix);
        CoTaskMemFree(captureFormat);
        CoUninitialize();
        std::wcerr << L"Virtual and physical endpoint formats differ. Format conversion is not yet implemented.\n";
        return 4;
    }

    const REFERENCE_TIME bufferDuration = 200000; // 20 ms
    hr = captureClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK, bufferDuration, 0, captureFormat, nullptr);
    if (FAILED(hr)) { CoTaskMemFree(renderMix); CoTaskMemFree(captureFormat); CoUninitialize(); return fail(L"Cannot initialize loopback capture", hr); }
    hr = renderClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, bufferDuration, 0, renderMix, nullptr);
    if (FAILED(hr)) { CoTaskMemFree(renderMix); CoTaskMemFree(captureFormat); CoUninitialize(); return fail(L"Cannot initialize physical output", hr); }

    ComPtr<IAudioCaptureClient> capture;
    hr = captureClient->GetService(IID_PPV_ARGS(&capture));
    if (FAILED(hr)) { CoTaskMemFree(renderMix); CoTaskMemFree(captureFormat); CoUninitialize(); return fail(L"Cannot get capture service", hr); }
    ComPtr<IAudioRenderClient> render;
    hr = renderClient->GetService(IID_PPV_ARGS(&render));
    if (FAILED(hr)) { CoTaskMemFree(renderMix); CoTaskMemFree(captureFormat); CoUninitialize(); return fail(L"Cannot get render service", hr); }

    subon::dsp::ProcessorConfig config;
    config.sampleRate = captureFormat->nSamplesPerSec;
    subon::dsp::ApoDspBridge dsp(config, captureFormat->nChannels);

    hr = renderClient->Start();
    if (SUCCEEDED(hr)) hr = captureClient->Start();
    if (FAILED(hr)) {
        renderClient->Stop(); CoTaskMemFree(renderMix); CoTaskMemFree(captureFormat);
        CoUninitialize(); return fail(L"Cannot start audio streams", hr);
    }

    std::wcout << L"SubON virtual-audio router running. Press Ctrl+C to stop.\n";
    while (true) {
        UINT32 packetFrames = 0;
        hr = capture->GetNextPacketSize(&packetFrames);
        if (FAILED(hr)) break;
        if (packetFrames == 0) { Sleep(2); continue; }

        BYTE* inputBytes = nullptr;
        UINT32 frames = 0;
        DWORD flags = 0;
        hr = capture->GetBuffer(&inputBytes, &frames, &flags, nullptr, nullptr);
        if (FAILED(hr)) break;
        UINT32 padding = 0;
        hr = renderClient->GetCurrentPadding(&padding);
        if (FAILED(hr)) { capture->ReleaseBuffer(frames); break; }
        const UINT32 renderCapacity = renderMix->nSamplesPerSec / 20;
        const UINT32 writable = renderCapacity > padding ? renderCapacity - padding : 0;
        const UINT32 count = std::min(frames, writable);
        if (count > 0) {
            BYTE* outputBytes = nullptr;
            hr = render->GetBuffer(count, &outputBytes);
            if (FAILED(hr)) { capture->ReleaseBuffer(frames); break; }
            auto* output = reinterpret_cast<float*>(outputBytes);
            const auto* input = reinterpret_cast<const float*>(inputBytes);
            if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0) {
                std::fill(output, output + count * captureFormat->nChannels, 0.0f);
            } else {
                dsp.process(input, output, count, captureFormat->nChannels, true);
            }
            render->ReleaseBuffer(count, 0);
        }
        capture->ReleaseBuffer(frames);
    }

    captureClient->Stop();
    renderClient->Stop();
    CoTaskMemFree(renderMix);
    CoTaskMemFree(captureFormat);
    CoUninitialize();
    return FAILED(hr) ? fail(L"Audio routing stopped after an error", hr) : 0;
}
