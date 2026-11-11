# SubON Windows APO — implementation status

This directory documents the boundary between the Windows Audio Processing Object (APO) implementation in `../src/SubOnNativeApo.cpp` and SubON's DSP core.

## Existing groundwork

The allocation-free bridge in `../include/SubOnApoDspBridge.h` accepts interleaved float32 buffers and delegates enabled processing to the existing per-channel DSP processors. It supports bypass and in-place buffers, and rejects invalid channel/buffer arguments without writing output. The callback-facing method does not allocate, lock, log, or call Windows APIs.

The portable format contract is in `FORMAT-CONTRACT.md`; the bridge and format tests run in the existing CMake/CTest suite.

## Current status — not yet a functional APO

The `SubONApo` CMake target implements a COM DLL with explicit exports, Windows SDK APO interfaces, format negotiation and a real-time processing callback. The portable tests exercise its DSP/buffer contracts. Windows compilation does not establish endpoint loading or audible operation.

**Still missing:** a supported endpoint/driver package, signing, reversible installer, desktop-to-APO configuration transport, and endpoint-level audio validation. Do not register the DLL on a production endpoint or replace an OEM effect ad hoc.

## Next code milestone

Validate the existing DLL on a dedicated Windows test endpoint, then design a supported signed package and reversible installation flow using Microsoft's APO documentation and SYSVAD sample. Add a validated configuration channel without coupling the real-time callback to the desktop UI. Keep allocation, locks, file I/O, logging and UI communication out of that callback.

Do not describe this bridge or a successful DLL compile as system-wide processing. Verify the APO is loaded and processes audio on a real test endpoint before making that claim.

References:
- https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/audio-processing-object-architecture
- https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/implementing-audio-processing-objects
- https://github.com/microsoft/Windows-driver-samples/tree/main/audio/sysvad
