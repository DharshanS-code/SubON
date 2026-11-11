# SubON shared design contract

Android Compose and the native Windows player share one application identity without pretending to expose identical audio engines. Preserve their native audio features; do not substitute a web shell or a nonfunctional mockup.

## Tokens

| Role | Hex | Android token |
|---|---|---|
| Canvas | `#10141C` | `Zaffre` |
| Card surface | `#1B2230` | `ZaffreDark` |
| Border / inactive track | `#344158` | `Periwinkle` |
| Primary text | `#F1F4FA` | `Lavender` |
| Secondary text | `#A6B1C2` | `Muted` |
| Action accent | `#CCCB66` | `Citron` |
| Warning / error | `#FFB4A8` | `Warn` |
| Response line | `#999832` | `ResponseLine` |

Source implementations: `app/src/main/java/app/subon/Theme.kt` and `windows-engine/src/WindowsApp.cpp`. Keep startup/icon resources aligned. The single-series chart color passed the palette validator against the card surface; this is not evidence of rendered layout or screen-reader compliance.

## Layout and interaction

- Use the SubON name, “Shape your sound” subtitle, dark cards, yellow-green actions and a common heading/body/value hierarchy.
- Use native sans-serif typography: Android system font / Segoe UI. Match hierarchy and spacing rather than requiring an unavailable font.
- Use rounded sections, a 4-unit spacing rhythm, 16–32-unit page gutters and generous control separation.
- Target at least 48 dp / DPI-scaled logical pixels for primary controls. Preserve native focus and keyboard semantics.
- Android stacks cards on narrow displays and uses two columns from 840 dp, with content capped at 1120 dp. Presets wrap instead of overflowing.
- Windows resizes native controls and scrolls vertically. Tab navigation must reveal focused offscreen controls.
- Use visible text as well as color for status, selection and errors. Keep values and labels in text colors.
- Match conceptual sections such as “Output & protection”; retain Android band-pass controls and Windows WAV playback/ten-band EQ.

## Truthful data and status

The Android graph is calculated, not measured. It includes gain, excludes signal-dependent limiter dynamics and does not model hardware or exact discrete-band device processing. Its log frequency axis is 20–500 Hz; level is −72 to +6 dB. Provide labeled axes and the numeric values toggle. Do not add simulated live meters.

“Effect attached” means an enabled effect is controlled by the application, not verified system-wide audio. Profiles are saved starting points, not hardware detection. A limiter does not guarantee safe listening or distortion-free output.

## Visual acceptance checks

Actual platform rendering is required; source review is not a substitute:

1. Compare Android phone portrait/landscape and tablet with Windows at 100%, 150% and 200% DPI.
2. Check Android at 320/360 dp widths and enlarged fonts. Check Windows minimum size and short windows with scrolling.
3. Exercise every preset, slider, toggle, profile, reset confirmation and start/stop control.
4. Verify keyboard focus, TalkBack/Narrator labels, selected states and readable errors.
5. Compare screenshots for palette, hierarchy, spacing, clipping, focus visibility and graph labels. Record outcomes; do not mark this gate complete without platform evidence.
