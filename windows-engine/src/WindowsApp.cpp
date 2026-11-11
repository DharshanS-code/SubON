#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <windowsx.h>
#include <uxtheme.h>
#include "SubOnDsp.h"
#include "WavFile.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <fstream>
#include <memory>
#include <streambuf>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef _MSC_VER
#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Uxtheme.lib")
#endif

namespace {
constexpr COLORREF background = RGB(16, 20, 28);
constexpr COLORREF surface = RGB(27, 34, 48);
constexpr COLORREF textColor = RGB(241, 244, 250);
constexpr COLORREF muted = RGB(166, 177, 194);
constexpr COLORREF accent = RGB(204, 203, 102);
constexpr COLORREF errorColor = RGB(255, 180, 168);
constexpr COLORREF border = RGB(52, 65, 88);
constexpr UINT_PTR stateTimer = 1;
constexpr int contentHeight = 1692;

enum ControlId : int {
    openFileId = 100, playId, stopId, flatId, bassPresetId, vocalId,
    preampId = 200, bassId, trebleId, limiterId, eqFirstId = 220
};
enum class Phase { idle, starting, playing, finished, stopped, error };
struct PlaybackState {
    Phase phase = Phase::idle;
    std::wstring error;
};
struct LoadResult {
    bool done = false;
    std::shared_ptr<const subon::desktop::WavData> wav;
    std::wstring filename;
    std::wstring error;
};
struct ControlLayout {
    HWND window{};
    int x{}, y{}, width{}, height{};
    bool stretch = false;
    bool rightAligned = false;
    bool onSurface = true;
    bool secondary = false;
    int font = 0;
};
struct GainControl {
    HWND slider{};
    HWND value{};
};

HWND g_window{}, g_status{}, g_file{}, g_format{}, g_play{}, g_stopButton{}, g_limiter{};
std::array<GainControl, 3> g_tone{};
std::array<GainControl, subon::dsp::kEqualizerBandCount> g_eq{};
std::vector<ControlLayout> g_controls;
std::array<HFONT, 3> g_fonts{};
HBRUSH g_backgroundBrush{}, g_surfaceBrush{};
UINT g_dpi = 96;
int g_scroll = 0;
int g_pageHeight = 0;
int g_contentWidth = 640;
int g_wheelRemainder = 0;
bool g_statusError = false;
bool g_running = false; // UI-thread only: a worker exists until the UI joins it.
bool g_loading = false; // UI-thread only: loader results are adopted by the timer.
bool g_closing = false;
std::shared_ptr<const subon::desktop::WavData> g_wav;
std::thread g_worker;
std::thread g_loader;
std::atomic<bool> g_cancelLoad{false};
std::mutex g_loadMutex;
LoadResult g_loadResult;
std::atomic<bool> g_stop{false};
std::mutex g_configMutex;
subon::dsp::ProcessorConfig g_config;
std::mutex g_stateMutex;
PlaybackState g_state;
Phase g_displayedPhase = Phase::idle;

int px(int logical) { return MulDiv(logical, static_cast<int>(g_dpi), 96); }
int logical(int pixels) { return MulDiv(pixels, 96, static_cast<int>(g_dpi)); }
UINT windowDpi(HWND window) {
    using GetDpi = UINT (WINAPI*)(HWND);
    const auto getDpi = reinterpret_cast<GetDpi>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    if (getDpi) return getDpi(window);
    HDC dc = GetDC(window);
    const auto dpi = static_cast<UINT>(GetDeviceCaps(dc, LOGPIXELSX));
    ReleaseDC(window, dc);
    return dpi;
}
std::wstring widen(const char* ascii) { return {ascii, ascii + std::strlen(ascii)}; }

// These functions only copy ordinary data. No worker path touches an HWND,
// calls SendMessage, or asks the UI for a control value, including at shutdown.
subon::dsp::ProcessorConfig configuration() {
    std::lock_guard<std::mutex> lock(g_configMutex);
    return g_config;
}
PlaybackState playbackState() {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return g_state;
}
void publishState(Phase phase, std::wstring error = {}) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_state = {phase, std::move(error)};
}

class PlaybackError : public std::runtime_error {
public:
    explicit PlaybackError(std::wstring message)
        : std::runtime_error("Audio playback failed"), message_(std::move(message)) {}
    const std::wstring& message() const noexcept { return message_; }
private:
    std::wstring message_;
};
void checkWave(MMRESULT result, const wchar_t* operation) {
    if (result == MMSYSERR_NOERROR) return;
    wchar_t description[MAXERRORLENGTH]{};
    if (waveOutGetErrorTextW(result, description, MAXERRORLENGTH) != MMSYSERR_NOERROR)
        std::swprintf(description, MAXERRORLENGTH, L"Windows audio error %u", result);
    throw PlaybackError(std::wstring(operation) + L": " + description);
}

