# SubON Windows Desktop Prototype

This is the first native Windows UI prototype and reuses the C++ DSP library.

## Included
- Native Windows desktop window.
- Bass, treble and preamp controls.
- Limiter toggle and Flat/Bass Boost presets.
- Synthetic-signal DSP preview.
- GitHub Actions build, DSP smoke tests, and downloadable EXE artifact.

## Important limitation
**This prototype does not process audio from Windows applications or system output.** It runs a generated test signal through the DSP processor only. It is not yet a system-wide equalizer or the finished v1.0.0 release.

System-wide processing requires a separate Windows audio integration layer, installation/registration strategy, compatibility tests and a reversible uninstall path.

## Build on Windows
Requirements: Visual Studio C++ Build Tools and CMake 3.20+.

```powershell
cmake -S windows-engine -B windows-engine/build -A x64
cmake --build windows-engine/build --config Release
ctest --test-dir windows-engine/build -C Release --output-on-failure
```

Executable: `windows-engine/build/Release/SubONWindows.exe`.

## Download CI build
Open the repository's Actions tab, choose **Windows Desktop Prototype**, open a successful run, and download artifact `SubON-Windows-x64-prototype`. This is a temporary prototype artifact, not an official release or signed installer.

## Next
1. Add the 10-band EQ UI and persistent profiles.
2. Implement the selected Windows audio integration architecture.
3. Test actual playback on Windows 10/11.
4. Then prepare a combined Android APK + Windows release.
