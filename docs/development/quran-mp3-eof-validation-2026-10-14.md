# Quran MP3 EOF validation — 2026-10-14

## Problem and resulting behavior

A controlled Waveshare test on main `2786ef7` served HTTP 200 with MP3 content
type but only text, without MP3 frames. Playback released ownership and returned
to `idle` with no error. Normal recovery playback worked, but the failed input
appeared to finish successfully.

The Quran-enabled product decoder now requires decoded MP3 PCM before accepting
EOF. A header alone does not qualify, and the observation resets on each start.
The existing pipeline propagates `FAILED` as a decoding error. Valid recordings
and graceful partial EOF after decoded PCM retain their existing behavior;
partial EOF does not prove complete recording playback. Non-Quran EOF behavior
is preserved.

## Automated validation

- The same 49-byte text fixture reproduced `FINISHED` on the unfixed production
  decoder and fails the new regression. The corrected decoder returns `FAILED`.
- The host regression compiles the production decoder, audio stream information
  and transfer buffers with UBSan. It links the unmodified host library from the
  firmware-resolved micro-MP3 0.4.0 package. Only allocation, timing and the
  single-threaded ring-buffer platform are deterministic adapters.
- Empty, text-only, ID3-only and incomplete first-frame inputs fail without PCM.
  Valid single-frame, short and 52-second synthetic clips finish and produce
  identical PCM through constant and chunked sources. Graceful partial EOF,
  sink backpressure and reuse after a valid clip also pass.
- The same executable is compiled without `OPENATHAN_QURAN` to verify the
  original EOF behavior. The reference CI job runs this resolved-codec check.
- All 20 UBSan CTests, all 135 Python tests with no skips, and the Quran worker,
  qualification-routing and Atom volume/Quran integration checks passed.
  The initial restricted Python run could not bind local TLS sockets or download
  pinned libraries; the complete permitted run passed without test-policy changes.

The host codec itself uses its upstream native build flags. UBSan applies to
the production ESPHome decoder, transfer buffers and host fixture; this is not
whole-codec sanitizer coverage. Synthetic fixtures are created only for these
host checks and must never be installed or played on hardware.

Reproduce after compiling the reference configuration:

```sh
python tools/check_mp3_decoder.py --build-dir /path/to/reference/openathan
```

## Matched firmware measurements

Nine before/after pairs used main `2786ef7`, the corrected source, ESPHome
2026.9.0, ESP-IDF 5.5.5 and Python 3.13.5 with identical public compile-only
settings and the same synthetic shared-audio image. Dependency versions and
sources matched after normalizing source paths and their manifest hash. The
embedded build-commit label was held constant to isolate the code-size change.
**These comparison images are compile-only and must never be installed.**

All 18 actual OTA images passed the existing capacity checker, including factory
payload consistency, flash headers, partition contracts and dependency pins.
Both application slots and the separate shared-audio partition are retained.

| Variant | Before OTA bytes | After OTA bytes | Delta | Application budget remaining |
| --- | ---: | ---: | ---: | ---: |
| Reference | 1,306,048 | 1,306,080 | +32 | 266,784 |
| Provisioning validation | 1,307,744 | 1,307,792 | +48 | 265,072 |
| Upgrade qualification | 1,312,496 | 1,312,544 | +48 | 260,320 |
| Upgrade startup failure | 1,312,496 | 1,312,544 | +48 | 260,320 |
| Waveshare production | 1,309,952 | 1,309,984 | +32 | 262,880 |
| Waveshare audio | 1,238,432 | 1,238,464 | +32 | 334,400 |
| Waveshare display | 1,288,176 | 1,288,208 | +32 | 284,656 |
| Waveshare diagnostics | 1,375,008 | 1,375,056 | +48 | 197,808 |
| Quran qualification | 1,377,504 | 1,377,536 | +32 | 195,328 |

Static RAM delta is zero for every pair. The fix adds a per-decoder boolean and
no buffers, tasks or dynamic allocations. Static measurements do not establish
runtime heap, fragmentation, PSRAM or stack headroom; those remain separate
attended measurements.

## Hardware boundary and follow-up

The source fix has not been installed or heard on hardware. The earlier
controlled failure and successful recoveries describe the unfixed source only.
Production was restored after that session; no device access occurred during
this implementation or its build checks.

Use the [failure qualification runbook](quran-failure-qualification.md) for a
fresh attended invalid-MP3 failure/recovery check on the reviewed source. Require
an error, released ownership, no automatic resume and clear explicit recovery.
Scheduled Athan preemption and rejected Quran Play during Athan require a suitable
real-clock prayer window. Provider soak, attended Wi-Fi interruption and Quran
cold-power/no-resume checks remain separate. Development evidence does not
establish public release readiness.