// The worker is the sole owner of the device, event, headers AND sample storage.
// Two reusable buffers keep the output queue fed. Reset/unprepare completes
// before either a header or its bytes can be destroyed, even during unwinding.
class WaveOutput {
public:
    struct Buffer {
        WAVEHDR header{};
        std::vector<std::uint8_t> bytes;
        bool prepared = false;
        bool queued = false;
    };
    WaveOutput() = default;
    WaveOutput(const WaveOutput&) = delete;
    WaveOutput& operator=(const WaveOutput&) = delete;
    WaveOutput(WaveOutput&&) = delete;
    WaveOutput& operator=(WaveOutput&&) = delete;
    ~WaveOutput() {
        if (wave_) {
            waveOutReset(wave_);
            for (auto& buffer : buffers_) release(buffer);
            waveOutClose(wave_);
        }
        if (event_) CloseHandle(event_);
    }
    void open(const subon::desktop::WavFormat& format) {
        for (auto& buffer : buffers_) buffer.bytes.resize(chunkFrames * format.blockAlign);
        event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!event_) throw PlaybackError(L"Could not create the audio completion event.");
        WAVEFORMATEX waveFormat{};
        waveFormat.wFormatTag = format.tag;
        waveFormat.nChannels = format.channels;
        waveFormat.nSamplesPerSec = format.sampleRate;
        waveFormat.nAvgBytesPerSec = format.byteRate;
        waveFormat.nBlockAlign = format.blockAlign;
        waveFormat.wBitsPerSample = format.bitsPerSample;
        checkWave(waveOutOpen(&wave_, WAVE_MAPPER, &waveFormat,
            reinterpret_cast<DWORD_PTR>(event_), 0, CALLBACK_EVENT), L"Cannot open the Windows output device");
    }
    Buffer& buffer(std::size_t index) { return buffers_[index]; }
    void submit(Buffer& buffer, std::size_t bytes) {
        buffer.header = {};
        buffer.header.lpData = reinterpret_cast<LPSTR>(buffer.bytes.data());
        buffer.header.dwBufferLength = static_cast<DWORD>(bytes);
        checkWave(waveOutPrepareHeader(wave_, &buffer.header, sizeof(WAVEHDR)), L"Cannot prepare audio buffer");
        buffer.prepared = true;
        checkWave(waveOutWrite(wave_, &buffer.header, sizeof(WAVEHDR)), L"Cannot queue audio buffer");
        buffer.queued = true;
    }
    bool wait(Buffer& buffer) {
        while (buffer.prepared) {
            if (g_stop.load()) return false;
            // Query completion via WinMM rather than reading asynchronously
            // modified WAVEHDR flags. STILLPLAYING keeps the buffer alive.
            const auto result = waveOutUnprepareHeader(wave_, &buffer.header, sizeof(WAVEHDR));
            if (result == MMSYSERR_NOERROR) {
                buffer.prepared = false;
                buffer.queued = false;
                return true;
            }
            if (result != WAVERR_STILLPLAYING) checkWave(result, L"Cannot release audio buffer");
            if (WaitForSingleObject(event_, 20) == WAIT_FAILED)
                throw PlaybackError(L"The audio completion wait failed.");
        }
        return true;
    }
    void close() {
        if (!wave_) return;
        // waveOutReset returns pending buffers to the application. Never let
        // stop bypass this operation and free a still-queued stack header.
        checkWave(waveOutReset(wave_), L"Cannot stop the Windows output device");
        for (auto& buffer : buffers_) checkWave(release(buffer), L"Cannot release audio buffer");
        checkWave(waveOutClose(wave_), L"Cannot close the Windows output device");
        wave_ = nullptr;
    }
    static constexpr std::size_t chunkFrames = 2048;
private:
    MMRESULT release(Buffer& buffer) noexcept {
        if (!buffer.prepared) return MMSYSERR_NOERROR;
        MMRESULT result{};
        do {
            result = waveOutUnprepareHeader(wave_, &buffer.header, sizeof(WAVEHDR));
            if (result == WAVERR_STILLPLAYING) {
                // A driver may return STILLPLAYING while its reset completes.
                // Retain both storage and header until ownership is returned.
                waveOutReset(wave_);
                WaitForSingleObject(event_, 20);
            }
        } while (result == WAVERR_STILLPLAYING);
        if (result == MMSYSERR_NOERROR) {
            buffer.prepared = false;
            buffer.queued = false;
        }
        return result;
    }
    HWAVEOUT wave_{};
    HANDLE event_{};
    std::array<Buffer, 2> buffers_{};
};
bool sameConfig(const subon::dsp::ProcessorConfig& a, const subon::dsp::ProcessorConfig& b) {
    return a.sampleRate == b.sampleRate && a.preampDb == b.preampDb &&
        a.bassDb == b.bassDb && a.trebleDb == b.trebleDb && a.equalizerDb == b.equalizerDb &&
        a.limiterEnabled == b.limiterEnabled && a.limiterCeiling == b.limiterCeiling;
}
void playback(std::shared_ptr<const subon::desktop::WavData> wav) noexcept {
    try {
        subon::dsp::AudioProcessor left, right;
        subon::dsp::ProcessorConfig applied{};
        bool configured = false;
        const auto& format = wav->format;
        const auto totalFrames = wav->pcm.size() / format.blockAlign;
        const unsigned bytesPerSample = format.bitsPerSample / 8;
        std::size_t frame = 0;
        WaveOutput output;
        output.open(format);
        const auto fill = [&](WaveOutput::Buffer& buffer) {
            if (frame >= totalFrames || g_stop.load()) return false;
            auto config = configuration();
            config.sampleRate = format.sampleRate;
            if (!configured || !sameConfig(config, applied)) {
                left.configure(config);
                right.configure(config);
                applied = config;
                configured = true;
            }
            const auto frames = std::min(WaveOutput::chunkFrames, totalFrames - frame);
            for (std::size_t i = 0; i < frames; ++i) {
                for (unsigned channel = 0; channel < format.channels; ++channel) {
                    const auto offset = i * format.blockAlign + channel * bytesPerSample;
                    const auto* source = wav->pcm.data() + frame * format.blockAlign + offset;
                    auto* destination = buffer.bytes.data() + offset;
                    const double sample = subon::desktop::decodeSample(source, format);
                    const double processed = (channel == 0 ? left : right).process(sample);
                    subon::desktop::encodeSample(destination, format, processed);
                }
            }
            if (g_stop.load()) return false;
            output.submit(buffer, frames * format.blockAlign);
            frame += frames;
            return true;
        };
        fill(output.buffer(0));
        fill(output.buffer(1));
        if (!g_stop.load()) publishState(Phase::playing);
        std::size_t next = 0;
        while (!g_stop.load() && output.buffer(next).queued) {
            if (!output.wait(output.buffer(next))) break;
            fill(output.buffer(next));
            next = (next + 1) % 2;
        }
        output.close();
        publishState(g_stop.load() ? Phase::stopped : Phase::finished);
    } catch (const PlaybackError& error) {
        publishState(Phase::error, error.message());
    } catch (const std::exception& error) {
        publishState(Phase::error, L"Playback failed: " + widen(error.what()));
    } catch (...) {
        publishState(Phase::error, L"Playback failed unexpectedly. Stop and reopen the WAV file.");
    }
}

