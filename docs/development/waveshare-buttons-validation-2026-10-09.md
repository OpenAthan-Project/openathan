# Waveshare BOOT controls — automated validation, 2026-10-09

The isolated Waveshare V2 profiles map BOOT to the reference Atom's stop,
skip-next and cancel-skip actions. The [profile](waveshare-box-v2.md#physical-controls)
records GPIO0 wiring, release timing and recovery behavior. Physical button
acceptance remains pending; these results establish source/build behavior only.

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

## Hardware limits

No device was accessed or flashed for these checks. BOOT stop/skip/cancel timing,
restart/download-mode recovery and runtime memory during button/audio/network
activity require a separately approved attended application-only installation.
Preserve current settings, consumption history and shared audio under the linked
runbooks. Synthetic CI recordings and these compile-only images must not be
installed or played. This change does not qualify a public release.
