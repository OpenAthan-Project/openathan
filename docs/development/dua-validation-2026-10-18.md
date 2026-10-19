# Dua development validation — 2026-10-18

This PR adds optional stored dua after scheduled Athan, independent preference
persistence, signed media installation and playback ownership through the pause.
At the time of these checks, no dua recording had been supplied or approved.
The subsequent [recording approval](../../AUDIO-LICENSES.md) does not add hardware
qualification to these compile-only development results. No device was accessed,
flashed or played, and no release was published.

## Automated checks

- All 22 UBSan CTests passed, including sequence timing, cancellation, default-off
  preferences, failed storage, stale completion and normal/Fajr behavior.
- All 152 pinned Python tests and 294 Chromium/WebKit browser tests passed.
  Browser coverage includes uncertain preference saves, download feedback, Retry,
  Stop through the silent pause and narrow-screen overflow. The new component was
  visually inspected at phone and desktop widths using simulated device state.
- The unchanged production pipeline state-machine bodies passed host checks for
  reader EOF, decoder success, drained output, latched errors, queued cancellation,
  stale startup flags and delayed task deletion. Core-owned shutdown cancellation
  is checked through the production settings adapter. The production MP3 decoder passed
  real-codec PCM, invalid/empty input, partial EOF, chunking, backpressure and reuse
  checks with and without Quran. Adapter tests separately cover stored completion
  without Quran and the original unmodified-player fallback.
- The production optional-audio downloader passed deterministic interruption tests
  at all 68 erase/write operations, with partial operations and reboot/retry.
  Normal/Fajr bytes stayed identical. Checks also cover committed-bank selection,
  fallback after payload corruption, retained installed media, signature rejection,
  cancellation, size faults and unknown tail data. Ordinary isolated builds refuse
  production downloads; the private qualification transport runs the same engine.
- Python crypto checks use actual ephemeral P-256 signatures. Device host tests
  substitute platform I/O and the mbedTLS verification result; firmware builds
  link the pinned production cryptographic implementation.
- Release tests bind downloadable audio/descriptor bytes to the fresh-install bank,
  reject mismatched assets, enforce MP3 decoding/duration, and preserve large legacy
  images whose Athan extends beyond the proposed optional-bank area.
- The actual website validator source matched the committed installer pin at
  `8b96c6c7d8239d5e6aa67b5042b900e24e66568d`. It accepted legacy and extended
  producer bundles for both boards and rejected altered audio. The baseline C++
  reader from main also accepted the extended two-track image.
- Existing updater, USB, Quran, Atom volume/Quran and phone-setup backend checks
  passed. Whitespace and relative documentation links were checked.

Host/browser prerequisites: Python 3.13.5, Node 24.19.0, the pinned ESPHome
requirements and Playwright 1.62.1. Synthetic fixtures must never be installed or
played. Reproduction commands are in the [dua contract](dua.md), [CI guide](../../.github/workflows/README.md)
and [device UI guide](../../web/device-ui/README.md).

## Matched firmware measurements

Before: main `9d7e87ce2a438bb7805967059ed3264a46cf273b`. After: this PR's feature
implementation. Both sides used ESPHome 2026.9.0, ESP-IDF 5.5.5, compiler
`esp-14.2.0_20260121`, Python 3.13.5, the same public compile-only configuration,
synthetic shared-audio bytes and development build identity. Each variant used
separate build directories. Final loaded local sources and compressed UI were
compared with the workspace; generic developer builds intentionally retain the
pinned unmodified upstream player.

All 22 actual OTA applications (11 before/after pairs) passed capacity checks,
including application/factory payload consistency, dependency pins, flash headers,
profile exclusions and partition contracts. Both 2 MiB application slots and the
separate 3.5 MiB shared-audio partition remain unchanged. Remaining budget below
is relative to 1,572,864 bytes; slot headroom is a further 524,288 bytes larger.

| Variant | Before OTA bytes | After OTA bytes | Delta | Application budget remaining | Static RAM delta |
| --- | ---: | ---: | ---: | ---: | ---: |
| atom | 1,385,952 | 1,400,976 | +15,024 | 171,888 | +256 |
| waveshare | 1,389,696 | 1,404,736 | +15,040 | 168,128 | +256 |
| device | 1,124,528 | 1,127,840 | +3,312 | 445,024 | +136 |
| validation | 1,142,112 | 1,145,456 | +3,344 | 427,408 | +136 |
| provisioning | 1,387,456 | 1,402,480 | +15,024 | 170,384 | +256 |
| upgrade | 1,392,288 | 1,411,696 | +19,408 | 161,168 | +256 |
| upgrade-failure | 1,392,288 | 1,411,696 | +19,408 | 161,168 | +256 |
| waveshare-audio | 1,304,560 | 1,321,056 | +16,496 | 251,808 | +256 |
| waveshare-display | 1,368,096 | 1,384,624 | +16,528 | 188,240 | +256 |
| waveshare-diagnostics | 1,455,712 | 1,472,048 | +16,336 | 100,816 | +256 |
| quran | 1,458,224 | 1,474,448 | +16,224 | 98,416 | +256 |

