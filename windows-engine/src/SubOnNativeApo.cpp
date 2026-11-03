#include "SubOnWindowsApoConnection.h"

#include <atomic>
#include <cstring>
#include <memory>
#include <new>

namespace {
constexpr CLSID CLSID_SubONApo =
{0x8d4b1e62, 0x4f3a, 0x4f2d, {0x9a,0x61,0x72,0x31,0x5b,0x0c,0x8e,0x44}};

std::atomic<long> g_objects{0};
std::atomic<long> g_locks{0};

class SubONApo final :
    public IAudioProcessingObject,
    public IAudioProcessingObjectConfiguration,
    public IAudioProcessingObjectRT,
    public IAudioSystemEffects {
public:
    SubONApo() noexcept { ++g_objects; }
    ~SubONApo() { releaseFormat(); --g_objects; }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (IsEqualIID(iid, IID_IUnknown) ||
            IsEqualIID(iid, __uuidof(IAudioProcessingObject))) {
            *out = static_cast<IAudioProcessingObject*>(this);
        } else if (IsEqualIID(iid, __uuidof(IAudioProcessingObjectConfiguration))) {
            *out = static_cast<IAudioProcessingObjectConfiguration*>(this);
        } else if (IsEqualIID(iid, __uuidof(IAudioProcessingObjectRT))) {
            *out = static_cast<IAudioProcessingObjectRT*>(this);
        } else if (IsEqualIID(iid, __uuidof(IAudioSystemEffects))) {
            *out = static_cast<IAudioSystemEffects*>(this);
        } else {
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(++refs_); }
    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG remaining = static_cast<ULONG>(--refs_);
        if (!remaining) delete this;
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE Reset() override {
        if (processor_) processor_->reset();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetLatency(HNSTIME* time) override {
        if (!time) return E_POINTER;
        *time = 0;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetRegistrationProperties(APO_REG_PROPERTIES** props) override {
        if (!props) return E_POINTER;
        *props = nullptr;
        constexpr UINT32 count = 4;
        const SIZE_T bytes = sizeof(APO_REG_PROPERTIES) + (count - 1) * sizeof(IID);
        auto* p = static_cast<APO_REG_PROPERTIES*>(CoTaskMemAlloc(bytes));
        if (!p) return E_OUTOFMEMORY;
        ZeroMemory(p, bytes);
        p->clsid = CLSID_SubONApo;
        p->Flags = static_cast<APO_FLAG>(APO_FLAG_INPLACE | APO_FLAG_SAMPLESPERFRAME_MUST_MATCH |
                   APO_FLAG_FRAMESPERSECOND_MUST_MATCH | APO_FLAG_BITSPERSAMPLE_MUST_MATCH);
        wcscpy_s(p->szFriendlyName, L"SubON System Audio");
        wcscpy_s(p->szCopyrightInfo, L"SubON");
        p->u32MajorVersion = 1;
        p->u32MinorVersion = 0;
        p->u32MinInputConnections = p->u32MaxInputConnections = 1;
        p->u32MinOutputConnections = p->u32MaxOutputConnections = 1;
        p->u32MaxInstances = 8;
        p->u32NumAPOInterfaces = count;
        p->iidAPOInterfaceList[0] = __uuidof(IAudioProcessingObject);
        p->iidAPOInterfaceList[1] = __uuidof(IAudioProcessingObjectConfiguration);
        p->iidAPOInterfaceList[2] = __uuidof(IAudioProcessingObjectRT);
        p->iidAPOInterfaceList[3] = __uuidof(IAudioSystemEffects);
        *props = p;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Initialize(UINT32 size, BYTE* data) override {
        if (size != 0 || data != nullptr) return E_INVALIDARG;
        initialized_ = true;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE IsInputFormatSupported(IAudioMediaType*, IAudioMediaType* requested,
                                                       IAudioMediaType** supported) override {
        return checkFormat(requested, supported);
    }
    HRESULT STDMETHODCALLTYPE IsOutputFormatSupported(IAudioMediaType*, IAudioMediaType* requested,
                                                        IAudioMediaType** supported) override {
        return checkFormat(requested, supported);
    }
    HRESULT STDMETHODCALLTYPE GetInputChannelCount(UINT32* count) override {
        if (!count) return E_POINTER;
        *count = format_.channels;
        return format_.channels ? S_OK : E_FAIL;
    }

    HRESULT STDMETHODCALLTYPE LockForProcess(UINT32 inCount, APO_CONNECTION_DESCRIPTOR** inputs,
                                               UINT32 outCount, APO_CONNECTION_DESCRIPTOR** outputs) override {
        if (!initialized_ || locked_ || inCount != 1 || outCount != 1 ||
            !inputs || !outputs || !inputs[0] || !outputs[0] ||
            !inputs[0]->pFormat || !outputs[0]->pFormat) return E_INVALIDARG;
        const WAVEFORMATEX* wf = inputs[0]->pFormat->GetAudioFormat();
        subon::apo::AudioFormat parsed{};
        if (!subon::apo::windows::describeFloat32Format(wf, parsed)) return APOERR_FORMAT_NOT_SUPPORTED;
        const WAVEFORMATEX* outWf = outputs[0]->pFormat->GetAudioFormat();
        subon::apo::AudioFormat outParsed{};
        if (!subon::apo::windows::describeFloat32Format(outWf, outParsed) ||
            parsed.sampleRate != outParsed.sampleRate || parsed.channels != outParsed.channels) {
            return APOERR_FORMAT_NOT_SUPPORTED;
        }
        format_ = parsed;
        inputs[0]->pFormat->AddRef();
        formatMediaType_ = inputs[0]->pFormat;
        subon::dsp::ProcessorConfig config{};
        config.sampleRate = static_cast<double>(format_.sampleRate);
        processor_.reset(new (std::nothrow) subon::apo::RealtimeProcessor(config, format_));
        if (!processor_) { releaseFormat(); return E_OUTOFMEMORY; }
        locked_ = true;
        ++g_locks;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE UnlockForProcess() override {
        if (locked_) { locked_ = false; --g_locks; }
        processor_.reset();
        releaseFormat();
        return S_OK;
    }
    void STDMETHODCALLTYPE APOProcess(UINT32 inCount, APO_CONNECTION_PROPERTY** inputs,
                                       UINT32 outCount, APO_CONNECTION_PROPERTY** outputs) override {
        if (!locked_ || inCount != 1 || outCount != 1 || !inputs || !outputs ||
            !inputs[0] || !outputs[0]) return;
        (void)subon::apo::windows::processConnectionBuffer(
            *processor_, formatMediaType_->GetAudioFormat(), inputs[0], outputs[0], true);
    }
    UINT32 STDMETHODCALLTYPE CalcInputFrames(UINT32 outputFrames) override { return outputFrames; }
    UINT32 STDMETHODCALLTYPE CalcOutputFrames(UINT32 inputFrames) override { return inputFrames; }

private:
    static HRESULT checkFormat(IAudioMediaType* requested, IAudioMediaType** supported) {
        if (supported) *supported = nullptr;
        if (!requested) return E_POINTER;
        subon::apo::AudioFormat parsed{};
        if (!subon::apo::windows::describeFloat32Format(requested->GetAudioFormat(), parsed))
            return APOERR_FORMAT_NOT_SUPPORTED;
        if (supported) { requested->AddRef(); *supported = requested; }
        return S_OK;
    }
    void releaseFormat() noexcept {
        if (formatMediaType_) { formatMediaType_->Release(); formatMediaType_ = nullptr; }
    }
    std::atomic<ULONG> refs_{1};
    bool initialized_ = false;
    bool locked_ = false;
    subon::apo::AudioFormat format_{};
    IAudioMediaType* formatMediaType_ = nullptr;
    std::unique_ptr<subon::apo::RealtimeProcessor> processor_;
};

class ClassFactory final : public IClassFactory {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (IsEqualIID(iid, IID_IUnknown) || IsEqualIID(iid, IID_IClassFactory)) {
            *out = static_cast<IClassFactory*>(this); AddRef(); return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(++refs_); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG n = static_cast<ULONG>(--refs_); if (!n) delete this; return n;
    }
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer, REFIID iid, void** out) override {
        if (outer) return CLASS_E_NOAGGREGATION;
        auto* object = new (std::nothrow) SubONApo();
        if (!object) return E_OUTOFMEMORY;
        HRESULT hr = object->QueryInterface(iid, out);
        object->Release();
        return hr;
    }
    HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override {
        if (lock) ++g_locks; else --g_locks;
        return S_OK;
    }
private:
    std::atomic<ULONG> refs_{1};
};
}

STDAPI DllGetClassObject(REFCLSID clsid, REFIID iid, void** out) {
    if (!IsEqualCLSID(clsid, CLSID_SubONApo)) return CLASS_E_CLASSNOTAVAILABLE;
    auto* factory = new (std::nothrow) ClassFactory();
    if (!factory) return E_OUTOFMEMORY;
    HRESULT hr = factory->QueryInterface(iid, out);
    factory->Release();
    return hr;
}
STDAPI DllCanUnloadNow() {
    return (g_objects.load() == 0 && g_locks.load() == 0) ? S_OK : S_FALSE;
}
