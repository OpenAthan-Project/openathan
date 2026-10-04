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

## Physical validation limits

No device was accessed, flashed or played for this change. Host rendering does
not establish physical readability, and static RAM/build success does not
establish runtime heap, fragmentation or PSRAM headroom. No audio or network
pipeline changes are introduced. Physical preference save/reboot and screen
acceptance remain for a later attended development test or release qualification.
