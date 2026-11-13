# Firmware description read retries, 2026-11-12

A failed read of a downloaded firmware description previously marked the release
as rejected and cleared its request. A later successful read or restart did not
restore automatic installation. The updater now retains the request and uses its
existing five-minute retry delay. A successfully read version mismatch still
suppresses the release.

## Source and behavior

Before: `98dda2aced3d12846cb972ce0e0afe7031fe24c9`.
Runtime fix: `93c0861a4ae74cb9e38eb015ff1d4be179933d54`.
The [initial automatic-update validation](automatic-updates-validation-2026-11-12.md)
remains evidence for its recorded source and measurements.

The correction separates the description-read result from the version comparison.
Image validation still completes first. A read failure does not stage an image,
select a boot partition, change automatic/manual ownership, or modify either
saved updater record. A retry must pass the existing safe-window and version
checks before boot selection. Cancellation, permanent rejection, signed release
verification, startup confirmation and rollback keep their existing paths.
No schema, partition, dependency or UI asset changes are included.

## Regression and host checks

The regression commit `af3c92f79f425ef3a0642d74adbfab7ef1c1eb16` fails on the previous
implementation with this output:

```text
Description read failure for automatic request: state=failed, skipped_version=v0.3.0, request_retained=0
```

The same regression passes after the runtime fix:

```text
Default-on automatic updates, opt-out, safe gaps, interruption and rejection suppression passed
Process return code: 0
```

The four recovery cases cover automatic and manual requests, both in the same
process and after a simulated restart. They check retained journals and policy,
no skipped version, no premature boot selection, the five-minute retry boundary,
and successful installation after reads recover. Checksum, image-validation and
actual version failures remain suppressed across restart for both request owners.

The complete `tools/check_upgrade_runtime.py` matrix passed under UBSan with
ArduinoJson 7.4.3. It compiled and ran 14 updater, USB, HTTP, isolated, disabled and
qualification binaries, and rejected three incomplete qualification profiles.
All 26 core UBSan CTests also passed. See the
[CI instructions](../../.github/workflows/README.md) for prerequisites and commands.

## Matched firmware measurements

All 15 affected variants passed the existing dependency, partition, image equality
and capacity checks. Each before/after pair uses Python 3.13.0, ESPHome 2026.9.0,
ESP-IDF 5.5.5, Xtensa GCC 14.2.0 `esp-14.2.0_20260121`, tzdata 2026.4, identical
SDK configuration bytes and the same public compile-only fixtures. Both stages
retain the declared v0.10.0 version and development build identity.

Archived before-images matched their capacity receipts and reviewed updater
sources. Final generated updater sources match the corrected source. All three
compressed UI assets match the source and actual OTA bytes in both stages.
An incomplete dependency cache interrupted one qualification build. Separate
per-variant caches resolved it; only completed successful builds appear below.

| Variant | Before OTA bytes | After OTA bytes | Delta | Remaining 1,572,864-byte budget | Static RAM delta |
| --- | ---: | ---: | ---: | ---: | ---: |
| atom | 1,426,560 | 1,426,640 | +80 | 146,224 | +0 |
| box | 1,430,432 | 1,430,496 | +64 | 142,368 | +0 |
| audio-board | 1,394,048 | 1,394,128 | +80 | 178,736 | +0 |
| provisioning-validation | 1,417,536 | 1,417,616 | +80 | 155,248 | +0 |
| upgrade-qualification | 1,427,424 | 1,427,520 | +96 | 145,344 | +0 |
| upgrade-startup-failure | 1,427,424 | 1,427,520 | +96 | 145,344 | +0 |
| waveshare-audio | 1,329,776 | 1,329,776 | +0 | 243,088 | +0 |
| waveshare-display | 1,393,344 | 1,393,344 | +0 | 179,520 | +0 |
| waveshare-diagnostics | 1,480,672 | 1,480,672 | +0 | 92,192 | +0 |
| quran-qualification | 1,483,120 | 1,483,120 | +0 | 89,744 | +0 |
| dua-qualification | 1,395,856 | 1,395,856 | +0 | 177,008 | +0 |
| waveshare-dua-qualification | 1,399,680 | 1,399,680 | +0 | 173,184 | +0 |
| audio-board-development | 1,357,040 | 1,357,040 | +0 | 215,824 | +0 |
| audio-board-upgrade | 1,394,912 | 1,395,008 | +96 | 177,856 | +0 |
| audio-board-startup-failure | 1,394,912 | 1,395,008 | +96 | 177,856 | +0 |

Both 2 MiB application slots and the separate 3.5 MiB shared audio partition are
retained. Every image leaves at least 512 KiB free in its application slot.
Synthetic audio fixtures must never be installed or played.

## Runtime source hashes

| File | SHA-256 |
| --- | --- |
| `firmware/esphome/components/openathan_device/upgrade.cpp` | `c46dafeccf61d67ccf275d7963265539ee461ef489190e164bc3cd9f0e5c1d06` |
| `tests/automatic_update_tests.cpp` | `e1e1d288968801fdc077fc63b9af2e28e00b3b9cc5cd0c78dc0c67da85b91c32` |

## Hardware and runtime limits

This validation did not access or update a speaker. It does not establish physical
installation, recovery, preservation or audible playback. Runtime heap,
fragmentation, stack and PSRAM measurements remain pending. These compile-only
images are not a release. Follow the
[attended upgrade runbook](../../firmware/esphome/upgrades/VALIDATION.md) before
hardware qualification. Publication and deployment remain separate steps.
