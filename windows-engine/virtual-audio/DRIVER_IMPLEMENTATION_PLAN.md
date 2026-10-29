# SubON Windows virtual audio driver implementation plan

## Goal

Expose a real Windows playback endpoint named **SubON Virtual Audio** so applications can select it as their output. Audio submitted to that endpoint must be available to the SubON processing path and then reach the selected physical output after DSP.

This is a driver/integration milestone, not a claim that the current user-mode router is already a virtual sound card.

## Current state

- `SubONVirtualRouter.exe` is an experimental WASAPI loopback-capture-to-render prototype.
- It expects an already-installed active render endpoint whose friendly name contains `SubON Virtual Audio`.
- It sends captured float32 audio through the existing DSP bridge and renders to the current default physical output.
- It is not a kernel-mode audio driver, does not create an endpoint, and has not been validated on physical Windows hardware.
- It currently has strict format matching and can drop captured frames when the render buffer has insufficient writable space. It must not be treated as production-ready.

## Recommended driver path

Use Microsoft's Windows Driver Kit (WDK) audio-driver documentation and the SYSVAD sample as the starting reference. Keep Microsoft's sample code and its licensing notices intact; make a separately reviewable SubON adaptation rather than blindly copying a large sample into the app.

References:
- [Virtual Audio Devices](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/virtual-audio-devices)
- [SYSVAD sample](https://github.com/microsoft/Windows-driver-samples/tree/main/audio/sysvad)
- [WDK downloads and setup](https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk)

Important: SYSVAD is a teaching/sample driver, not a ready-made production virtual cable. Its topology and loopback behavior must be examined; do not assume that installing the sample automatically provides a transparent capture-to-physical-output path.

## Implementation gates

### Gate 1 — Driver build and endpoint enumeration
- Create a dedicated WDK solution/project for the virtual endpoint, separate from the portable DSP library and CMake user-mode targets.
- Define a stable endpoint identity and the friendly name expected by the router.
- Build the driver package with the supported Windows SDK/WDK toolchain.
- Verify installation, device enumeration, uninstall, and rollback on a disposable Windows test machine or VM.
- Do not require disabling Secure Boot or weakening the machine's security settings.

**Pass condition:** Windows exposes the intended playback endpoint and it can be selected and removed cleanly.

### Gate 2 — Audio transport contract
- Decide how the driver exposes submitted playback data to the user-mode processing service. Document ownership, synchronization, buffer sizes, format negotiation, and behavior when the service is absent.
- Define the supported formats and negotiate them explicitly. Do not assume the virtual endpoint mix format matches the physical output mix format.
- Ensure audio cannot silently disappear: define bounded-buffer overflow, underflow, service restart, and physical-device-disconnect behavior.
- Prevent feedback loops when the physical output or default endpoint changes.

**Pass condition:** deterministic test audio reaches the user-mode service and can be accounted for across start, stop, underrun, and restart tests.

### Gate 3 — DSP and physical render integration
- Connect the transport to the existing `ApoDspBridge` without allocations, blocking locks, logging, or OS calls inside the real-time processing callback.
- Add a bounded FIFO and explicit sample-rate/channel conversion when endpoint formats differ; measure latency and drift.
- Add endpoint-change notifications, cancellation, graceful shutdown, and recoverable error reporting to the router.
- Keep a clear bypass path so audio can be routed without effects for diagnosis.

**Pass condition:** test tones and music pass through with DSP enabled and bypassed, with no persistent dropouts, runaway feedback, or unexplained channel changes.

### Gate 4 — Installer, signing, and release safety
- Package the driver and user-mode service/router together with clear install/uninstall and rollback behavior.
- Follow Microsoft's current driver signing and distribution requirements; do not ship an unsigned or test-signed package as a normal consumer release.
- Test clean install, upgrade, uninstall, sleep/resume, device unplug/replug, default-device changes, service crash/restart, and Windows updates.
- Provide a documented recovery route to restore the user's previous default playback device.

**Pass condition:** repeatable installation and recovery on supported Windows 11 systems, with logs and test evidence attached to the release checklist.

## Validation checklist

- [ ] WDK/SDK versions and target Windows versions recorded
- [ ] Driver builds from a clean checkout
- [ ] Endpoint appears with expected identity and format capabilities
- [ ] Endpoint selection and restoration tested
- [ ] Audio transport verified with deterministic test signals
- [ ] DSP enabled/bypass paths verified
- [ ] Buffer underrun/overflow and disconnect recovery verified
- [ ] Latency, CPU use, and long-duration playback measured
- [ ] Install/upgrade/uninstall and rollback tested
- [ ] Driver signing/distribution path reviewed
- [ ] No claim of system-wide support until the above gates pass

## Scope boundary

This plan does not implement the driver or certify the existing router. The actual WDK driver work and Windows-device validation remain outstanding. The existing Windows CI success only confirms that the current user-mode code compiles and its configured tests pass; it does not prove the virtual endpoint or end-to-end audio path works on a Windows machine.
