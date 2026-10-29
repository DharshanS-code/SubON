# SubON virtual-audio routing prototype

This directory documents the first user-mode part of a FxSound-style virtual-soundcard architecture.

## Audio path

1. Applications render to a **virtual playback/render endpoint**.
2. The SubON router opens that endpoint with WASAPI loopback capture.
3. Captured float32 audio passes through SubON's existing DSP bridge.
4. The router renders processed samples to the current Windows default physical output.

The router searches active render endpoints for the friendly name **SubON Virtual Audio**. This is a name contract, not a driver: this repository does not yet provide the kernel-mode virtual audio device package that creates that endpoint.

## Build

The Windows CI build compiles `SubONVirtualRouter.exe` as a separate target. It does not install or change the default playback device.

## Current limits

- A separately installed virtual render endpoint is required.
- The first router only accepts interleaved IEEE float32, 1–8 channels, and matching virtual/physical sample rate, channel count and block alignment. It exits with an error for unsupported formats.
- Device hot-plug, endpoint changes, sample-rate/channel conversion, buffering recovery, UI controls, service/autostart, installer and signed driver package are not implemented.
- This is an experimental routing prototype, not yet a safe system-wide release. Do not set an unfamiliar virtual endpoint as the only output until the router and recovery path have been tested.

## Driver work still required

A real SubON virtual endpoint must be supplied by a WDK audio driver package based on Microsoft's documented virtual audio / SYSVAD sample architecture. The driver must be independently built, packaged, signed as appropriate, and tested on a disposable Windows test environment before installation. The user-mode router alone cannot create a Windows audio endpoint.

Reference: Microsoft's [Virtual Audio Devices](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/virtual-audio-devices) documentation and [Windows-driver-samples SYSVAD](https://github.com/microsoft/Windows-driver-samples/tree/main/audio/sysvad).
