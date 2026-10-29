# SubON — Android app + Windows audio prototype

SubON is an experimental audio-processing project. The Android app and Windows tools are separate platform implementations sharing configurable audio filtering.

## Windows desktop player

The native C++ desktop player plays WAV files opened inside SubON and applies bass/treble shelves, preamp, 10-band EQ and limiting. Supported WAV inputs are mono/stereo PCM 16/24/32-bit or IEEE float32, 32–192 kHz.

## Experimental virtual-audio router

A separate Windows-only `SubONVirtualRouter.exe` target now prototypes the user-mode half of a FxSound-style virtual soundcard path:

- Find an active playback endpoint named `SubON Virtual Audio`.
- Capture that endpoint using WASAPI loopback.
- Process float32 samples through SubON's existing DSP bridge.
- Render processed samples to the current default physical output.

This is **not yet a complete system-wide audio product**. The router requires a separately installed virtual audio driver that creates the named playback endpoint. This repository does not yet include the kernel-mode driver package, INF, signed installer, endpoint switching/recovery, format conversion, or production buffering/drift correction. The first router accepts only matching float32 endpoint formats and is experimental; do not rely on it as your only audio path.

See [virtual-audio/README.md](virtual-audio/README.md) for the current path and limitations.

## Build on Windows

Install Visual Studio C++ tools and CMake 3.20+.

```powershell
cmake -S windows-engine -B windows-engine/build -A x64
cmake --build windows-engine/build --config Release
ctest --test-dir windows-engine/build -C Release --output-on-failure
```

Executables:
- `windows-engine/build/Release/SubONWindows.exe` — WAV player.
- `windows-engine/build/Release/SubONVirtualRouter.exe` — experimental router host.

## CI artifact

Open the repository Actions tab and choose **Windows Desktop Build and Test**. CI artifacts are temporary test builds, not signed releases.

## Android

The Android application is in `app/` and uses Kotlin, Jetpack Compose and Android audio effects. Effect attachment and audible behavior vary by device and audio session; system-wide effects are not guaranteed.

## v1.0.0 release gate

Do not publish v1.0.0 until both platform builds pass, the virtual driver is built and safely packaged, the router is tested with independent applications on real Windows endpoints, recovery/uninstall is verified, Android builds successfully, and release notes accurately describe the supported audio paths.