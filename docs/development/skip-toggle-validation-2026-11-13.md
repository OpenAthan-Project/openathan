# Skip/Restore hold validation — 2026-11-13

The front or BOOT button now uses the same release-triggered 2–5 second hold
(about three seconds) to skip the next Athan and restore a saved skip. The
separate 6–10 second cancel gesture is removed from all three boards and the
standalone scheduler development profile.

`openathan.button` adds `action: toggle_skip`. The shared adapter resolves it
against the saved skip state before dispatching the existing skip/cancel action,
including optional display navigation. Explicit physical, automation and phone
Skip/Cancel commands retain their existing meanings. No durable-record format,
settings, credentials, prayer-consumption policy or recording changes are needed.

## Automated checks

- The production and isolated settings bridge regression failed before the change:
  `f.device.status().skip == next` was false after the first toggle. Both passed
  after the change, with and without the pinned ArduinoJson/local-API bridge.
- Adapter cases cover repeated toggle holds, restart persistence, unchanged saved
  records after restoring, both display layouts and connection-page dismissal,
  absent displays, failed skip/restore commits, setup gating and maintenance.
- All 13 scheduler/display schema cases, four Waveshare cases and three
  AUDIO-Board cases passed again after rebasing onto the v0.11.0 release base. Generated C++ checks verify the short Stop gesture
  and one 2–5 second toggle gesture on each production board, with no seven-second
  restore gesture. Waveshare diagnostic dependency downloads required a rerun
  with network access after sandbox DNS prevented the initial run.
- Four UBSan CTests passed: scheduler scenarios, NVS durability, settings
  durability and display/backlight behavior.
- Changed-document relative links and `git diff --check` passed.

## Matched firmware measurements

Baseline: `07eb37d6ce7cf817bc1200a26ce163d9fd044634`. Measurements use the same
configuration, build directory and pinned Python 3.13.0, ESPHome 2026.9.0,
ESP-IDF 5.5.5, Xtensa GCC 14.2.0 (`esp-14.2.0_20260121`) and tzdata 2026.4.
Only public compile-only settings and synthetic audio fixtures are used. These
images must never be installed or played.

All 17 affected configurations passed ESPHome configuration validation and the
existing dependency, partition, factory/OTA equality and audio-capacity checks
before and after the change. Both 2 MiB application slots and the separate
3.5 MiB shared audio partition remain unchanged. Production images each shrink
by 48 bytes. Standalone scheduler development images grow by 128–144 bytes
when switching their direct action to the shared physical-control dispatcher.

| Configuration | Before OTA bytes | After OTA bytes | Delta | Remaining 1,572,864-byte budget | Static RAM delta |
| --- | ---: | ---: | ---: | ---: | ---: |
| audio-board-development | 1,357,424 | 1,357,376 | -48 | 215,488 | -40 |
| audio-board-production | 1,401,072 | 1,401,024 | -48 | 171,840 | -40 |
| audio-board-startup-failure | 1,402,048 | 1,402,000 | -48 | 170,864 | -40 |
| audio-board-upgrade | 1,402,048 | 1,402,000 | -48 | 170,864 | -40 |
| box-audio | 1,330,144 | 1,330,096 | -48 | 242,768 | -40 |
| box-diagnostics | 1,481,040 | 1,480,992 | -48 | 91,872 | -40 |
| box-display | 1,393,712 | 1,393,664 | -48 | 179,200 | -40 |
| box-dua-qualification | 1,400,032 | 1,400,000 | -32 | 172,864 | -40 |
| box-production | 1,437,424 | 1,437,376 | -48 | 135,488 | -40 |
| device | 1,130,176 | 1,130,320 | +144 | 442,544 | -40 |
| dua-qualification | 1,396,224 | 1,396,176 | -48 | 176,688 | -40 |
| provisioning-validation | 1,424,528 | 1,424,464 | -64 | 148,400 | -40 |
| quran-qualification | 1,483,504 | 1,483,456 | -48 | 89,408 | -40 |
| reference | 1,433,600 | 1,433,552 | -48 | 139,312 | -40 |
| upgrade-qualification | 1,434,544 | 1,434,480 | -64 | 138,384 | -40 |
| upgrade-startup-failure | 1,434,544 | 1,434,480 | -64 | 138,384 | -40 |
| validation | 1,147,856 | 1,147,984 | +128 | 424,880 | -40 |

Every generated adapter matches the candidate runtime source. Generated button
wiring contains the 2–5 second toggle and excludes the old 6–10 second gesture
in every configuration. These SHA-256 hashes identify the candidate inputs:

| Runtime input | SHA-256 |
| --- | --- |
| `firmware/esphome/components/openathan/__init__.py` | `f884a2d4ea06dadde4c7ae7a737d582b257377da1bf34da4b0f3e97ef297fc72` |
| `firmware/esphome/components/openathan/openathan.cpp` | `10f1e1e0b50b70d5a6b8170f82e1a66f421d98c9400de0ffe21e93909b29aba1` |
| `firmware/esphome/openathan.yaml` | `6a0b9cbd4d59686d3b75a3841c4a6e5ecfe79551068735de5415a313750cb797` |
| `firmware/esphome/packages/hardware/waveshare-audio-board.yaml` | `ca057d8127ba5b12b2360a08ae77e70e4552a37c80c683b25dd0533910d62224` |
| `firmware/esphome/packages/hardware/waveshare-box-v2.yaml` | `d662b658864fdf2ff7e53d42ae93d4f6cb18aae9a98cfaa787f45c192f916512` |
| `firmware/esphome/scheduler/development.yaml` | `ba1a4fdf56f74a4eca3da405d17e409f817b862daea731bfe59a4db334251d5e` |

## Hardware and runtime limits

No speaker was accessed, installed, restarted or played for this change. The new
toggle gesture has automated coverage; physical timing, feedback and preservation
on these exact images remain untested. Earlier dated separate-hold hardware
results remain historical evidence. Runtime heap, fragmentation and PSRAM were
not measured. These compile-only builds do not establish release readiness.
