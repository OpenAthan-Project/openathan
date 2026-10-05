# Time-format preference — development validation, 2026-10-04

This unreleased change adds a saved 12/24-hour presentation preference to the
local device page and optional status screen. Existing devices retain 24-hour
time. The API's local timestamps, prayer settings, consumption history and
scheduler behavior retain their existing contracts. Release version declarations
remain unchanged; these development builds are not published release artifacts.

The preference uses a separate checksummed NVS record with revision-checked saves.
Older firmware can still load the original prayer settings and consumption
records. Missing preference records default to 24-hour time without a write;
corrupt records are retained and storage failures do not gate Athan playback.

## Automated and visual checks

- All 14 UBSan CTests passed, including production/isolated preference storage,
  interrupted-write outcomes, corrupt records, revision conflicts and
  midnight/noon formatting.
- All 103 pinned-environment Python tests passed. The production and isolated
  C++ settings/local-API bridge tests additionally use the resolved ArduinoJson
  library; they verify strict request validation, unchanged prayer settings and
  occurrence identity, preference persistence, display refresh and maintenance
  write denial.
- All 42 Chromium/WebKit browser cases passed. The new cases cover saved format
  after reload, timetable/preview/next-event formatting, a dropped save response,
  preserved edits after a revision conflict and narrow-screen overflow.
- Desktop and narrow-screen captures were reviewed. The shared production LCD
  renderer's host captures keep large next-prayer digits and fit an AM/PM line
  above the existing status text. Pixel-bound checks passed.
- Whitespace checks passed. The UI detector reported existing header typography
  and decorative-label findings; this focused preference change retains the
  existing interface styling.

## Matched firmware measurements

Baseline source is `382a3f8` with public compile-only configuration. Both reference
builds use the same source directory, configuration and pinned Python 3.13.0,
ESPHome 2026.9.0, ESP-IDF 5.5.5 and compiler `esp-14.2.0_20260121`.
The baseline uses compile-only identity, so its 1,249,408-byte image differs from
the published exact-source v0.3.0 image; compare these matched builds for cost.
Synthetic CI audio fixtures were used only for build checks.

| Build | Actual OTA bytes | Remaining 1.5 MiB budget |
| --- | ---: | ---: |
| Reference before | 1,249,408 | 323,456 |
| Reference after | 1,252,912 | 319,952 |
| Isolated provisioning after | 1,254,640 | 318,224 |

Reference growth is **3,504 bytes**. Static RAM grows by 40 bytes to 115,163.
The existing capacity checker passed for reference and isolated provisioning
images, including pinned dependencies, profiles, partition layout, factory/OTA
consistency and exclusion of audio from application storage. Both application
slots and the separate shared-audio partition remain unchanged.

## Upcoming-prayer layout follow-up

The status label now appears above the prayer name. On the 128×128 screen,
upcoming-event rows begin at y=6 (clock), 30 (status label), 44 (prayer name),
68 (large time) and 100 (AM/PM). The Offline footer remains at y=116.
“Will be skipped” and “Not ready yet” use the same label position as “Next Athan.”
Setup, playback, waiting, disabled and fault screens retain their existing layout.
The internal frame flag changes presentation only; public APIs, durable records
and scheduling retain their existing behavior.

All 14 UBSan CTests and 103 Python tests passed again. Renderer checks cover every
prayer name in both time formats, ready/skipped/not-ready states, online/offline
footers, midnight and noon. They verify bounds and empty separation bands between
rows. Actual renderer previews were reviewed against the approved mockup;
eight other-state renders remain pixel-identical to their pre-change captures.

The baseline is the saved-format implementation at `5968de4`. Before/after builds
reuse the same temporary source directory, compile-only identities, configurations
and pinned toolchain described above. Capacity checks passed for both variants.

| Build | Before OTA bytes | After OTA bytes | Delta | Remaining 1.5 MiB budget |
| --- | ---: | ---: | ---: | ---: |
| Reference | 1,252,912 | 1,253,008 | +96 | 319,856 |
| Isolated provisioning | 1,254,640 | 1,254,752 | +112 | 318,112 |

Static RAM remains 115,163 bytes for both variants.

The exact `39cc037` production-profile development application was then installed
on the reference device: 1,253,040 bytes, SHA-256
`c957077df85c674147491a1deb90e6aada3120ae6f95f16ab01a9113d526bda0`,
leaving 319,824 bytes of application budget. Fresh paired reads and independent
application readback passed. Full control/settings/history/credentials/OTA
metadata, bootloader, inactive application and shared audio remained byte-identical
around this application-only installation. Before installation, the durable
preference decoded as 12-hour time/revision 2, with settings revision 3 and current
prayer-consumption history retained. Physical bottom-power startup, revised-layout
readability and 10-second cold-restart persistence remain pending.

The observations below apply to the earlier layout and exact image stated there.

## Attended hardware validation

The reference AtomS3R C126 + Voice Pyramid A167 was tested with an unpublished
production-profile development image from `5968de4`. The exact application is
1,252,944 bytes, SHA-256
`123281f200520de3490b6e4faf33f0ca9a04d7996056f3f9a6ac12a35179f0c4`,
leaving 319,920 bytes of application budget. Its source-bound build checks passed.
The 32-byte difference from the compile-only measurement above comes from the
exact build identity; this does not change the matched 3,504-byte feature cost.

Fresh paired USB reads retained the installed released application for recovery.
After another complete paired checkpoint comparison, one application-only write
and independent readback passed. Full settings/history/credentials/OTA metadata,
bootloader, inactive application and shared audio were byte-identical around
installation. No factory image, synthetic audio or historical NVS was installed.

On bottom-only power, authenticated status confirmed the exact development
commit, active/applied setup and the existing device's 24-hour default. One
revision-checked save applied 12-hour time at revision 2. Prayer settings/revision,
history, light settings and the next occurrence were unchanged across that save.
Later scheduling remained ready without fault; history advanced normally as
scheduled occurrences passed.

A supplied physical-screen photo shows the current clock at **8:30 PM** and next
Fajr at **6:02 AM**, with both meridiem labels readable and no visible clipping.
Authenticated follow-up agrees with the next occurrence and still reports the
saved 12-hour preference. The photo was taken before the requested cold restart;
physical cold-restart persistence remains pending.

Static RAM/build success does not establish runtime heap, fragmentation or PSRAM
headroom. No audio or network pipeline changes are introduced, and unrelated
completed playback checks were not repeated. These observations establish
development behavior, not qualification of a new published release.
