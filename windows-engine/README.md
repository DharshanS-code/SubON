# SubON — Android app + Windows desktop audio player

SubON is an experimental audio-processing project. The Android app and the Windows desktop player are separate platform implementations that share the goal of configurable audio filtering.

## Windows desktop build

The Windows target is a native C++ desktop application built with CMake and Visual Studio C++ Build Tools. GitHub Actions compiles it on a hosted Windows runner and uploads a temporary EXE artifact.

### Current desktop features
- Open and play supported WAV audio through the Windows audio output.
- Bass and treble shelves, preamp, 10-band EQ, limiter toggle.
- Flat, Bass Boost and Vocal starting presets.
- Native DSP processing using the project's C++ processor.

Supported WAV inputs: mono/stereo, PCM 16/24/32-bit or IEEE float 32-bit, 32–192 kHz. Other formats are not currently supported.

### Important limitation
**The Windows player processes audio played inside SubON only. It does not intercept or modify audio from other applications or the entire Windows system.** System-wide processing is a separate feature requiring a Windows audio integration/driver or APO architecture, installation and uninstallation handling, signing/compatibility work, and testing. Do not treat this build as a system-wide equalizer.

## Build on Windows
Install Visual Studio C++ Build Tools and CMake 3.20+.

```powershell
cmake -S windows-engine -B windows-engine/build -A x64
cmake --build windows-engine/build --config Release
ctest --test-dir windows-engine/build -C Release --output-on-failure
```

The executable is `windows-engine/build/Release/SubONWindows.exe`.

## CI artifact
Open the repository's Actions tab, choose **Windows Desktop Prototype**, open a successful run and download `SubON-Windows-x64-prototype`. CI artifacts expire after 14 days; they are not official signed releases.

## Android
The Android application is in `app/` and uses Kotlin, Jetpack Compose and Android audio effects. Android effect attachment and audible behavior vary by device and audio session; system-wide effects are not guaranteed.

## v1.0.0 release gate
Do not publish v1.0.0 until both platform builds pass, the Windows app has been exercised by automated tests, Android has been built successfully, and release notes accurately describe the supported audio paths.