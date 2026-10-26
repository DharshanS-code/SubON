# SubON

SubON is an experimental Android audio-filter app built with Kotlin and Jetpack Compose.

## Project structure

- `app/src/main/AndroidManifest.xml` — Android manifest and service declarations
- `app/src/main/java/app/subon/` — Kotlin application, UI, filter math, preferences, and audio engine
- `app/src/main/res/` — app theme, strings, colors, and launcher assets
- `app/build.gradle.kts` — Android application module and dependencies
- `.github/workflows/build-debug.yml` — builds a debug APK on pull requests and pushes to `main`
- `.github/workflows/release.yml` — builds and attaches a debug APK to a GitHub Release when a `v*` tag is pushed

## Build requirements

- JDK 17
- Android SDK platform 34
- Gradle 8.2 (the GitHub Actions workflows install Gradle; this repository does not currently include the Gradle wrapper)

From the repository root, run:

```sh
gradle assembleDebug
```

The APK is expected at `app/build/outputs/apk/debug/app-debug.apk`.

## Important limitations

SubON uses Android's `DynamicsProcessing` audio effect. Whether it can attach to the global output mix or individual app audio sessions depends on the Android version, device firmware, and audio-effect permissions. A successful build does not guarantee system-wide audio filtering on every device.

This project has not yet been verified by a successful CI build. Treat it as experimental until the workflow passes and device behavior is tested.

## Releases

Published releases appear on the [GitHub Releases page](https://github.com/DharshanS-code/SubON/releases). The current workflow publishes a **debug APK**, not a signed production release.

## Contributing

Please read [CONTRIBUTING.md](CONTRIBUTING.md) before proposing changes.

## License

No license has been selected for this repository. Until one is added, do not assume that reuse, modification, or redistribution is permitted.
