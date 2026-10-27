# SubON Windows Audio Engine

This directory contains a **standalone C++ DSP core** intended to be built separately from the Kotlin/Compose application. It provides a low-pass/high-pass biquad chain and a smoke test.

## Important status

This is **not yet a working system-wide Windows audio effect**. The DSP library processes sample values passed to it; it does not capture or intercept audio from other applications. The Windows system-wide integration layer is a separate Windows Audio Processing Object (APO) component and is not implemented in this initial scaffold.

Windows system effects are integrated into the Windows audio engine through APOs and typically require COM interfaces, packaging/registration, device compatibility testing, and an installation strategy. Read Microsoft's [APO architecture documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/audio-processing-object-architecture) before implementing that layer.

## Build the DSP core

Requirements: CMake 3.20+ and a C++17 compiler. On Windows, use a Visual Studio C++ build environment.

From this directory:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The library target is `subon_dsp`; the test target is `subon_dsp_smoke_test`.

## Design boundary

- **Kotlin/Compose UI:** shared configuration and controls (as the CMP migration proceeds).
- **Android engine:** remains in the Android source set and continues using Android audio APIs.
- **Windows DSP core:** this C++ library; built by CMake, not by the Android Gradle task.
- **Future Windows APO adapter:** native Windows integration that will call the DSP core from the supported real-time processing callbacks.
- **Desktop app:** a separate Compose Desktop executable that edits settings and communicates with the Windows integration layer once a safe, tested settings bridge exists.

Do not load UI code, allocate memory, perform file/network I/O, or acquire blocking locks from a real-time audio callback. Any settings bridge must use bounded, real-time-safe data transfer and validate all parameters.

## Current limitations

- No APO COM adapter or system effect registration is included.
- No system-wide audio capture or routing is included.
- No installer or driver package is included.
- The filter chain is a basic DSP primitive and has not yet been tuned or validated for production audio.
- This target must not be described as system-wide until the APO integration is implemented, packaged, installed, and tested on supported Windows versions and devices.
