# Reference display orientation — 2026-10-03

The reference build defines its front as the side opposite the Pyramid's power
and expansion ports. The GC9107 profile changes from rotation 180 to rotation 0
so the display is intended to be upright from that side, with the cord at the
back. Brightness, layout, polling, controls and public HTTP payloads are unchanged.

## Automated validation and capacity

The pinned ESPHome code-generation regression requires the default 0-degree
orientation. ESPHome omits the setter for zero and initializes the driver to
zero; nonzero rotations emit a setter. The regression passes for the corrected
profile and rejects the original rotation-180 profile. All 103 Python tests pass
without skips, and all nine CI configuration schemas validate.

Matched before/after measurements use Python 3.13.0, ESPHome 2026.9.0,
ESP-IDF 5.5.5, `esp-14.2.0_20260121` and tzdata 2026.4. The retained rotation-180
baseline artifacts were independently rechecked against their recorded hashes
and capacity results. Their firmware inputs match main
`162bfc73611e102de40eb4296b13db519b165bc8`; the corrected export changes only
the board profile's rotation and explanatory comment. Both sides use the same
compile-only settings, synthetic audio fixture and qualification trust inputs.
Project/version labels are matched for each pair.

Actual `firmware.ota.bin` sizes:

| Variant | Before, 180° | After, 0° | Delta | Remaining 1.5 MiB budget |
| --- | ---: | ---: | ---: | ---: |
| Reference | 1,249,424 | 1,249,408 | −16 | 323,456 |
| Isolated provisioning | 1,251,200 | 1,251,184 | −16 | 321,680 |
| Upgrade qualification | 1,256,640 | 1,256,624 | −16 | 316,240 |
| Forced startup rollback | 1,256,640 | 1,256,624 | −16 | 316,240 |

All eight before/after images pass the existing capacity checker, including
dependency pins, factory/OTA consistency, component exclusion and unchanged
dual-2-MiB-slot/shared-3.5-MiB-audio layout. Static RAM is unchanged: 115,123 bytes
for reference/isolated and 115,195 bytes for both qualification variants.
[Machine-readable measurements](display-orientation-build-2026-10-03.json)
retain source digests, actual OTA hashes and budgets. Measurement images use
synthetic, non-playable audio fixtures and must not be installed or played.

## Physical evidence and remaining check

The [original attended run](display-validation-2026-10-03.md#attended-isolated-hardware-acceptance)
used rotation 180 and confirmed readability from the operator's viewing side.
The operator subsequently reported that the text faced the ports. The original
source, audio/control observations and runtime measurements remain intact; they
do not establish ports-at-back orientation for the corrected source.

Physical confirmation of rotation 0 is pending the next attended candidate
installation. With only Pyramid bottom power connected, place its power and
expansion ports away from the viewer and confirm upright, readable clock and
next-prayer text from the opposite side. Use the existing
[preserving hardware procedure](../../firmware/esphome/provisioning/HARDWARE_TEST.md#optional-gc9107-status-display-acceptance)
and reuse accepted audio/control evidence within its source limits.
No hardware access, installation or release action occurred for this correction.
Long-soak/runtime-memory limits and unqualified ST7735 revisions remain as
recorded in the original report.
