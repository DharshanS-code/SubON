# Audit and verification record

Date: 2026-11-10. Branch: `rework/reliability-responsive-ui`.

## Architecture and scope

This is a native Android Kotlin/Compose application plus a native Windows C++ WAV player, portable DSP library, experimental WASAPI router and APO COM DLL. There is no web application, remote API, account system or server database to repair. Native implementations and Git history were retained; the proposed Compose Multiplatform migration was not performed.

Source review covered Android models/preferences/math/activity/service/effects/UI/resources/build files, Windows playback/WAV/DSP/APO/router/format code, tests, workflows and setup documentation. The UI uses a shared design contract, not a claim that the engines have identical capabilities.

## Prioritized repairs implemented

| Priority | Defect / improvement | Implementation and verification scope |
|---|---|---|
| High | Windows playback worker synchronously queried UI while UI joined worker | UI publishes configuration snapshots; worker owns device/event/headers/buffers. Source-reviewed; native Windows runtime pending. |
| High | Stop/error paths could release queued audio storage and silently bypass DSP failures | Reset/unprepare ownership and explicit error states. Windows fault-injection/playback validation pending. |
| High | Malformed/non-finite Android preferences reached controls/effects | Normalized model and map-based decoding; JUnit regressions added, not executed here. |
| High | Android configuration flow could discard an update; effect updates recreated handles and left stale status | Replay latest state, serialize service work, update handles in place, refresh control/enable status. Android build/device checks pending. |
| High | Retry discarded remembered active audio sessions | Reuse existing engine for retry; same-settings/session recovery regression added. Not executed here. |
| High | Native non-finite samples, buffer overflow checks, mismatched rates and state handling | Portable regression suite passes. |
| High | APO negotiation/lifetime/output metadata and callback boundary defects | COM lifetime/exception containment, format checks, output capacity/frame validation, silent-tail processing and Windows contract tests added. Portable tests pass; Windows tests not run. |
| High | Router COM shutdown ordering, feedback loop and dropped packet tails | RAII, separate-endpoint validation, checked errors and bounded FIFO. Windows runtime pending. |
| Medium | Large WAV loading blocked the UI | Joinable loader, bounded cancellable reads, loading/error state, previous-file retention, deferred safe close. Temporary portable loader tests reported passing; Windows integration pending. |
| Medium | Loader advertised 32 kHz though DSP could not construct its highest EQ band | Loader now rejects rates at or below 32 kHz; parser tests and displayed limits aligned. Portable tests pass. |
| Medium | Windows fixed layout/fake scrollbar and Android narrow-only controls | Native DPI/scroll/focus handling; adaptive Compose cards, wrapping presets and labeled 48 dp controls. Rendering/accessibility validation pending. |
| Medium | Unsupported system-wide/hardware/limiter guarantees | Explicit attachment scope, calculated graph with values, honest profiles and limiter text. |
| Medium | Missing wrapper and incomplete CI gates | Official Gradle 8.2 wrapper with distribution checksum; Android unit/lint/build jobs; Linux/Windows Debug/Release portable matrix; publishing-only write permission. Workflows not run remotely. |
| Medium | Stale Android/APO setup documentation | Cross-platform quick start, environment requirements, real APO implementation status, shared design contract and this record. |

## Checks actually performed

### Portable native C++

- Host: Alpine Linux ARM64, GCC 15.2, CMake/Ninja.
- Initial baseline: 5/5 tests passed.
- Updated default/debug-style build: 6/6 CTest tests passed.
- Explicit Debug build: 6/6 passed (native engine implementation pass).
- Release build with `-Wall -Wextra -Wpedantic -Werror`: 6/6 passed after fixing WAV encoder width validation.
- Test names: `subon_dsp_smoke_test`, `subon_interleaved_float_test`, `subon_apo_dsp_bridge_test`, `subon_apo_format_test`, `subon_apo_realtime_processor_test`, `subon_wav_file_test`.
- Release tests use explicit checks rather than disabled `assert` calls.
- Standalone WAV and extracted asynchronous-loader tests passed GCC/Clang in the desktop implementation pass. The temporary loader harness is not a committed CI test and is not Windows runtime evidence.
- ASan/UBSan were attempted but unavailable: host linker could not find `libasan_preinit.o`, `libasan`, and `libubsan`.

