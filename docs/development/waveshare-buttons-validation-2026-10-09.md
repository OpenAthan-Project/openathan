# Waveshare BOOT controls — validation, 2026-10-09

The isolated Waveshare V2 profiles map BOOT to the reference Atom's stop,
skip-next and cancel-skip actions. The [profile](waveshare-box-v2.md#physical-controls)
records GPIO0 wiring, release timing and recovery behavior. Automated checks and
attended stop, skip, cancel-skip and RESET persistence checks passed on one V2
unit. Exact timing boundaries and physical download-mode recovery remain untested.

## Firmware capacity

Baseline source was main `d32eedf11b3a8ccae94bc1a58704d1199c1f2eb4`.
Baseline and candidate used identical public compile-only configuration/fixtures,
ESPHome 2026.9.0, ESP-IDF 5.5.5 and the pinned Xtensa toolchain.
Every paired build passed the existing dependency, partition, image-header,
factory/OTA consistency and shared-audio capacity checks.

| Variant | Baseline OTA bytes | Candidate OTA bytes | Delta bytes | Remaining application budget bytes |
| --- | ---: | ---: | ---: | ---: |
| Waveshare audio only | 1,207,680 | 1,210,544 | +2,864 | 362,320 |
| Waveshare round display | 1,256,256 | 1,258,896 | +2,640 | 313,968 |
| Waveshare encrypted diagnostics | 1,342,144 | 1,345,536 | +3,392 | 227,328 |
| Atom reference | 1,274,880 | 1,274,880 | 0 | 297,984 |

The application limit remains 1,572,864 bytes within each 2 MiB slot. Both slots
and the separate 3.5 MiB shared audio partition are unchanged. Static RAM increased
by 308 bytes for Waveshare audio and 316 bytes for its display/diagnostic variants;
static figures do not establish runtime heap, fragmentation or PSRAM headroom.

## Automated checks

- All 17 C++ CTests passed with UndefinedBehaviorSanitizer.
- All 105 Python tests passed, including production/isolated settings bridges
  using the resolved ArduinoJson library and pinned ESPHome timezone code.
- All 12 firmware configurations passed schema validation.
- Generated Waveshare GPIO0 pull-up/inversion and scheduler action bindings
  passed regression checks. Generated click windows/actions matched Atom exactly;
  the gaps between configured windows have no assigned action.
- The real display adapter rendered the correct button/phone guidance for both
  square and round layouts, with unchanged-frame refresh suppression.

## Attended button acceptance

The attended builds used source
[`a728f3c214883d38e53ed43c88c4d9d8fa28e8c2`](https://github.com/OpenAthan-Project/openathan/commit/a728f3c214883d38e53ed43c88c4d9d8fa28e8c2),
the same pinned toolchain, private configuration and approved real shared audio.
The public compile-only images and synthetic fixtures above were not installed.

| Physical action | Observed result |
| --- | --- |
| BOOT about 0.5 seconds, released during normal Athan | Audible playback stopped; “Button to stop” guidance was correct. |
| BOOT about 3 seconds, released | The next Fajr was saved as skipped; “Will be skipped” appeared. |
| Brief RESET press | A new boot returned synchronized and ready, retaining the skip and saved state. |
| BOOT about 7 seconds, released | The skip cleared and normal next-prayer guidance returned. |

Hold times were approximate. Stop acceptance uses operator audible confirmation
and subsequent idle readback; the first playing-state HTTP read returned after
the physical stop. Settings, consumption history, preferences and setup matched
after the tests and final startup, with no skip remaining.

Both applications passed capacity inspection and were installed with one
application-only write each. Independent complete 16 MiB readbacks verified the
exact image and every byte outside its application erase extent before restart.
Bootloader, partitions, OTA metadata, inactive application and shared audio were
preserved. Private diagnostics and recovery evidence remain outside Git.

- Temporary diagnostic OTA: **1,345,536 bytes**, SHA-256
  `4cd264a29762ec042272d65dd7ccdeabb88015221cb99e0d46a0f1afdeaad5c5`.
- Final clean development OTA: **1,258,896 bytes**, SHA-256
  `5ec0dd504024db3fb5a1e70fadc81937354d24d438c8b23e81821d05b8552322`.
  Growth over the previous clean image is **2,640 bytes**, leaving **313,968 bytes**
  within the application budget. The diagnostic native API is excluded.

The final clean-image screen was confirmed upright, with saved brightness,
12-hour time and no skipped-prayer message. Compatible application-only USB
recovery is retained; the inactive vendor image remains an unqualified fallback.

## Runtime observations and limits

The temporary diagnostic image supplied 32 encrypted telemetry samples during
brief playback, network/control activity and one planned physical RESET.
Minimum sampled internal free heap/largest block were **217,576/176,128 bytes**;
the reported internal low-water mark was **207,000 bytes**. Minimum sampled PSRAM
free/largest block were **6,973,060/6,815,744 bytes**. No codec/display fault sample
or unexpected uptime regression was observed.

These samples do not establish continuous peak use, fragmentation bounds or
clean-image runtime headroom. Exact click boundaries and unassigned gaps,
BOOT-held physical download-mode recovery and clean-image runtime memory remain
untested. No full listening, scheduled occurrence, cold-power or hour-long soak
was repeated; [earlier evidence](waveshare-validation-2026-10-09.md) retains its
source limits. The [intermittent HTTP finding](waveshare-network-development-2026-10-09.md)
remains unresolved. Public updates remain disabled; this development acceptance
does not qualify a public release.