// The parser accepts an istream. Bound each underlying file read so cancellation
// is observed between chunks without changing the portable parser or buffering a
// second full copy of the file. A filesystem call already in progress may still
// take time; normal close keeps the UI pumping until the loader exits.
class CancellableFileBuffer final : public std::streambuf {
public:
    explicit CancellableFileBuffer(const std::filesystem::path& path) {
        if (!file_.open(path, std::ios::in | std::ios::binary))
            throw subon::desktop::WavError("Could not open that file. Check its location and permissions.");
    }
protected:
    std::streamsize xsgetn(char* destination, std::streamsize requested) override {
        constexpr std::streamsize blockBytes = 64 * 1024;
        std::streamsize count = 0;
        while (count < requested && !g_cancelLoad.load()) {
            const auto block = std::min(blockBytes, requested - count);
            const auto received = file_.sgetn(destination + count, block);
            count += received;
            if (received != block) break;
        }
        return count;
    }
    pos_type seekoff(off_type offset, std::ios_base::seekdir direction,
                     std::ios_base::openmode mode) override {
        return g_cancelLoad.load() ? pos_type(off_type(-1)) : file_.pubseekoff(offset, direction, mode);
    }
    pos_type seekpos(pos_type position, std::ios_base::openmode mode) override {
        return g_cancelLoad.load() ? pos_type(off_type(-1)) : file_.pubseekpos(position, mode);
    }
private:
    std::filebuf file_;
};
void loadFile(std::filesystem::path path) noexcept {
    LoadResult result;
    try {
        result.filename = path.filename().wstring();
        if (!g_cancelLoad.load()) {
            CancellableFileBuffer buffer(path);
            std::istream stream(&buffer);
            auto loaded = std::make_shared<const subon::desktop::WavData>(subon::desktop::readWav(stream));
            if (!g_cancelLoad.load()) result.wav = std::move(loaded);
        }
    } catch (const std::exception& error) {
        if (!g_cancelLoad.load()) result.error = widen(error.what());
    } catch (...) {
        if (!g_cancelLoad.load()) result.error = L"The WAV file could not be loaded.";
    }
    result.done = true;
    std::lock_guard<std::mutex> lock(g_loadMutex);
    g_loadResult = std::move(result);
}

