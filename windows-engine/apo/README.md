# APO DSP bridge (implementation groundwork)

This directory documents the boundary between the Windows APO callback and SubON's DSP core.

The allocation-free bridge in `../include/SubOnApoDspBridge.h` accepts interleaved float32 buffers and delegates enabled processing to the existing per-channel DSP processors. It supports bypass and in-place buffers, and it rejects invalid channel/buffer arguments without writing output. The callback-facing method does not allocate, lock, log, or call Windows APIs.

## Current status

This is **not yet a Windows APO DLL** and it does not register with a playback endpoint. The COM/APO implementation, format negotiation, INF/driver-package integration, signing, installer, and endpoint-level validation remain required. The bridge tests only validate the portable buffer/DSP boundary.

The next implementation work must use Microsoft's Windows APO interfaces and SYSVAD sample as the reference, then prove that the APO is loaded on a real test endpoint. Do not describe this bridge as system-wide processing.

## Test

The new `subon_apo_dsp_bridge_test` target is included in the normal CMake/CTest suite and runs on the existing Windows CI workflow.
