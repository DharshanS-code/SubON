# Contributing to SubON

Thanks for your interest in contributing.

## Before opening a pull request

1. Explain the problem or improvement in the PR description.
2. Keep changes focused and avoid unrelated formatting changes.
3. Never commit passwords, API tokens, signing keys, keystores, or local SDK paths.
4. Include screenshots or device details for UI or Android-device-specific changes.

## Build and test

Android source is in `app/`; native Windows/DSP source is in `windows-engine/`. Android requires JDK 17, Gradle 8.2 and Android SDK 34. Portable tests require CMake 3.20+ and C++17. Windows builds additionally require Visual Studio C++ tools and the Windows SDK.

```sh
# Android: unit regressions, lint and debug APK
./gradlew :app:testDebugUnitTest :app:lintDebug :app:assembleDebug

# Portable native checks; install Ninja first
cmake -S windows-engine -B windows-engine/build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build windows-engine/build
ctest --test-dir windows-engine/build --output-on-failure
```

For Windows, use `-A x64` instead of `-G Ninja`, and `--config Release` / `-C Release` for build/CTest. Also test Debug. Assertions must remain effective in Release tests.

Keep Android lifecycle work on the service's designated dispatcher. Keep allocation, blocking, locks and I/O out of APO processing callbacks. Add regressions for malformed settings/audio files and lifecycle failures; never silently bypass a failed processor.

Include exact commands and results in your PR. Distinguish portable unit tests, platform compilation, UI interactions and measured audio behavior. Check narrow/wide layouts, enlarged fonts, keyboard navigation and TalkBack/Narrator. See [the shared design contract](docs/design-system.md).

CI artifacts are debug/prototype builds, not signed production releases. Keep signing keys and local SDK paths out of Git. APO endpoint packaging, installation and real-device validation remain a separate unfinished integration.

## Reporting a bug

Include the Android version, device model, steps to reproduce, expected behavior, actual behavior, and relevant non-sensitive logs.