Reproduce portable checks from the repository root:

```sh
cmake -S windows-engine -B windows-engine/build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build windows-engine/build-debug
ctest --test-dir windows-engine/build-debug --output-on-failure
cmake -S windows-engine -B windows-engine/build-release -G Ninja -DCMAKE_BUILD_TYPE=Release \
  '-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic -Werror'
cmake --build windows-engine/build-release
ctest --test-dir windows-engine/build-release --output-on-failure
```

### Android and tooling

- JDK 17 and Gradle 8.2 launch successfully.
- Gradle distribution SHA-256 verified against the official checksum: `38f66cd6eef217b4c35855bb11ea4e9fbc53594ccccb5fb82dfd317ef8c2c5a3`.
- Official wrapper generation succeeded. Its URL-validation request failed in this environment, so generation used the already checksum-verified official downloads URL with URL preflight disabled. Distribution checksum validation remains enabled.
- Gradle attempt `:app:testDebugUnitTest :app:compileDebugKotlin` failed before compilation: **SDK location not found**.
- 32 JUnit tests are present: config 6, math 8, preferences 4, engine 14. They have **not run** here.
- A direct JVM fallback was attempted but permission was explicitly denied because it would execute downloaded Gradle/Maven JARs. It was not rerouted or represented as a successful test.
- API 34 `javap` inspection confirmed the DynamicsProcessing setter signatures; that is not an Android compilation.
- Full Compose/activity/service compilation, APK packaging and lint remain unverified.

### UI and source checks

- Shared response-line palette validation passed against the dark card surface.
- Read-only review found and prompted fixes for retry/session loss, WAV rate mismatch and synchronous file loading.
- Follow-up read-only inspection confirmed the retry/session, rate-contract and asynchronous-loader fixes; no additional concrete loader/lifecycle defect was found.
- `git diff --check` passed; all 7 Android XML files parsed; relative documentation links resolved.
- No Android/Windows app was rendered, no screenshots compared, and no TalkBack/Narrator or audible processing test was performed. Visual parity is designed in source, **not yet verified**.

## Remaining release gates

1. **Android:** provision a supported Android SDK 34/build-tools 34.0.0 host, permit the required build dependencies, then run `./gradlew :app:testDebugUnitTest :app:lintDebug :app:assembleDebug`. Test Android 10+ devices, notification permission denial, rotation, profiles/reset, control loss/regain, sessions, stop/retry and real audio behavior.
2. **Windows:** compile with Visual Studio/Windows SDK and run CTest including `subon_windows_apo_test`. Test actual WAV playback, stop/close/load cancellation, inaccessible/slow files, device failure, keyboard focus, DPI and Narrator. Portable C++ does not compile Windows-only paths.
3. **Visual consistency:** execute the screenshot and interaction checklist in [design-system.md](design-system.md). Do not call it complete based on shared colors alone.
4. **APO/router:** supported signed endpoint package, reversible install/uninstall, separate desktop configuration transport and hardware validation are not implemented. Router additionally lacks clock-drift compensation/resampling/hotplug recovery and needs an independently supplied virtual endpoint. Do not change production endpoints or disable Windows security to test it.
5. **Distribution:** Android production keystore/passwords and Windows signing infrastructure must be supplied privately by the owner; no keys are in this repository. Debug APKs/prototype EXEs are not production releases. Current target SDK 34 also needs reassessment against distribution-store requirements before submission.

No commits, pushes, releases, PRs or deployments were performed during this audit. Changes remain local and uncommitted. This record is not a production-readiness certification.
