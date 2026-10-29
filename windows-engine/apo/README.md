# SubON Windows APO — implementation status

This directory documents and tests the boundary between a future Windows Audio Processing Object (APO) callback and SubON's DSP core.

## Existing groundwork

The allocation-free bridge in `../include/SubOnApoDspBridge.h` accepts interleaved float32 buffers and delegates enabled processing to the existing per-channel DSP processors. It supports bypass and in-place buffers, and rejects invalid channel/buffer arguments without writing output. The callback-facing method does not allocate, lock, log, or call Windows APIs.

The portable format contract is in `FORMAT-CONTRACT.md`; the bridge and format tests run in the existing CMake/CTest suite.

## Current status — not yet a functional APO

There is **not yet a Windows APO DLL**. The COM/APO implementation, Windows SDK interface integration, real-time processing callback, endpoint registration/package, signing, installer, and endpoint-level validation remain to be implemented. Existing tests validate only portable DSP/buffer behavior.

## Next code milestone

Create a separate Windows-only APO DLL target based on Microsoft's documented APO interfaces and SYSVAD sample. Begin with a transparent pass-through implementation and format negotiation; then connect the existing DSP bridge. Keep allocation, locks, file I/O, logging, and UI communication out of the real-time callback.

Do not describe this bridge or a successful DLL compile as system-wide processing. Verify the APO is loaded and processes audio on a real test endpoint before making that claim.

References:
- https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/audio-processing-object-architecture
- https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/implementing-audio-processing-objects
- https://github.com/microsoft/Windows-driver-samples/tree/main/audio/sysvad
