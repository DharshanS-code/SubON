<div align="center">

# 🔊 SubON

### TAKE CONTROL OF YOUR LOW END.

**An experimental Android audio-filter application focused on low-frequency control.**

Tune your frequency range. Shape your sound. Explore your audio.

![Platform](https://img.shields.io/badge/Platform-Android-3DDC84?style=for-the-badge&logo=android&logoColor=white)
![Language](https://img.shields.io/badge/Kotlin-1.9.22-7F52FF?style=for-the-badge&logo=kotlin&logoColor=white)
![UI](https://img.shields.io/badge/UI-Jetpack%20Compose-4285F4?style=for-the-badge&logo=jetpackcompose&logoColor=white)
![Version](https://img.shields.io/badge/Version-0.1.0-CAFF70?style=for-the-badge)
![Status](https://img.shields.io/badge/Status-Experimental-orange?style=for-the-badge)

**[Repository](https://github.com/DharshanS-code/SubON) · [Releases](https://github.com/DharshanS-code/SubON/releases) · [Report a Bug](https://github.com/DharshanS-code/SubON/issues)**

</div>

---

## 📖 Table of Contents

- [About SubON](#-about-subon)
- [Why SubON?](#-why-subon)
- [Features](#-features)
- [How It Works](#-how-it-works)
- [Output Profiles](#-output-profiles)
- [Filter Presets](#-filter-presets)
- [Technology Stack](#-technology-stack)
- [Project Structure](#-project-structure)
- [Installation](#-installation)
- [Build From Source](#-build-from-source)
- [Compatibility and Limitations](#-compatibility-and-limitations)
- [Roadmap](#-roadmap)
- [Contributing](#-contributing)
- [Troubleshooting](#-troubleshooting)
- [Version History](#-version-history)
- [License](#-license)

---

## 🎧 About SubON

**SubON** is an experimental Android audio-filter application built with Kotlin and Jetpack Compose.

The project explores low-frequency audio filtering through an interactive interface. Users can configure lower and upper frequency boundaries, select filter steepness, adjust output gain, toggle the limiter setting, and choose an output profile.

A custom frequency-response graph visualizes the calculated response for the selected configuration.

SubON uses Android's `DynamicsProcessing` audio effect. Actual effect attachment and audible results depend on Android's audio framework, device implementation, and available audio sessions.

### Project information

| Detail | Information |
|---|---|
| Application | SubON |
| Current project version | `0.1.0` |
| Version code | `1` |
| Platform | Android |
| Language | Kotlin |
| UI framework | Jetpack Compose |
| Audio API | Android `DynamicsProcessing` |
| Minimum Android API | API 29 — Android 10 |
| Compile SDK | Android API 34 |
| Target SDK | Android API 34 |
| Development status | Experimental |
| Repository | `DharshanS-code/SubON` |

> **Important:** A successful build does not guarantee that the audio effect will attach to every device's output or produce the same audible result on every Android version.

---

## 💡 Why SubON?

Phone speakers, wired headphones, Bluetooth headsets, and dedicated subwoofers have different frequency-response characteristics. Hardware limits, Android system processing, and each playback application's audio path also influence the final sound.

SubON provides a configurable interface for experimenting with low-frequency filtering.

The project focuses on three principles:

- **Control:** Adjust the frequency range and filter steepness instead of relying on one fixed configuration.
- **Visibility:** View a calculated frequency-response graph while changing settings.
- **Simplicity:** Keep the principal controls together in a compact Android interface.

SubON is not a replacement for a calibrated measurement system, dedicated hardware equalizer, or professional digital signal processor. It is an experimental software project exploring accessible audio-filter controls on Android.

---

## ✨ Features

### 🎚️ Adjustable frequency range

Configure the low and high boundaries using an interactive range slider.

- Low edge: **20–100 Hz**
- High edge: **40–300 Hz**
- Minimum separation between boundaries
- Frequency values shown in hertz
- Graph updates to reflect the selected configuration

The selected range determines how the calculated filter attenuates frequencies below and above the desired region. The application translates the configuration into audio-processing band gains.

**A software filter cannot make a physical speaker reproduce frequencies it is unable to produce.**

### 📈 Frequency-response visualization

SubON includes a custom-drawn response graph featuring:

- A logarithmic frequency axis
- Frequency reference markers
- Decibel reference lines
- A calculated response curve
- Markers for the selected low and high edges
- Shading beneath the response curve

The graph represents the application's calculated filter response. It is not a live acoustic measurement of your speaker, headphones, room, or subwoofer.

### 🎛️ Filter steepness

Choose from three settings:

| Setting | Internal order | Intended behavior |
|---|---:|---|
| Gentle | 2 | Gentler transition |
| Steep | 6 | Sharper transition |
| Max | 16 | Very sharp transition |

Changing the setting changes the mathematical response used by the application. Audible behavior may vary with the Android effect and playback device.

### 🔉 Output gain

Adjust the configured output gain from **−12 dB to +6 dB**.

Increasing gain can cause clipping, distortion, or excessive playback volume depending on the signal and device. Higher gain does not guarantee deeper bass or better sound quality. Adjust volume carefully.

### 🛡️ Limiter setting

The limiter toggle configures the limiter component of Android's `DynamicsProcessing` effect.

Its intended purpose is to help manage excessive signal levels. Limiter behavior depends on the underlying Android implementation; do not assume it prevents every form of clipping, distortion, or unsafe listening level.

### 🎧 Output profiles

Choose a starting profile for your intended playback device:

- Phone speaker
- Wired headphones
- Bluetooth
- Subwoofer

Each profile has its own default frequency boundaries and saved configuration. Profiles do not automatically identify connected hardware, measure its frequency response, or guarantee that a device can reproduce the selected range.

### 🎚️ Quick presets

Use the included presets to change frequency boundaries quickly.

| Preset | Low edge | High edge |
|---|---:|---:|
| True sub | 25 Hz | 60 Hz |
| Sub + bass | 30 Hz | 120 Hz |
| Small speaker | 100 Hz | 200 Hz |
| Custom | User-configurable | User-configurable |

Presets are starting configurations, not guarantees of a particular sound signature.

### 💾 Saved preferences

SubON uses Android's local `SharedPreferences` storage to retain:

- Selected output profile
- Low and high frequency boundaries
- Filter steepness
- Limiter preference
- Output-gain setting

Settings are stored locally on the device. The current implementation does not require a user account or remote database to save these preferences.

### ⚡ Audio-effect status

The interface can display engine states such as:

- **Off** — The filter is not active.
- **Attached** — An audio effect has been attached.
- **Waiting** — A waiting state is reported by the engine.
- **Failed** — The effect could not be attached successfully.

The exact result depends on Android and the device implementation. An attached effect does not necessarily mean that every sound from every application is being processed.

### 🌙 Modern Android interface

The Jetpack Compose interface includes a custom SubON logo mark, dark theme, contrasting accent colors, rounded controls, status indicators, interactive sliders and switches, and a custom frequency-response visualization.

---

## ⚙️ How It Works

SubON has three main logical components: the user interface, filter calculations, and Android audio-effect engine.

### 1. Configuration

The user selects a frequency range, steepness, gain, limiter preference, and output profile. These values are represented by the `FilterConfig` model.

### 2. Response calculation

`FilterMath` calculates the expected response for the selected configuration and generates band-gain values used when building the audio-effect configuration.

### 3. Audio-effect configuration

`SubOnEngine` uses Android's `DynamicsProcessing` API to create and configure an audio effect. It attempts different band counts when creating the effect. Successful attachment depends on device support and audio-effect availability.

### 4. Persistent settings

`Prefs` stores configuration settings locally so the application can restore them later.

### Simplified architecture

```text
┌─────────────────────────────┐
│        SubON Interface      │
│     Jetpack Compose UI      │
└──────────────┬──────────────┘
               │
               ▼
┌─────────────────────────────┐
│      FilterConfig Model     │
│ Range • Gain • Steepness    │
│ Profile • Limiter           │
└──────────────┬──────────────┘
               │
       ┌───────┴────────┐
       ▼                ▼
┌──────────────┐  ┌────────────────┐
│  FilterMath  │  │      Prefs     │
│ Response &   │  │ Local Settings │
│ Band Gains   │  │   Persistence  │
└──────┬───────┘  └────────────────┘
       │
       ▼
┌─────────────────────────────┐
│        SubOnEngine          │
│ Android DynamicsProcessing  │
└──────────────┬──────────────┘
               ▼
┌─────────────────────────────┐
│ Android Audio Effect System │
│ Device / Session Dependent  │
└─────────────────────────────┘
```

This diagram describes the logical components. It does not imply guaranteed system-wide audio processing.

---

## 🎧 Output Profiles

The application's current profile defaults are:

| Profile | Default low edge | Default high edge |
|---|---:|---:|
| Phone speaker | 100 Hz | 200 Hz |
| Wired headphones | 30 Hz | 120 Hz |
| Bluetooth | 50 Hz | 150 Hz |
| Subwoofer | 25 Hz | 80 Hz |

These values are starting configurations defined in the application, not measurements or specifications of your actual hardware.

- **Phone speaker:** Many phone speakers have limited bass extension. Frequencies below their capabilities may produce little audible output.
- **Wired headphones:** Useful frequency response varies considerably between models.
- **Bluetooth:** Connected devices may apply their own processing, equalization, and volume management.
- **Subwoofer:** Useful range depends on the model, enclosure, amplifier, and setup.

Start at a comfortable listening volume and adjust settings gradually.

---

## 🧰 Technology Stack

| Technology | Purpose |
|---|---|
| Kotlin | Primary programming language |
| Jetpack Compose | Declarative Android UI |
| Material 3 | UI components |
| Android SDK | Native Android APIs |
| `DynamicsProcessing` | Audio-effect configuration |
| Kotlin Coroutines | Coroutine support dependency |
| SharedPreferences | Local configuration storage |
| Gradle | Build automation |
| GitHub Actions | Automated build workflow |

### Android configuration

- **Minimum SDK:** 29
- **Compile SDK:** 34
- **Target SDK:** 34
- **Java compatibility:** 17
- **Kotlin JVM target:** 17
- **Compose compiler extension:** 1.5.10
- **Android Gradle Plugin:** 8.2.2
- **Kotlin plugin:** 1.9.22

---

## 📂 Project Structure

```text
SubON/
├── .github/
│   └── workflows/
│       ├── build-debug.yml
│       └── release.yml
├── app/
│   ├── build.gradle.kts
│   └── src/
│       └── main/
│           ├── AndroidManifest.xml
│           ├── java/app/subon/
│           │   ├── MainActivity.kt
│           │   ├── Model.kt
│           │   ├── FilterMath.kt
│           │   ├── Prefs.kt
│           │   ├── SubOnEngine.kt
│           │   ├── SubOnService.kt
│           │   ├── Theme.kt
│           │   └── Ui.kt
│           └── res/
├── build.gradle.kts
├── gradle.properties
├── settings.gradle.kts
└── README.md
```

### Key files

| File | Responsibility |
|---|---|
| `MainActivity.kt` | Activity entry point and UI callbacks |
| `Ui.kt` | Main screen, sliders, presets, graph, and controls |
| `Model.kt` | Filter configuration, presets, profiles, and status models |
| `FilterMath.kt` | Response calculations and band-gain calculations |
| `Prefs.kt` | Saving and restoring settings |
| `SubOnEngine.kt` | Creating, configuring, and releasing audio effects |
| `SubOnService.kt` | Service component used to manage audio-effect operation |
| `Theme.kt` | Application colors and theme |
| `AndroidManifest.xml` | Application metadata, permissions, and components |
| `build-debug.yml` | Automated debug-build workflow |
| `release.yml` | Build-and-release workflow |

---

## 📲 Installation

### Option 1: Download a release

1. Open the [SubON Releases page](https://github.com/DharshanS-code/SubON/releases).
2. Select a release.
3. Download the APK asset if one is available.
4. Open the downloaded APK on your Android device.
5. Review Android's installation prompt and proceed only if you trust the source.

Android may require permission to install applications from the app used to download the APK. Only install APKs from trusted sources.

**The current release workflow is configured to publish a debug APK.** A debug build is intended for development and testing, not as a signed production release.

If no APK is attached to a release, build the project from source.

---

## 🛠️ Build From Source

### Requirements

- JDK 17
- Android SDK platform 34
- Gradle 8.2
- Compatible Android SDK build tools
- Git, if cloning the repository

The repository currently does not include the Gradle wrapper, so these commands require an installed Gradle distribution.

### Step 1 — Clone the repository

```bash
git clone https://github.com/DharshanS-code/SubON.git
cd SubON
```

### Step 2 — Check Java and Gradle

```bash
java -version
gradle --version
```

Confirm that Java 17 and Gradle 8.2 are available.

### Step 3 — Build the debug APK

From the repository root, run:

```bash
gradle assembleDebug
```

Gradle resolves dependencies and builds the Android application.

### Step 4 — Find the APK

After a successful build, the APK is expected at:

```text
app/build/outputs/apk/debug/app-debug.apk
```

Transfer the APK to a compatible Android device for installation and testing.

### Build troubleshooting

**Gradle cannot find Java:** Check that JDK 17 is installed and that `JAVA_HOME` points to the correct installation.

**Android SDK not found:** Configure the Android SDK location in your development environment or local Gradle configuration.

**Dependency download fails:** Check your internet connection and retry. Dependency repositories may be temporarily unavailable.

**The APK builds, but audio processing fails:** A successful compilation only confirms that the source was built. Audio-effect availability is controlled by Android and the device implementation.

---

## ⚠️ Compatibility and Limitations

SubON is experimental. Keep the following limitations in mind.

### System-wide processing is not guaranteed

The application attempts to attach an Android `DynamicsProcessing` effect to the global output mix. Android versions, manufacturers, firmware, and audio-effect policies may prevent this attachment.

The engine also contains logic for individual audio sessions, but it cannot necessarily attach to every application's audio session.

**Do not assume SubON filters every sound on the phone.**

### Hardware limitations remain

A software filter cannot make a small speaker physically reproduce deep sub-bass at high output levels. Profiles do not change the physical capabilities of the playback device.

### The graph is calculated

The response graph displays the application's calculated filter response. It does not measure the acoustic response of a speaker, headphone, room, or subwoofer.

### Audio-effect behavior varies

Android's `DynamicsProcessing` implementation may behave differently between devices. Some devices may restrict effect attachment, use different audio paths, or apply additional system processing.

### Debug build, not production release

The release workflow builds a debug APK and does not configure a dedicated production-signing process. A successful debug build is not proof of production readiness.

### Early development

Expect potential compatibility issues, incomplete behavior, and implementation changes. Testing on real devices is necessary to understand practical behavior.

---

## 🗺️ Roadmap

The following are possible development directions, not promises that these features are already implemented.

### Core stability
- [ ] Improve audio-effect attachment reliability.
- [ ] Test across different Android versions.
- [ ] Investigate device-specific audio-session restrictions.
- [ ] Improve error reporting and recovery.
- [ ] Expand testing of configuration changes.

### Audio controls
- [ ] Evaluate whether additional filter controls are practical.
- [ ] Review gain and limiter behavior across devices.
- [ ] Improve explanations of frequency ranges.
- [ ] Investigate additional configuration options based on feedback.

### User experience
- [ ] Refine layout and accessibility.
- [ ] Improve status messages when an effect cannot attach.
- [ ] Add clearer guidance for output profiles.
- [ ] Continue improving the frequency graph.

### Compatibility and distribution
- [ ] Test on more physical Android devices.
- [ ] Improve automated build and release verification.
- [ ] Evaluate a dedicated signed release process.
- [ ] Document verified device compatibility.

Roadmap items may change as the project develops and technical limitations become clearer.

---

## 🧪 Testing and Feedback

Testing on real Android devices is important for evaluating SubON's practical behavior.

When reporting an issue, include as much of the following information as possible:

- Device manufacturer and model
- Android version
- SubON version
- Whether the APK installed successfully
- Audio-engine status shown by the app
- Selected output profile and frequency range
- Playback device type
- Application playing audio when the issue occurred
- Steps to reproduce the issue

Do not include private information, account credentials, or personal files in bug reports.

### Report a bug

Open the [GitHub Issues page](https://github.com/DharshanS-code/SubON/issues) and describe the problem. Clear reproduction steps make technical issues easier to investigate.

---

## 🤝 Contributing

Contributions, testing feedback, and thoughtful suggestions are welcome.

You can contribute by:

- Reporting reproducible bugs.
- Testing on different Android devices.
- Improving documentation.
- Reviewing filter calculations.
- Suggesting usability improvements.
- Submitting focused code changes.

### Suggested workflow

1. Fork the repository.
2. Create a branch for your change.
3. Make a focused modification.
4. Build the application.
5. Test the changed behavior where possible.
6. Open a pull request describing the change and its purpose.

For audio-engine changes, distinguish between behavior verified in code, behavior verified by compilation, and behavior confirmed through actual device testing.

---

## 🐛 Troubleshooting

### SubON shows “Failed”

The audio effect may have been blocked, unavailable, or unable to obtain control. Try restarting the application. If the issue persists, report your device model, Android version, and the displayed status message.

### The filter is attached, but sound does not change

An attached effect does not guarantee that all playback is routed through it. The playback application's audio session, Android's audio framework, and manufacturer-specific processing may affect the result.

### The selected range produces little sound

The playback device may not reproduce the selected frequencies effectively. Try a range more suitable for the device; profile values are suggestions, not hardware measurements.

### Settings do not behave as expected

Check the selected output profile. SubON stores settings by profile, so switching profiles can load a different set of values.

### The application will not install

Confirm that the APK download completed successfully and that the Android version meets the application's minimum SDK requirement. Only install APKs from trusted sources.

---

## 📦 Version History

### v0.1.0 — Initial Development Release

The first development version establishes the Android application structure and core experimental audio-filter interface.

**Project features include:**

- Kotlin-based Android application
- Jetpack Compose interface
- Adjustable low and high frequency boundaries
- Calculated frequency-response visualization
- Filter-steepness settings
- Output-gain control
- Limiter configuration
- Output profiles and presets
- Local preference storage
- Android `DynamicsProcessing` integration
- Automated debug-build workflow
- GitHub Actions release workflow

**Release status:** Experimental.

The release workflow is configured to attach a debug APK to a GitHub Release when triggered by a version tag or manual workflow dispatch. Check the Releases page for an actual published artifact.

---

## 📄 License

**SubON is licensed under the GNU General Public License v3.0 (GPL-3.0).**

You are free to use, study, modify, and redistribute this software under the terms of the license.

If you distribute copies or modified versions of SubON, you must comply with the applicable GPL-3.0 requirements, including providing the corresponding source code and preserving required notices.

See the [LISENCE](https://github.com/DharshanS-code/SubON/LISENCE ). file for the complete license terms.

SPDX-License-Identifier: GPL-3.0-only

---

## 🙌 Acknowledgements

SubON is built on the Android development ecosystem, including Kotlin, Android SDK, Jetpack Compose, Material 3, Android audio-effect APIs, Gradle, and GitHub Actions.

Thanks to everyone who tests experimental software, reports bugs, and contributes constructive feedback.

---

<div align="center">

## 🔊 SUBON

### Your sound. Your frequencies. Your control.

An experimental project exploring configurable low-frequency audio filtering on Android.

**[⭐ Star the Repository](https://github.com/DharshanS-code/SubON) · [📦 Releases](https://github.com/DharshanS-code/SubON/releases) · [🐛 Issues](https://github.com/DharshanS-code/SubON/issues)**

<sub>Built for Android. Evolving through experimentation.</sub>

</div>