const ControlLayout* layoutFor(HWND window) {
    const auto item = std::find_if(g_controls.begin(), g_controls.end(),
        [window](const ControlLayout& control) { return control.window == window; });
    return item == g_controls.end() ? nullptr : &*item;
}
void setStatus(const std::wstring& message, bool error = false) {
    g_statusError = error;
    SetWindowTextW(g_status, message.c_str());
    InvalidateRect(g_status, nullptr, TRUE);
    NotifyWinEvent(EVENT_OBJECT_NAMECHANGE, g_status, OBJID_CLIENT, CHILDID_SELF);
}
void updatePlaybackButtons() {
    EnableWindow(GetDlgItem(g_window, openFileId), !g_loading && !g_closing);
    EnableWindow(g_play, !g_running && !g_loading && !g_closing && static_cast<bool>(g_wav));
    EnableWindow(g_stopButton, !g_closing && (g_running || (g_loading && !g_cancelLoad.load())));
    SetWindowTextW(g_stopButton, g_loading ? L"&Cancel" : L"&Stop");
    InvalidateRect(g_play, nullptr, TRUE);
    InvalidateRect(g_stopButton, nullptr, TRUE);
}
void cancelLoading() {
    if (!g_loading) return;
    g_cancelLoad.store(true);
    updatePlaybackButtons();
    setStatus(g_closing ? L"Cancelling file loading before closing..." : L"Cancelling file loading...");
}
void updateLoading() {
    if (!g_loading) return;
    LoadResult result;
    {
        std::lock_guard<std::mutex> lock(g_loadMutex);
        if (!g_loadResult.done) return;
        result = std::move(g_loadResult);
        g_loadResult = {};
    }
    if (g_loader.joinable()) g_loader.join();
    g_loading = false;
    if (g_closing) {
        DestroyWindow(g_window);
        return;
    }
    const auto retained = g_wav ? L" The previous file is still selected." : L"";
    if (g_cancelLoad.load()) {
        setStatus(std::wstring(L"Loading cancelled.") + retained);
    } else if (!result.error.empty()) {
        setStatus(result.error + retained, true);
    } else if (result.wav) {
        g_wav = std::move(result.wav);
        SetWindowTextW(g_file, result.filename.c_str());
        const auto& format = g_wav->format;
        const auto seconds = g_wav->pcm.size() / format.byteRate;
        wchar_t rate[24]{};
        std::swprintf(rate, 24, L"%g kHz / ", format.sampleRate / 1000.0);
        const auto description = std::wstring(rate) + std::to_wstring(format.bitsPerSample) +
            (format.tag == 3 ? L"-bit float / " : L"-bit PCM / ") +
            (format.channels == 1 ? L"Mono / " : L"Stereo / ") + std::to_wstring(seconds) + L" sec";
        SetWindowTextW(g_format, description.c_str());
        setStatus(L"Ready to play. Audio uses your default Windows output device.");
    } else {
        setStatus(std::wstring(L"No audio was loaded.") + retained, true);
    }
    updatePlaybackButtons();
    SetFocus(g_wav ? g_play : GetDlgItem(g_window, openFileId));
}
void updateState() {
    const auto state = playbackState();
    if (g_running && (state.phase == Phase::finished || state.phase == Phase::stopped || state.phase == Phase::error)) {
        if (g_worker.joinable()) g_worker.join();
        g_running = false;
        updatePlaybackButtons();
    }
    if (state.phase == g_displayedPhase) return;
    g_displayedPhase = state.phase;
    switch (state.phase) {
    case Phase::starting: setStatus(L"Starting the Windows output device..."); break;
    case Phase::playing: setStatus(L"Playing through SubON. Changes apply to the next queued audio blocks."); break;
    case Phase::finished: setStatus(L"Playback finished. Play starts again from the beginning."); break;
    case Phase::stopped: setStatus(L"Stopped. Play starts again from the beginning."); break;
    case Phase::error: setStatus(state.error, true); break;
    default: break;
    }
}
void stopPlayback() {
    g_stop.store(true);
    // Safe to join without pumping messages: the worker never waits for the UI.
    // In particular, the UI must never call waveOutReset on the worker's handle.
    if (g_worker.joinable()) g_worker.join();
    g_running = false;
    g_stop.store(false);
    updateState();
    updatePlaybackButtons();
}
void snapshotControls() {
    // UI calls happen before locking. Only the completed value object is shared.
    auto config = configuration();
    config.preampDb = static_cast<int>(SendMessageW(g_tone[0].slider, TBM_GETPOS, 0, 0));
    config.bassDb = static_cast<int>(SendMessageW(g_tone[1].slider, TBM_GETPOS, 0, 0));
    config.trebleDb = static_cast<int>(SendMessageW(g_tone[2].slider, TBM_GETPOS, 0, 0));
    config.limiterEnabled = SendMessageW(g_limiter, BM_GETCHECK, 0, 0) == BST_CHECKED;
    for (std::size_t i = 0; i < g_eq.size(); ++i)
        config.equalizerDb[i] = static_cast<int>(SendMessageW(g_eq[i].slider, TBM_GETPOS, 0, 0));
    {
        std::lock_guard<std::mutex> lock(g_configMutex);
        g_config = config;
    }
    const auto updateGain = [](const GainControl& gain) {
        const int value = static_cast<int>(SendMessageW(gain.slider, TBM_GETPOS, 0, 0));
        wchar_t text[24]{};
        std::swprintf(text, 24, L"%+d dB", value);
        SetWindowTextW(gain.value, text);
    };
    for (const auto& gain : g_tone) updateGain(gain);
    for (const auto& gain : g_eq) updateGain(gain);
}
void openFile() {
    if (g_loading || g_closing) return;
    OPENFILENAMEW dialog{};
    std::array<wchar_t, 32768> path{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = g_window;
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.lpstrFilter = L"WAV audio (*.wav)\0*.wav\0All files (*.*)\0*.*\0";
    dialog.nFilterIndex = 1;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&dialog)) {
        const DWORD error = CommDlgExtendedError();
        if (error != 0) setStatus(L"The file picker failed (Windows error " + std::to_wstring(error) + L").", true);
        return;
    }
    stopPlayback();
    publishState(Phase::idle);
    g_displayedPhase = Phase::idle;
    g_cancelLoad.store(false);
    {
        std::lock_guard<std::mutex> lock(g_loadMutex);
        g_loadResult = {};
    }
    try {
        g_loader = std::thread(loadFile, std::filesystem::path(path.data()));
        g_loading = true;
        setStatus(L"Loading WAV... You can cancel while the file is being checked.");
    } catch (const std::exception& error) {
        setStatus(L"Cannot start file loading: " + widen(error.what()) +
            (g_wav ? L" The previous file is still selected." : L""), true);
    }
    updatePlaybackButtons();
    if (g_loading) SetFocus(g_stopButton);
}
void startPlayback() {
    if (g_running || g_loading || g_closing || !g_wav) return;
    if (g_worker.joinable()) g_worker.join();
    snapshotControls();
    g_stop.store(false);
    publishState(Phase::starting);
    g_displayedPhase = Phase::idle;
    try {
        g_worker = std::thread(playback, g_wav);
        g_running = true;
    } catch (const std::exception& error) {
        publishState(Phase::error, L"Cannot start playback: " + widen(error.what()));
    }
    updatePlaybackButtons();
    updateState();
    if (g_running) SetFocus(g_stopButton);
}
void preset(int kind) {
    SendMessageW(g_tone[0].slider, TBM_SETPOS, TRUE, kind == 2 ? 2 : kind == 1 ? -2 : 0);
    SendMessageW(g_tone[1].slider, TBM_SETPOS, TRUE, kind == 1 ? 6 : kind == 2 ? 1 : 0);
    SendMessageW(g_tone[2].slider, TBM_SETPOS, TRUE, kind == 2 ? 4 : kind == 1 ? 1 : 0);
    for (const auto& gain : g_eq) SendMessageW(gain.slider, TBM_SETPOS, TRUE, 0);
    if (kind == 2) {
        SendMessageW(g_eq[2].slider, TBM_SETPOS, TRUE, -2);
        SendMessageW(g_eq[3].slider, TBM_SETPOS, TRUE, -1);
        SendMessageW(g_eq[5].slider, TBM_SETPOS, TRUE, 2);
    }
    snapshotControls();
}

