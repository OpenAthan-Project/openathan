# Waveshare persistent-ring validation — 2026-10-14

The Waveshare prayer ring remains visible during Quran/Athan playback, setup,
clock-waiting and faults. Center messages, stop guidance and the existing
upcoming-prayer countdown layout are preserved. This report describes automated
development validation; physical readability of the additional states has not
been tested.

## Behavior and compatibility

The presenter calculates proximity independently of the center-screen state,
using the existing validated, strictly future visual prayer. Exact UTC duration
sets green above 30 minutes, orange above 10 through 30 minutes and red within
10 minutes. Muted and skipped calculated prayers still participate. At a prayer
timestamp, the ring advances to the next future prayer, including during Athan.

The renderer always draws the existing 8px band at radii 164–172px. Incomplete
setup, unhealthy settings storage, an invalid clock or missing/invalid future
prayer timing gives a neutral gray ring (`#606060`). Audio or consumption-history
faults can retain a timing color when the independent visual timetable is valid.
Unavailable timing clears the color rather than retaining a stale frame.

The Atom layout, screen brightness, 259,200-byte Waveshare framebuffer,
one-second polling and visible-frame deduplication are preserved. There are no
new persistent records, HTTP APIs, fonts, embedded images or dependencies.

## Automated validation

- All 20 CMake/CTest checks passed with UndefinedBehaviorSanitizer.
- All 135 Python tests passed with no skips using the pinned ESPHome environment.
- The production and isolated settings/display adapters passed against pinned
  ESPHome timezone conversion and ArduinoJson 7.4.3 headers.
- New presenter coverage fails against unchanged main, reproducing the previous
  loss of prayer proximity during playback.
- Renderer checks verify every pixel of the ring and ensure status text remains
  inside its inner radius. The design detector reported no findings.

Coverage includes Quran/Athan playback, every scheduler fault category, gray
fallback/recovery, exact 30-/10-minute thresholds during playback, muted/skipped
prayers, Offline, unchanged-frame caching, invalid/stale timing, and the normal
Quran-to-Athan scheduled handoff selecting the following prayer. Adapter polls
preserve settings and perform no additional durable writes.

These host previews use the shipping C++ renderer and existing bitmap font with
illustrative values. Top row: Quran, Athan, audio fault and history-storage fault
with valid prayer timing. Bottom row: setup, clock-waiting, settings-storage
fault and the existing upcoming-prayer layout. The image is documentation only
and is not embedded in firmware.

![Persistent ring across playback, fault and waiting states](waveshare-persistent-ring/screens.png)

## Matched firmware capacity

Baseline source: `6a197fa0e29ced28045aaeb31fb9014804c4d733`.
Both phases use Python 3.13.5, ESPHome 2026.9.0, ESP-IDF 5.5.5 and Xtensa GCC
14.2.0 (`esp-14.2.0_20260121`), identical profile configuration, source/build
paths, embedded baseline commit metadata and generated compile-only fixtures.
Only the two display headers change between phases. Synthetic audio fixtures
must never be installed or played.

| Profile | Baseline OTA bytes | Candidate OTA bytes | Delta | Budget remaining | Slot free |
| --- | ---: | ---: | ---: | ---: | ---: |
| Atom reference | 1,306,080 | 1,306,128 | +48 | 266,736 | 791,024 |
| Isolated provisioning | 1,307,792 | 1,307,840 | +48 | 265,024 | 789,312 |
| Upgrade qualification | 1,312,560 | 1,312,608 | +48 | 260,256 | 784,544 |
| Startup-failure qualification | 1,312,560 | 1,312,608 | +48 | 260,256 | 784,544 |
| Waveshare production | 1,309,984 | 1,310,048 | +64 | 262,816 | 787,104 |
| Waveshare display development | 1,288,208 | 1,288,272 | +64 | 284,592 | 808,880 |
| Waveshare diagnostics | 1,375,056 | 1,375,120 | +64 | 197,744 | 722,032 |
| Quran qualification | 1,377,536 | 1,377,600 | +64 | 195,264 | 719,552 |

All 16 builds passed the existing dependency, partition, factory/OTA consistency,
qualification-exclusion and audio-capacity checks. Each pair has an identical
resolved dependency lock. Static RAM delta is zero for every profile. All actual
OTA images remain below the 1,572,864-byte application budget in unchanged
2,097,152-byte slots; both slots and the 3.5 MiB shared audio partition are retained.

## Hardware and release limits

No device access, installation, playback, release or website change was performed
for this change. Existing dated hardware findings retain their original scope.
The same framebuffer and frame representation are used, with no new dynamic
allocation; static RAM and successful compilation do not establish concurrent
audio/network/display runtime headroom or physical readability. A future
coordinated hardware session must reidentify live state and preserve settings,
prayer history, shared audio and compatible application-only recovery.
