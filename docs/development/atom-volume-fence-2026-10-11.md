# Atom output fence validation — 2026-10-11

This corrects PR #44's GPIO gate and logical volume acknowledgement. Checked
AW87559 SYSCTRL PA control replaces the expander switch; the codec remains a
second checked gate. The calibrated adapter owns software gain and waits for a
complete current-generation DMA descriptor before opening either gate. A fault
never authorizes unmute. See the [calibration contract](atom-volume-calibration-2026-10-11.md)
and [pinned component provenance and maintained patch](../../third_party/esphome-i2s-audio/UPSTREAM.md).

The branch was rebased onto main `2cea368` (Quran streaming). The existing
partition-player streaming capability, ownership, state and playback epoch are
forwarded through the calibrated adapter; Athan and Quran use the same output
control. HTTP contracts, volume curve, 56% default, compatible settings records,
prayer history and shared audio retain their contracts.

## Automated checks

- All 20 CTests passed with UndefinedBehaviorSanitizer, including production and
  isolated settings migration/interrupted-save regressions.
- The actual vendored standard speaker task runs against a deterministic fake
  I2S transport. The tests reproduce logical readback while louder DMA buffers
  remain queued, and a staged raw tail across pause and bit-depth narrowing.
  Reintroducing early unmute or early staging attenuation makes those regressions
  fail. Transport/sample helpers are simulated; this is not physical playback.
- Coverage includes rapid/superseded and duplicate requests, separate deadlines
  for distinct targets, duplicate requests at the deadline, paused/idle playback,
  Stop without replay, session restart/stale tokens, allocation/task/channel/
  callback failures, missing/overflowed completions, queue desynchronization,
  partial PCM/silence/preload writes, invalid DMA layout and counter exhaustion.
  Deferred ISR event-group updates and faults during driver teardown cannot
  erase failure. Startup cleanup retains another bus owner's lock and releases
  its own once.
- Checked amplifier/codec writes and incorrect readbacks, startup/shutdown,
  applied zero with both gates closed, generic enabled startup, ignored
  media-player preferences and Quran ownership handoffs are covered.
- All 120 Python tests and 13 configuration schemas passed. The six affected
  provenance/schema tests passed again after the final maintained-patch update.
  Generic hardware retains its ordinary DAC route; calibrated profiles reject
  an unfenced speaker, enabled startup, a direct DAC binding and SPDIF.
- All 532 Chromium/WebKit cases passed with Node 24.19.0 / Playwright 1.62.1.
  Upgrade runtime, resolved ESP-IDF HTTP and Quran worker regressions passed.

## Matched firmware measurements

Before is the original PR #44 calibration rebased onto current main (`45db537`).
After is the delivered fence and compatibility correction. Both source exports
use identical compile-only settings/audio, generated development identities,
ESPHome 2026.9.0, ESP-IDF 5.5.5 and compiler `esp-14.2.0_20260121`.
All ten pairs were built; after-builds were refreshed after the final review
fixes. Dependency hashes match within every pair and all capacity checks pass.
These are development measurements, not release-qualified images.

| Variant | Before OTA bytes | After OTA bytes | Delta | Remaining 1.5 MiB budget | Static RAM delta |
| --- | ---: | ---: | ---: | ---: | ---: |
| Atom reference | 1,305,888 | 1,305,296 | -592 | 267,568 | +20 |
| Atom real schedule | 1,125,792 | 1,123,808 | -1,984 | 449,056 | +20 |
| Atom synthetic timetable | 1,143,344 | 1,141,344 | -2,000 | 431,520 | +20 |
| Atom provisioning validation | 1,307,632 | 1,307,040 | -592 | 265,824 | +20 |
| Atom upgrade qualification | 1,312,416 | 1,311,824 | -592 | 261,040 | +20 |
| Atom startup-failure qualification | 1,312,416 | 1,311,824 | -592 | 261,040 | +20 |
| Waveshare production | 1,309,872 | 1,309,872 | +0 | 262,992 | +0 |
| Waveshare audio | 1,238,352 | 1,238,352 | +0 | 334,512 | +0 |
| Waveshare display | 1,288,080 | 1,288,080 | +0 | 284,784 | +0 |
| Waveshare diagnostics | 1,374,928 | 1,374,928 | +0 | 197,936 | +0 |

The four Waveshare controls have unchanged OTA sizes and static RAM; independently
rebuilt images have different hashes. Both 2 MiB OTA slots, the 1,572,864-byte
application limit and separate 3.5 MiB shared audio partition are preserved.
Reference slot headroom is 791,856 bytes, exceeding the required 512 KiB.

The fence adds no audio task, PCM buffer or queue. The existing ten-entry
completion-record queue grows from 4 to 12 bytes per entry (+80 heap bytes).
Its state occupies 24 bytes; the ESP32-S3 critical-section lock occupies 8 bytes,
plus the enable flag/alignment. The measured static RAM delta includes removal
of the incorrect GPIO objects and added transition state. Ring/staging/DMA buffer
sizes and the 4096-byte speaker task stack remain unchanged. New task locals and
concurrent audio/network/UI load still need runtime heap, fragmentation, stack
and PSRAM measurements; static RAM does not establish that headroom.

## Attended acceptance remains separate

No hardware was connected, installed, played or restarted for this correction.
Compile-only fixtures must NEVER be installed or played. Follow the
[scheduler validation runbook](../../firmware/esphome/scheduler/VALIDATION.md) or
[provisioning hardware runbook](../../firmware/esphome/provisioning/HARDWARE_TEST.md).
Identify the live device and preserve compatible application-only recovery,
settings, consumption history and shared audio before any attended installation.

With approved recordings, verify that PA and codec readbacks correspond to
observed output gating, then exercise zero, live/rapid/paused adjustments, Stop
and fault handling. Check cold restart and runtime memory separately. Register
verification and simulated FIFO completion do not establish physical silence,
analog transients, distortion, comfortable maximum output or release readiness.
Merge, release, website work and installation remain separate actions.
