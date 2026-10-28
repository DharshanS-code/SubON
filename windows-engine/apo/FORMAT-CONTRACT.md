# First APO format contract

The initial integration boundary accepts interleaved IEEE float32 samples only,
with 1–8 channels, a sample rate from 8 kHz through 384 kHz, and a block alignment
of exactly channels * sizeof(float). This deliberately narrow contract prevents
unsupported PCM data from being passed to the float DSP processor.

SubOnApoFormat.h is a portable validation contract and its tests run in the
existing CMake/CTest workflow. It is not a Windows APO implementation.

## Required next implementation

- Implement the actual Windows COM APO class against the Windows SDK/WDK APO
  interfaces and model lifetime/registration on Microsoft's SYSVAD sample.
- Implement IAudioProcessingObject format negotiation and
  IAudioProcessingObjectRT processing, returning transparent pass-through for
  unsupported formats rather than interpreting them as float samples.
- Connect negotiated float32 buffers to subon::dsp::ApoDspBridge.
- Add a Windows-only APO DLL build and compile it on a runner with the required
  Windows audio/driver headers available.
- Add a properly packaged, signed endpoint integration and recovery plan before
  attempting installation on a real Windows machine.

No DLL, INF, endpoint registration, installer, or system-wide processing path
is provided by this contract. Do not claim system-wide processing is available
until independent applications have been verified on a real endpoint.
