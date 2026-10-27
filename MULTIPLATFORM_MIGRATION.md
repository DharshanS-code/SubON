# SubON Compose Multiplatform and Windows System Audio Plan

## Goal

Keep one Kotlin/Compose UI and shared configuration model for Android and Windows, while selecting a platform-specific audio engine at build/runtime. The Android audio implementation must remain Android-only. The Windows system-wide implementation must be native and separate from the Compose Desktop executable.

## Planned layers

```text
shared Compose Multiplatform UI and configuration
             |                         |
       Android target             Desktop JVM target
             |                         |
  Android service/effects       Settings client/bridge
             |                         |
  Android audio APIs          Windows APO (native component)
                                       |
                              Windows audio engine graph
```

## Engine selection

- Android builds include the Android engine and Android service.
- Windows desktop builds include the desktop UI and a settings client only.
- The Windows APO/DSP component is built separately with CMake and a Windows C++ toolchain.
- The APO is not bundled as if it were an ordinary JVM library. It needs its own supported installation, registration, signing/packaging, and compatibility strategy.
- Platform-specific source sets and Gradle dependencies must prevent Android-only classes such as `android.media.audiofx.DynamicsProcessing` from compiling into desktop code.

## Migration phases

1. Add and test a standalone Windows DSP core (initial scaffold in `windows-engine/`).
2. Introduce Kotlin Multiplatform and move pure models/calculations into `commonMain`.
3. Move shared Compose UI/theme into `commonMain`; preserve Android entry point and service in `androidMain`.
4. Add a Compose Desktop entry point that uses a platform-neutral engine/settings interface.
5. Implement and test the native Windows APO adapter using Microsoft's supported APO interfaces.
6. Add a safe configuration bridge between desktop UI and APO; do not perform blocking IPC or disk operations in audio callbacks.
7. Package and test Windows APO integration independently from the desktop `.exe`.
8. Add separate CI jobs for Android APK and Windows native/desktop builds.

## Definition of done for system-wide Windows support

- APO adapter builds against the intended Windows SDK and implements the required interfaces.
- APO installation/registration is explicit and reversible, with appropriate user/admin permissions and a clear uninstall path.
- Audio is processed across the intended Windows output modes and supported endpoints.
- The system remains stable when the APO is unavailable, disabled, or reconfigured.
- Automated DSP tests and real-device integration tests pass.
- Documentation does not claim system-wide processing before those checks pass.

The initial CMake target is only the DSP foundation. It is not a finished APO and does not yet provide system-wide audio processing.
