# First APO format contract

The initial integration boundary accepts interleaved IEEE float32 samples only,
with 1–8 channels, a sample rate strictly above 32 kHz through 384 kHz, and a block alignment
of exactly channels * sizeof(float). This deliberately narrow contract prevents
unsupported PCM data from being passed to the float DSP processor.

SubOnApoFormat.h is a portable validation contract and its tests run in the
existing CMake/CTest workflow. It is not a Windows APO implementation.

## Implementation and remaining integration

`../src/SubOnNativeApo.cpp` implements the Windows COM APO class, format
negotiation and real-time callback. `../include/SubOnWindowsApoConnection.h`
adapts SDK connection properties to the portable bridge. CMake builds the
Windows-only `SubONApo` DLL and a Windows-only contract test.

Unsupported formats are rejected during negotiation; they must never be
interpreted as float32 or silently relabeled as supported. Rates at or below
32 kHz cannot represent the highest (16 kHz) EQ band below Nyquist and are
rejected rather than advertised and then failing DSP construction.

A supported signed endpoint package and recovery plan remain required before
installation. No INF, supported installer or verified system-wide path is
supplied. Verify independent applications on a real endpoint before making
any system-wide-processing claim.
