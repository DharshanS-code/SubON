# Windows System-Wide Audio: APO Architecture Decision

Status: Proposed implementation direction  
Related issue: #8  
Scope: Windows desktop audio path for SubON v1.x

## Decision

Use a Windows Audio Processing Object (APO) as the first native integration path, reusing SubON's existing C++ DSP core. Start with a render endpoint effect (EFX) or another placement justified by a small Windows test harness; do not assume one placement catches every audio mode or every application.

Microsoft describes APOs as real-time, in-process COM components inserted into the Windows audio engine's processing graph. They are a natural fit for EQ, gain/preamp, and limiting without requiring SubON to capture and replay every app's audio itself.

Official references:
- APO architecture: https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/audio-processing-object-architecture
- APO implementation and packaging: https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/implementing-audio-processing-objects
- Windows 11 APO APIs: https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/windows-11-apis-for-audio-processing-objects
- Microsoft SYSVAD audio driver sample: https://github.com/microsoft/Windows-driver-samples/tree/main/audio/sysvad

## Important constraints

1. APO registration is associated with audio-device driver packages/endpoints; this is not a simple global DLL that automatically attaches to every device.
2. Processing-mode behavior matters. Some paths, including RAW processing modes, may not load certain APO placements. Validate ordinary browser/media/game audio and document unsupported paths instead of claiming universal coverage.
3. The APO processing callback is real-time code. It must avoid blocking, allocation, file I/O, locks, UI work, and other operations that can stall the audio thread.
4. Keep UI/control communication separate from the real-time callback. Send immutable, validated parameter snapshots to the APO and apply them safely at audio-block boundaries.
5. Installation needs a proper driver/INF package and appropriate signing/distribution. Do not ask users to disable Windows security or install an unsigned driver.
6. Make the change reversible: document the selected playback endpoint, provide an explicit disable/uninstall path, and test recovery to normal Windows audio.
7. Device-driver and OEM interactions can vary. Never overwrite an existing vendor APO blindly; investigate extension/componentized packaging and coexistence before installation.

## Proposed staged implementation

### Stage 1 — Buildable APO skeleton
- Add a Windows-only APO DLL project using the Windows SDK's audio APO interfaces and the Microsoft sample as a reference.
- Implement required COM/APO interfaces and format negotiation.
- Initially pass audio through unchanged.
- Keep the APO target separate from the current WAV demo and portable DSP tests.
- Add CI compile checks on a Windows runner; CI compilation alone does not prove it can be installed or loaded on a real endpoint.

### Stage 2 — Connect the DSP core
- Adapt APO input/output buffers to the existing DSP core without allocation or blocking in the real-time callback.
- Preserve filter state across blocks and avoid resetting DSP state on each callback.
- Use safe parameter snapshots for EQ, bass/treble, preamp, limiter and presets.
- Add deterministic unit tests for bypass, channel/format handling, limiter ceiling and state continuity.

### Stage 3 — Device integration and recovery
- Prepare a properly packaged and signed test installation path; do not bundle an untrusted or unsigned driver package.
- Test on a dedicated Windows test machine with a restore/recovery plan.
- Verify audio from at least two independent apps, multiple playback endpoints, endpoint changes, sleep/resume, and disable/uninstall recovery.
- Check whether the selected APO placement is invoked in the tested signal modes; record exclusions explicitly.
- Only after real-device validation should release notes claim system-wide processing, and claims must match tested coverage.

## Acceptance criteria for closing issue #8

- A documented, supportable installation package and rollback procedure exist.
- EQ, preamp and limiter demonstrably process output from at least two independent Windows applications on a documented endpoint/configuration.
- Normal playback can be restored after disabling or uninstalling SubON.
- CI passes, DSP regression tests pass, and real-device manual test results are recorded.
- Known unsupported modes/endpoints are documented. Do not claim every Windows audio path is covered unless evidence supports that claim.

## Current status

This document records the proposed architecture; it does not mean an APO has been implemented, installed, or tested. The existing Windows WAV player remains an in-app playback prototype until the staged work is completed.
