#include "SubOnWindowsApoConnection.h"

#include <cstdlib>
#include <iostream>
#include <limits>

STDAPI DllGetClassObject(REFCLSID, REFIID, void**);
STDAPI DllCanUnloadNow();

#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << "Check failed at line " << __LINE__ << '\n'; return EXIT_FAILURE; \
} } while (false)

namespace {
constexpr CLSID subonClsid =
{0x8d4b1e62, 0x4f3a, 0x4f2d, {0x9a,0x61,0x72,0x31,0x5b,0x0c,0x8e,0x44}};
}

int main() {
    using namespace subon::apo;
    WAVEFORMATEX wave{};
    wave.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    wave.nChannels = 2;
    wave.nSamplesPerSec = 48000;
    wave.nAvgBytesPerSec = 384000;
    wave.nBlockAlign = 8;
    wave.wBitsPerSample = 32;
    AudioFormat format{};
    CHECK(windows::describeFloat32Format(&wave, format));
    wave.nAvgBytesPerSec = 1;
    CHECK(!windows::describeFloat32Format(&wave, format));
    CHECK(format.sampleRate == 48000);
    wave.nAvgBytesPerSec = 384000;
    wave.nSamplesPerSec = 32000;
    CHECK(!windows::describeFloat32Format(&wave, format));

    subon::dsp::ProcessorConfig config;
    config.bassDb = 12;
    config.limiterEnabled = false;
    RealtimeProcessor processor(config, format);
    float source[] = {0.8f, 0.0f};
    float destination[] = {9.0f, 9.0f};
    APO_CONNECTION_PROPERTY input{}, output{};
    input.pBuffer = reinterpret_cast<UINT_PTR>(source);
    input.u32ValidFrameCount = 1;
    input.u32BufferFlags = BUFFER_VALID;
    output.pBuffer = reinterpret_cast<UINT_PTR>(destination);
    output.u32ValidFrameCount = 0; // Output count is written by the APO, not the host.
    CHECK(SUCCEEDED(windows::processConnectionBuffer(processor, format, &input, &output, true, 4)));
    CHECK(output.u32ValidFrameCount == 1 && output.u32BufferFlags == BUFFER_VALID);
    input.pBuffer = 0;
    input.u32BufferFlags = BUFFER_SILENT;
    CHECK(SUCCEEDED(windows::processConnectionBuffer(processor, format, &input, &output, true, 4)));
    CHECK(destination[0] != 0.0f && output.u32BufferFlags == BUFFER_VALID);
    processor.reset();
    CHECK(SUCCEEDED(windows::processConnectionBuffer(processor, format, &input, &output, true, 4)));
    CHECK(destination[0] == 0.0f && output.u32BufferFlags == BUFFER_SILENT);
    input.u32ValidFrameCount = 5;
    destination[0] = 7.0f;
    CHECK(FAILED(windows::processConnectionBuffer(processor, format, &input, &output, true, 4)));
    CHECK(output.u32ValidFrameCount == 0 && output.u32BufferFlags == BUFFER_INVALID && destination[0] == 7.0f);
    input.u32ValidFrameCount = 1;
    input.u32BufferFlags = BUFFER_INVALID;
    CHECK(SUCCEEDED(windows::processConnectionBuffer(processor, format, &input, &output, true, 4)));
    CHECK(output.u32ValidFrameCount == 0 && output.u32BufferFlags == BUFFER_INVALID);
    input.u32BufferFlags = BUFFER_VALID;
    input.pBuffer = reinterpret_cast<UINT_PTR>(source);
    CHECK(SUCCEEDED(windows::processConnectionBuffer(processor, format, &input, &input, false, 4)));
    CHECK(input.u32ValidFrameCount == 1 && source[0] == 0.8f);

    CHECK(DllCanUnloadNow() == S_OK);
    CHECK(DllGetClassObject(subonClsid, IID_IClassFactory, nullptr) == E_POINTER);
    IClassFactory* factory = nullptr;
    CHECK(SUCCEEDED(DllGetClassObject(subonClsid, IID_IClassFactory, reinterpret_cast<void**>(&factory))));
    CHECK(DllCanUnloadNow() == S_FALSE); // A live factory must keep its DLL loaded.
    CHECK(FAILED(factory->LockServer(FALSE)));
    CHECK(SUCCEEDED(factory->LockServer(TRUE)));
    CHECK(SUCCEEDED(factory->LockServer(FALSE)));
    CHECK(factory->CreateInstance(nullptr, IID_IUnknown, nullptr) == E_POINTER);
    IAudioProcessingObject* apo = nullptr;
    CHECK(SUCCEEDED(factory->CreateInstance(nullptr, __uuidof(IAudioProcessingObject), reinterpret_cast<void**>(&apo))));
    factory->Release();
    CHECK(DllCanUnloadNow() == S_FALSE);
    APO_REG_PROPERTIES* props = nullptr;
    CHECK(SUCCEEDED(apo->GetRegistrationProperties(&props)) && props && props->u32NumAPOInterfaces == 4);
    CoTaskMemFree(props);
    APOInitBaseStruct init{};
    init.cbSize = sizeof(init);
    init.clsid = subonClsid;
    CHECK(SUCCEEDED(apo->Initialize(sizeof(init), reinterpret_cast<BYTE*>(&init))));
    CHECK(FAILED(apo->Initialize(0, nullptr)));
    IAudioProcessingObjectRT* realtime = nullptr;
    CHECK(SUCCEEDED(apo->QueryInterface(__uuidof(IAudioProcessingObjectRT), reinterpret_cast<void**>(&realtime))));
    output.u32BufferFlags = BUFFER_VALID;
    output.u32ValidFrameCount = 123;
    APO_CONNECTION_PROPERTY* outputs[] = {&output};
    realtime->APOProcess(0, nullptr, 1, outputs);
    CHECK(output.u32BufferFlags == BUFFER_INVALID && output.u32ValidFrameCount == 0);
    realtime->Release();
    apo->Release();
    CHECK(DllCanUnloadNow() == S_OK);
    std::cout << "Windows APO connection and COM lifetime tests passed\n";
    return EXIT_SUCCESS;
}
