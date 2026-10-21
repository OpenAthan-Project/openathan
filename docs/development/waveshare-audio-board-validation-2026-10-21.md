# AUDIO-Board software validation — October 21, 2026

This report records software/build evidence, not physical hardware acceptance.
The AUDIO-Board PCB revision is unverified. No flash writes, installation,
playback, RTC test or update/recovery session was performed for this report.

## Pinned builds and capacity

Baseline: main `cbb0d8ee6a803568b8a3a1282d0bb28e840234c6`; same
ESPHome 2026.9.0 / ESP-IDF 5.5.5 configuration and dependency pins before/after.
All measured profiles use the unchanged partition layout and pass the capacity
checker, including factory/OTA consistency and absence of recording payloads in
application images. Both old-board static RAM measurements are unchanged.

| Profile | OTA bytes | Byte delta | Application budget remaining | Static RAM bytes |
| --- | ---: | ---: | ---: | ---: |
| Atom production | 1,401,440 | -16 | 171,424 | 117,259 |
| Box V2 production | 1,405,312 | 64 | 167,552 | 117,455 |
| AUDIO-Board production | 1,369,168 | New profile | 203,696 | 117,459 |
| AUDIO-Board isolated development | 1,349,552 | New profile | 223,312 | 117,459 |

Every image is below 1,572,864 bytes and retains more than 512 KiB free inside
its 2 MiB slot. Synthetic compile-only fixtures were used; these builds must
never be installed or played. Runtime heap, fragmentation and PSRAM availability
cannot be inferred from these static figures.

## Automated evidence

The CMake/UBSan host suite passes 23 tests, including amplifier sequencing and
write/readback failures, unrelated expander-bit preservation, held keys and failed
reads, debounce/no-repeat, and RGB output failure. Production/isolated settings
bridge tests cover physical volume persistence, bounds and failed commits.
Profile generation checks verify wiring, light/playback/RTC bindings, hardware
identity and isolated-update denial. Release regressions include shared-media
three-board bundles, older single/two-board compatibility and signatures that
cannot cross between the two 16 MiB boards.

The complete pinned Python suite passes 165 tests. All three profiles pass
legacy and phone-setup producer fixtures through the exact pinned website
consumer. The website separately passes 207 unit tests and 513 Chromium/WebKit/
mobile browser cases, with three intentional skips. Its check, formatting,
build and static-output checks pass. Desktop and narrow-screen handoff captures
were reviewed. The initial reused-preview-server USB-uncertainty failures did
not reproduce on fresh isolated servers; original failed logs were retained. No simulator opens Web Serial.
Physical phone/USB installation and recovery remain pending; see the
[qualification procedure](waveshare-audio-board.md#hardware-qualification).