void arrangeControls();
void setScroll(int position) {
    const int maximum = std::max(0, px(contentHeight) - g_pageHeight);
    const int next = std::clamp(position, 0, maximum);
    if (g_scroll == next) return;
    g_scroll = next;
    arrangeControls();
    SetScrollPos(g_window, SB_VERT, g_scroll, TRUE);
    RedrawWindow(g_window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}
void revealControl(HWND control) {
    const auto* item = layoutFor(control);
    if (!item) return;
    const int top = px(item->y);
    const int bottom = px(item->y + item->height);
    if (top < g_scroll + px(12)) setScroll(top - px(12));
    else if (bottom > g_scroll + g_pageHeight - px(12)) setScroll(bottom - g_pageHeight + px(12));
}
void wheelScroll(WPARAM parameter) {
    UINT lines = 3;
    SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
    g_wheelRemainder += GET_WHEEL_DELTA_WPARAM(parameter);
    const int notches = g_wheelRemainder / WHEEL_DELTA;
    g_wheelRemainder %= WHEEL_DELTA;
    const int step = lines == WHEEL_PAGESCROLL ? g_pageHeight : px(20) * static_cast<int>(std::min(lines, 20u));
    setScroll(g_scroll - notches * step);
}
LRESULT CALLBACK childProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
                           UINT_PTR, DWORD_PTR) {
    if (message == WM_SETFOCUS) revealControl(window);
    if (message == WM_MOUSEWHEEL) {
        wheelScroll(wparam);
        return 0; // Scroll the page; never change a gain accidentally while scrolling.
    }
    if (message == WM_NCDESTROY) RemoveWindowSubclass(window, childProc, 1);
    return DefSubclassProc(window, message, wparam, lparam);
}
HWND control(const wchar_t* type, const wchar_t* text, DWORD style, int id,
             int x, int y, int width, int height, bool stretch = false,
             bool rightAligned = false, bool onSurface = true, bool secondary = false, int font = 0) {
    HWND window = CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style,
        0, 0, 0, 0, g_window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    if (!window) throw std::runtime_error("Could not create a native desktop control.");
    g_controls.push_back({window, x, y, width, height, stretch, rightAligned, onSurface, secondary, font});
    SendMessageW(window, WM_SETFONT, reinterpret_cast<WPARAM>(g_fonts[font]), TRUE);
    SetWindowSubclass(window, childProc, 1, 0);
    return window;
}
HWND label(const wchar_t* text, int x, int y, int width, int height = 24,
           bool stretch = false, bool onSurface = true, bool secondary = false, int font = 0, DWORD style = 0) {
    return control(L"STATIC", text, style, 0, x, y, width, height, stretch, false, onSurface, secondary, font);
}
HWND button(const wchar_t* text, int id, int x, int y, int width) {
    return control(L"BUTTON", text, BS_OWNERDRAW | WS_TABSTOP, id, x, y, width, 48);
}
GainControl gain(const wchar_t* title, const wchar_t* name, int id, int y) {
    label(title, 40, y, 360, 24, true);
    auto slider = control(TRACKBAR_CLASSW, name, WS_TABSTOP | TBS_HORZ | TBS_NOTICKS,
        id, 40, y + 24, 464, 48, true);
    SetWindowTheme(slider, L"", L"");
    SendMessageW(slider, TBM_SETRANGE, TRUE, MAKELONG(-12, 12));
    SendMessageW(slider, TBM_SETPOS, TRUE, 0);
    SendMessageW(slider, TBM_SETPAGESIZE, 0, 3);
    auto value = control(L"STATIC", L"+0 dB", SS_RIGHT | SS_CENTERIMAGE, 0,
        40, y + 26, 72, 44, false, true);
    return {slider, value};
}
void createFonts() {
    const std::array<int, 3> heights{16, 20, 32};
    std::array<HFONT, 3> fonts{};
    for (std::size_t i = 0; i < fonts.size(); ++i)
        fonts[i] = CreateFontW(-px(heights[i]), 0, 0, 0, i == 0 ? FW_NORMAL : FW_SEMIBOLD,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    for (const auto& item : g_controls)
        SendMessageW(item.window, WM_SETFONT, reinterpret_cast<WPARAM>(fonts[item.font]), TRUE);
    for (auto font : g_fonts) if (font) DeleteObject(font);
    g_fonts = fonts;
}
void createControls() {
    label(L"SubON", 24, 22, 350, 42, true, false, false, 2);
    label(L"Shape your sound", 24, 68, 350, 26, true, false, true);
    label(L"EXPERIMENTAL / LOCAL WAV PLAYER", 24, 108, 570, 24, true, false, true);
    label(L"Only audio played here is processed. Other apps and system audio are not changed.",
          24, 138, 570, 56, true, false, true);

    label(L"Playback", 40, 218, 400, 30, true, true, false, 1);
    g_file = label(L"No file selected", 40, 258, 550, 26, true, true, false, 0, SS_PATHELLIPSIS | SS_NOPREFIX);
    g_format = label(L"PCM 16/24/32-bit or float32 / Mono or stereo / >32-192 kHz", 40, 288, 550, 48, true, true, true);
    button(L"&Open WAV", openFileId, 40, 344, 160);
    g_play = button(L"&Play", playId, 212, 344, 110);
    g_stopButton = button(L"&Stop", stopId, 334, 344, 110);
    g_status = label(L"Open a WAV file to begin.", 40, 408, 550, 78, true, true, true);

    label(L"Tone & equalizer", 40, 526, 440, 30, true, true, false, 1, SS_NOPREFIX);
    label(L"Start with a preset, then make it yours.", 40, 564, 520, 24, true, true, true);
    button(L"&Flat", flatId, 40, 600, 104);
    button(L"&Bass boost", bassPresetId, 156, 600, 156);
    button(L"&Vocal", vocalId, 324, 600, 120);
    g_tone[1] = gain(L"Bass shelf", L"Bass shelf gain in decibels", bassId, 668);
    g_tone[2] = gain(L"Treble shelf", L"Treble shelf gain in decibels", trebleId, 744);
    label(L"10-band equalizer", 40, 830, 450, 30, true, true, false, 1);
    label(L"-12 to +12 dB / Arrow keys: 1 dB / Page keys: 3 dB", 40, 868, 530, 48, true, true, true);
    // Compact horizontal bands retain real native sliders and numerical values.
    // A single responsive column keeps the keyboard order and small-window layout predictable.
    for (std::size_t i = 0; i < g_eq.size(); ++i) {
        wchar_t name[64]{};
        const double frequency = subon::dsp::kEqualizerFrequencies[i];
        if (frequency >= 1000) std::swprintf(name, 64, L"%g kHz", frequency / 1000);
        else std::swprintf(name, 64, L"%g Hz", frequency);
        const int y = 916 + static_cast<int>(i) * 48;
        label(name, 40, y, 82, 38, false, true, false, 0, SS_CENTERIMAGE);
        const auto accessibleName = std::wstring(name) + L" equalizer gain in decibels";
        auto slider = control(TRACKBAR_CLASSW, accessibleName.c_str(), WS_TABSTOP | TBS_HORZ | TBS_NOTICKS,
            eqFirstId + static_cast<int>(i), 126, y - 3, 378, 48, true);
        SetWindowTheme(slider, L"", L"");
        SendMessageW(slider, TBM_SETRANGE, TRUE, MAKELONG(-12, 12));
        SendMessageW(slider, TBM_SETPAGESIZE, 0, 3);
        auto value = control(L"STATIC", L"+0 dB", SS_RIGHT | SS_CENTERIMAGE, 0, 40, y, 72, 38, false, true);
        g_eq[i] = {slider, value};
    }

    label(L"Output & protection", 40, 1430, 440, 30, true, true, false, 1, SS_NOPREFIX);
    g_tone[0] = gain(L"Preamp", L"Preamp gain in decibels", preampId, 1472);
    g_limiter = control(L"BUTTON", L"Enable &limiter", WS_TABSTOP | BS_AUTOCHECKBOX,
        limiterId, 40, 1552, 350, 48, true);
    SetWindowTheme(g_limiter, L"", L"");
    SendMessageW(g_limiter, BM_SETCHECK, BST_CHECKED, 0);
    label(L"Reduces peaks; it does not guarantee hearing safety. Start with a low device volume.",
        40, 1610, 540, 60, true, true, true);
    snapshotControls();
    updatePlaybackButtons();
}
void arrangeControls() {
    for (const auto& item : g_controls) {
        int width = item.stretch ? item.width + g_contentWidth - 640 : item.width;
        int x = item.rightAligned ? g_contentWidth - item.x - item.width : item.x;
        MoveWindow(item.window, px(x), px(item.y) - g_scroll, px(std::max(width, 24)), px(item.height), TRUE);
    }
}
void resizeLayout() {
    RECT client{};
    GetClientRect(g_window, &client);
    g_pageHeight = client.bottom;
    g_contentWidth = std::max(480, logical(client.right));
    g_scroll = std::clamp(g_scroll, 0, std::max(0, px(contentHeight) - g_pageHeight));
    SCROLLINFO info{sizeof(info)};
    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin = 0;
    info.nMax = px(contentHeight) - 1;
    info.nPage = static_cast<UINT>(g_pageHeight);
    info.nPos = g_scroll;
    SetScrollInfo(g_window, SB_VERT, &info, TRUE);
    arrangeControls();
    InvalidateRect(g_window, nullptr, TRUE);
}
void paintCard(HDC dc, int top, int bottom) {
    RECT rect{px(24), px(top) - g_scroll, px(g_contentWidth - 24), px(bottom) - g_scroll};
    const auto pen = CreatePen(PS_SOLID, px(1), border);
    const auto oldPen = SelectObject(dc, pen);
    const auto oldBrush = SelectObject(dc, g_surfaceBrush);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, px(16), px(16));
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}
void drawButton(const DRAWITEMSTRUCT& item) {
    const bool enabled = (item.itemState & ODS_DISABLED) == 0;
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const bool primary = item.CtlID == openFileId || item.CtlID == playId;
    const COLORREF fill = enabled && primary ? accent : pressed ? border : surface;
    const auto brush = CreateSolidBrush(fill);
    const auto pen = CreatePen(PS_SOLID, px(1), enabled && primary ? accent : border);
    const auto oldBrush = SelectObject(item.hDC, brush);
    const auto oldPen = SelectObject(item.hDC, pen);
    const auto oldFont = SelectObject(item.hDC, g_fonts[0]);
    FillRect(item.hDC, &item.rcItem, g_surfaceBrush);
    RoundRect(item.hDC, item.rcItem.left, item.rcItem.top, item.rcItem.right, item.rcItem.bottom, px(12), px(12));
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, !enabled ? muted : primary ? background : textColor);
    wchar_t text[80]{};
    GetWindowTextW(item.hwndItem, text, 80);
    RECT content = item.rcItem;
    if (pressed) OffsetRect(&content, px(1), px(1));
    DrawTextW(item.hDC, text, -1, &content, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (item.itemState & ODS_FOCUS) {
        RECT focus = item.rcItem;
        InflateRect(&focus, -px(5), -px(5));
        SetTextColor(item.hDC, primary ? background : accent);
        SetBkColor(item.hDC, fill);
        DrawFocusRect(item.hDC, &focus);
    }
    SelectObject(item.hDC, oldFont);
    SelectObject(item.hDC, oldPen);
    SelectObject(item.hDC, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}
LRESULT drawSlider(NMCUSTOMDRAW& draw) {
    if (draw.dwDrawStage == CDDS_PREPAINT) {
        RECT client{};
        GetClientRect(draw.hdr.hwndFrom, &client);
        FillRect(draw.hdc, &client, g_surfaceBrush);
        return CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
    }
    if (draw.dwDrawStage == CDDS_ITEMPREPAINT) {
        if (draw.dwItemSpec == TBCD_CHANNEL || draw.dwItemSpec == TBCD_THUMB) {
            RECT rect = draw.rc;
            if (draw.dwItemSpec == TBCD_CHANNEL) {
                const int center = (rect.top + rect.bottom) / 2;
                rect.top = center - px(2);
                rect.bottom = center + px(2);
            }
            const auto brush = CreateSolidBrush(draw.dwItemSpec == TBCD_THUMB ? accent : border);
            FillRect(draw.hdc, &rect, brush);
            DeleteObject(brush);
            return CDRF_SKIPDEFAULT;
        }
    }
    if (draw.dwDrawStage == CDDS_POSTPAINT && GetFocus() == draw.hdr.hwndFrom) {
        RECT rect{};
        GetClientRect(draw.hdr.hwndFrom, &rect);
        InflateRect(&rect, -px(2), -px(2));
        SetTextColor(draw.hdc, accent);
        SetBkColor(draw.hdc, surface);
        DrawFocusRect(draw.hdc, &rect);
    }
    return CDRF_DODEFAULT;
}
LRESULT CALLBACK wndProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE: {
        g_window = window;
        g_dpi = windowDpi(window);
        createFonts();
        try {
            createControls();
        } catch (const std::exception& error) {
            MessageBoxW(window, widen(error.what()).c_str(), L"SubON could not start", MB_OK | MB_ICONERROR);
            return -1;
        }
        if (!SetTimer(window, stateTimer, 80, nullptr)) {
            MessageBoxW(window, L"Could not start playback status updates.", L"SubON could not start", MB_OK | MB_ICONERROR);
            return -1;
        }
        return 0;
    }
    case WM_SIZE: resizeLayout(); return 0;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
        info->ptMinTrackSize.x = px(520);
        info->ptMinTrackSize.y = px(400);
        return 0;
    }
    case WM_DPICHANGED: {
        const int oldScroll = logical(g_scroll);
        g_dpi = HIWORD(wparam);
        g_scroll = px(oldScroll);
        createFonts();
        const auto* rect = reinterpret_cast<const RECT*>(lparam);
        SetWindowPos(window, nullptr, rect->left, rect->top, rect->right - rect->left,
            rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
        resizeLayout();
        return 0;
    }
    case WM_VSCROLL: {
        SCROLLINFO info{sizeof(info)};
        info.fMask = SIF_ALL;
        GetScrollInfo(window, SB_VERT, &info);
        int position = g_scroll;
        switch (LOWORD(wparam)) {
        case SB_LINEUP: position -= px(36); break;
        case SB_LINEDOWN: position += px(36); break;
        case SB_PAGEUP: position -= g_pageHeight - px(36); break;
        case SB_PAGEDOWN: position += g_pageHeight - px(36); break;
        case SB_THUMBTRACK: case SB_THUMBPOSITION: position = info.nTrackPos; break;
        case SB_TOP: position = 0; break;
        case SB_BOTTOM: position = px(contentHeight); break;
        default: break;
        }
        setScroll(position);
        return 0;
    }
    case WM_MOUSEWHEEL: wheelScroll(wparam); return 0;
    case WM_KEYDOWN:
        if (wparam == VK_NEXT) setScroll(g_scroll + g_pageHeight - px(36));
        else if (wparam == VK_PRIOR) setScroll(g_scroll - g_pageHeight + px(36));
        else if (wparam == VK_HOME) setScroll(0);
        else if (wparam == VK_END) setScroll(px(contentHeight));
        return 0;
    case WM_HSCROLL: snapshotControls(); return 0;
    case WM_TIMER:
        if (wparam == stateTimer) {
            updateState();
            updateLoading();
        }
        return 0;
    case WM_COMMAND:
        if (HIWORD(wparam) != BN_CLICKED) break;
        switch (LOWORD(wparam)) {
        case openFileId: openFile(); return 0;
        case playId: startPlayback(); return 0;
        case stopId:
            if (g_loading) cancelLoading();
            else { stopPlayback(); SetFocus(g_play); }
            return 0;
        case flatId: preset(0); return 0;
        case bassPresetId: preset(1); return 0;
        case vocalId: preset(2); return 0;
        case limiterId: snapshotControls(); return 0;
        default: break;
        }
        break;
    case WM_NOTIFY: {
        auto* header = reinterpret_cast<NMHDR*>(lparam);
        if (header->code == NM_CUSTOMDRAW &&
            (header->idFrom == preampId || header->idFrom == bassId || header->idFrom == trebleId ||
             (header->idFrom >= eqFirstId && header->idFrom < eqFirstId + g_eq.size())))
            return drawSlider(*reinterpret_cast<NMCUSTOMDRAW*>(lparam));
        break;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        const auto child = reinterpret_cast<HWND>(lparam);
        const auto* item = layoutFor(child);
        const bool onSurface = !item || item->onSurface;
        const auto dc = reinterpret_cast<HDC>(wparam);
        SetBkColor(dc, onSurface ? surface : background);
        SetTextColor(dc, child == g_status && g_statusError ? errorColor : item && item->secondary ? muted : textColor);
        return reinterpret_cast<LRESULT>(onSurface ? g_surfaceBrush : g_backgroundBrush);
    }
    case WM_DRAWITEM: drawButton(*reinterpret_cast<DRAWITEMSTRUCT*>(lparam)); return TRUE;
    case WM_ERASEBKGND: return TRUE;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        FillRect(dc, &paint.rcPaint, g_backgroundBrush);
        paintCard(dc, 202, 500);
        paintCard(dc, 510, 1406);
        paintCard(dc, 1416, 1684);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_CLOSE:
        if (g_closing) return 0;
        g_closing = true;
        stopPlayback();
        if (g_loading) {
            // Keep the window and timer alive until cancellation finishes. A
            // slow disk read must not freeze the UI or outlive its owning thread.
            cancelLoading();
            return 0;
        }
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        // Also handles destruction without WM_CLOSE (including failed creation).
        KillTimer(window, stateTimer);
        g_stop.store(true);
        g_cancelLoad.store(true);
        if (g_worker.joinable()) g_worker.join();
        if (g_loader.joinable()) g_loader.join();
        PostQuitMessage(0);
        return 0;
    default: break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
void enableDpiAwareness() {
    const HMODULE user32 = GetModuleHandleW(L"user32.dll");
    using SetContext = BOOL (WINAPI*)(HANDLE);
    const auto setContext = reinterpret_cast<SetContext>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
    if (setContext && setContext(reinterpret_cast<HANDLE>(static_cast<INT_PTR>(-4)))) return;
    SetProcessDPIAware();
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    enableDpiAwareness();
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_BAR_CLASSES};
    if (!InitCommonControlsEx(&controls)) return 1;
    g_backgroundBrush = CreateSolidBrush(background);
    g_surfaceBrush = CreateSolidBrush(surface);
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = wndProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = L"SubONDesktop";
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = g_backgroundBrush;
    if (!RegisterClassW(&windowClass)) return 1;
    HDC dc = GetDC(nullptr);
    g_dpi = static_cast<UINT>(GetDeviceCaps(dc, LOGPIXELSX));
    ReleaseDC(nullptr, dc);
    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    const int height = std::min(px(920), static_cast<int>(workArea.bottom - workArea.top));
    HWND window = CreateWindowExW(WS_EX_CONTROLPARENT, windowClass.lpszClassName,
        L"SubON - Shape your sound", WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, px(700), height, nullptr, nullptr, instance, nullptr);
    int result = 1;
    if (window) {
        ShowWindow(window, show);
        UpdateWindow(window);
        SetFocus(GetDlgItem(window, openFileId));
        MSG message{};
        BOOL received{};
        while ((received = GetMessageW(&message, nullptr, 0, 0)) > 0) {
            if (!IsDialogMessageW(window, &message)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }
        result = received == -1 ? 1 : static_cast<int>(message.wParam);
        // GetMessage failure must not leave a joinable thread at static teardown.
        if (IsWindow(window)) DestroyWindow(window);
    }
    for (auto font : g_fonts) if (font) DeleteObject(font);
    if (g_backgroundBrush) DeleteObject(g_backgroundBrush);
    if (g_surfaceBrush) DeleteObject(g_surfaceBrush);
    return result;
}
