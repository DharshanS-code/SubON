# SubON — Android app + Windows audio prototype

SubON is an experimental audio-processing project. The Android app and Windows tools are separate platform implementations sharing configurable audio filtering.

## Windows audio integration direction

The primary Windows integration path is now a **Windows Audio Processing Object (APO)** that reuses SubON's C++ DSP core. The portable DSP bridge and float32 format contract are groundwork only: there is not yet a Windows APO DLL registered with a playback endpoint, a supported installer, or verified system-wide processing.

See [the APO implementation notes](apo/README.md) and [the architecture decision](../docs/windows-apo-architecture.md).

The previous WASAPI virtual-audio router remains an experimental prototype only. It is not the primary release path and should not be presented as a ready-to-use system-wide enhancement.

## Windows desktop player

The native C++ desktop player plays WAV files opened inside SubON and applies bass/treble shelves, preamp, 10-band EQ, and limiting. Supported WAV inputs are mono/stereo PCM 16/24/32-bit or IEEE float32, 32–192 kHz.

## Build on Windows

Install Visual Studio C++ tools and CMake 3.20+.

```powershell
cmake -S windows-engine -B windows-engine/build -A x64
cmake --build windows-engine/build --config Release
ctest --test-dir windows-engine/build -C Release --output-on-failure
```

Executables:
- `windows-engine/build/Release/SubONWindows.exe` — WAV player.
- `windows-engine/build/Release/SubONVirtualRouter.exe` — experimental router, if enabled.

## CI artifact

Open the repository Actions tab and choose **Windows Desktop Build and Test**. CI artifacts are temporary test builds, not signed releases.

## Android

The Android application is in `app/` and uses Kotlin, Jetpack Compose, and Android audio effects. Effect attachment and audible behavior vary by device and audio session; system-wide effects are not guaranteed.

## Windows APO release gate

Do not claim Windows system-wide APO processing until the APO DLL is built, correctly integrated with a documented endpoint package, safely installed and removable, and tested with independent applications on real Windows hardware. CI compilation and portable DSP tests alone do not prove endpoint loading or audible processing.