No recording enters an application image. Actual approved-media packaging remains
pending; media-bank payload size does not consume application budget.

## Review-fix validation

The follow-up fixes separate transient descriptor read failures from invalid
metadata and allow healthy dua preference saves when media reports
`supported: false`. Cancellation after a blocked read wins over its transport
error. Media availability feedback remains independent of saved intent.

- Both new regressions reproduced on reviewed head
  `49a44b5e6f41c36a61e464290633ce12a1b32428` before the fixes.
- All 22 UBSan CTests and 152 pinned Python tests passed. The full browser matrix
  covered all 564 registered cases: 282 Chromium and 282 WebKit. WebKit coverage
  was completed across continued runs, with every case accounted for.
- Production downloader checks cover immediate and partial descriptor read
  failures, the five-minute retry boundary, disconnected/unsafe/disabled retry
  admission, cancellation during a failing read, and explicit Retry for oversized,
  malformed and unverifiable descriptors. Existing 68-point interrupted-write,
  isolated-storage and private-qualification checks passed.
- Browser regressions cover enabling/disabling intent with unavailable media,
  uncertain-save readback without duplicate POSTs, preference-storage faults and
  older firmware without dua. Existing unsupported display/light guards passed.
- Production pipeline, updater/USB, Quran and Atom volume continuity checks passed.
  Whitespace and relative documentation links were checked.

The matrix below compares rebuilt reviewed-head applications with the fixes,
using the same pinned toolchain, compile-only settings and synthetic media on
both sides. Baseline sizes and static RAM match the earlier reviewed-head
measurements above. All 22 actual OTA images passed capacity checks; final loaded
downloader sources and compressed UI matched the workspace. These incremental
measurements supersede the final sizes above while retaining the original feature
comparison with main. No application slot or shared-audio layout changed.

| Variant | Before OTA bytes | After OTA bytes | Fix delta | Application budget remaining | Static RAM delta |
| --- | ---: | ---: | ---: | ---: | ---: |
| atom | 1,400,976 | 1,400,976 | 0 | 171,888 | 0 |
| waveshare | 1,404,736 | 1,404,752 | +16 | 168,112 | 0 |
| device | 1,127,840 | 1,127,840 | 0 | 445,024 | 0 |
| validation | 1,145,456 | 1,145,456 | 0 | 427,408 | 0 |
| provisioning | 1,402,480 | 1,402,496 | +16 | 170,368 | 0 |
| upgrade | 1,411,696 | 1,411,680 | -16 | 161,184 | 0 |
| upgrade-failure | 1,411,696 | 1,411,680 | -16 | 161,184 | 0 |
| waveshare-audio | 1,321,056 | 1,321,056 | 0 | 251,808 | 0 |
| waveshare-display | 1,384,624 | 1,384,624 | 0 | 188,240 | 0 |
| waveshare-diagnostics | 1,472,048 | 1,472,048 | 0 | 100,816 | 0 |
| quran | 1,474,448 | 1,474,464 | +16 | 98,400 | 0 |

No hardware was accessed or qualified for these fixes. The remaining media,
physical playback/recovery and runtime-memory limits below still apply.

## Remaining qualification

The final recording, exact text, redistribution permission, attribution and
listening quality still require review before media packaging. Development builds
retain the current development version; they are not signed release candidates.

Physical offline playback, Stop, interrupted media installation/recovery, firmware
and provisioning exclusion, scheduler continuity/lateness under flash operations,
and runtime heap/fragmentation/PSRAM have not been observed for this feature on
either board. Host task/flash adapters do not establish those timing or power-loss
properties. Static RAM and successful builds do not establish runtime headroom.

Use separately authorized, attended sessions under the existing hardware runbooks,
with isolated preferences, fresh compatible application recovery and preservation
of current settings, consumption and the complete shared-audio partition. Test
preferences do not isolate the physical audio banks. Neither these results nor
existing source-bound hardware observations establish qualification of this change.
