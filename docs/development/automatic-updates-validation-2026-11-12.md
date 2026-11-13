# Automatic firmware updates validation — 2026-11-12

The implementation extends the existing signed application updater with default-on
automatic installation, a durable local opt-out and cancellation/rejection
suppression. See the [behavior and API contract](firmware-upgrades.md).
The [unreleased notes](../releases/unreleased.md) describe the owner-facing change.
Existing speakers need one preserving update to receive this feature.

The measurements and source hashes below describe the initial implementation at
`98dda2aced3d12846cb972ce0e0afe7031fe24c9`. The
[description-read retry follow-up](automatic-update-description-retry-2026-11-12.md)
records the subsequent correction and its validation.

## Source and compatibility

Baseline: `d2f03bb953936cdb53efc92a2743ae56534f002b`. The candidate is identified
by the runtime source hashes below. Both stages retain the development identity
and declared v0.10.0 version; these are compile-only measurements, not a release.
No new release version, tag, signature or publication is part of this validation.

The schema-1 Wi-Fi and schema-2 USB journals, signed release payload, production
public key, bootloader allowlist, settings/audio formats and partition layout are
unchanged. The additive automatic preference record stays in the updater namespace.
Corrupt automatic metadata does not prevent prayer scheduling or startup confirmation.

## Automated checks

- All 646 Chromium/WebKit device-UI and phone-setup cases passed, with no failures, skips or cancellations.
- All 180 pinned Python tests and 26 UBSan CTests passed.
- The production updater/USB/HTTP host matrix passed under UBSan, including ordinary isolated, disabled-update and healthy/failed-startup qualification variants.
- Automatic-policy cases cover default-on migration, saved opt-out, manual ownership, safe-window boundaries, setup readiness after queue restoration, transport retry, cancellation during download, both metadata/request commit cuts, corrupt storage, checksum/image/version rejection and rollback suppression.
- Browser cases cover saved and uncertain toggles, stale replies, revision conflicts, automatic download cancellation, manual installation, unsupported/older firmware, isolated storage faults and narrow/enlarged layouts. Production Digest verification is used with simulated device APIs.
- Desktop and phone renders were inspected. The mechanical UI detector reported no deterministic findings; this does not establish assistive-technology or physical-device acceptance.
- Whitespace, JavaScript syntax and changed-document relative links passed.

## Matched firmware measurements

Both stages use Python 3.13.0, ESPHome 2026.9.0, ESP-IDF 5.5.5, Xtensa GCC
14.2.0 (`esp-14.2.0_20260121`) and tzdata 2026.4. Each pair uses the same
configuration, SDK settings and public compile-only fixtures. No dependency pin
or partition budget changed. The synthetic audio fixtures must never be installed
or played.

All 15 affected UI-bearing variants passed the existing dependency, partition,
factory/application equality and capacity checks. All three current compressed
UI assets were verified against generated source and actual OTA bytes in every
candidate. Generated updater sources also match the final source hashes below.
An incomplete generated component cache required one fresh qualification build;
only successful completed builds contribute to these measurements.

| Variant | Before OTA bytes | After OTA bytes | Delta | Remaining 1,572,864-byte budget | Static RAM delta |
| --- | ---: | ---: | ---: | ---: | ---: |
| AtomS3R / Voice Pyramid | 1,418,400 | 1,426,560 | +8,160 | 146,304 | +64 |
| audio-board-development | 1,355,792 | 1,357,040 | +1,248 | 215,824 | +64 |
| audio-board-startup-failure | 1,386,880 | 1,394,912 | +8,032 | 177,952 | +64 |
| audio-board-upgrade | 1,386,880 | 1,394,912 | +8,032 | 177,952 | +64 |
| Waveshare AUDIO-Board | 1,385,984 | 1,394,048 | +8,064 | 178,816 | +64 |
| Waveshare Box V2 | 1,422,208 | 1,430,432 | +8,224 | 142,432 | +64 |
| dua-qualification | 1,394,576 | 1,395,856 | +1,280 | 177,008 | +64 |
| provisioning-validation | 1,410,064 | 1,417,536 | +7,472 | 155,328 | +64 |
| quran-qualification | 1,481,888 | 1,483,120 | +1,232 | 89,744 | +64 |
| upgrade-qualification | 1,419,264 | 1,427,424 | +8,160 | 145,440 | +64 |
| upgrade-startup-failure | 1,419,264 | 1,427,424 | +8,160 | 145,440 | +64 |
| waveshare-audio | 1,328,496 | 1,329,776 | +1,280 | 243,088 | +64 |
| waveshare-diagnostics | 1,479,440 | 1,480,672 | +1,232 | 92,192 | +64 |
| waveshare-display | 1,392,080 | 1,393,344 | +1,264 | 179,520 | +64 |
| waveshare-dua-qualification | 1,398,400 | 1,399,680 | +1,280 | 173,184 | +64 |

Both 2 MiB application slots and the separate 3.5 MiB shared audio partition
are retained. Every application leaves at least 512 KiB free in its slot.
Scheduler-only device/validation configurations do not include the changed
product updater or embedded UI; they are covered by host/configuration tests.

## Hardware and runtime limits

No speaker was accessed, flashed, reset, played or updated for this implementation.
Runtime heap, fragmentation, worker stack and PSRAM have not been measured for
this change. Automated builds and browser simulations do not establish physical
update/recovery, audible playback or release readiness.

Before release, follow the [attended upgrade runbook](../../firmware/esphome/upgrades/VALIDATION.md)
for unattended installation, opt-out during transfer, failed-startup recovery,
repeated-release suppression and preservation. Capture bounded runtime memory
and concurrent playback/networking evidence with approved recordings. Hardware
installation and stable/latest publication remain separate steps.

## Runtime source hashes

| File | SHA-256 |
| --- | --- |
| `firmware/esphome/components/openathan_device/automatic_updates.cpp` | `5f1fe2902cb9410f60087e4b35080d66871f0b51a74b9907d3f9e5bfcaa9a01a` |
| `firmware/esphome/components/openathan_device/upgrade.cpp` | `449e525b3948ec502c75ebbdbc73e330b5367ba91ff6966a0ebc6ffcf9e684e6` |
| `firmware/esphome/components/openathan_device/upgrade.h` | `abea06460399ddffc6cba8abe05425c4852163627c6facc9920759f446ddffbc` |
| `firmware/esphome/components/openathan_device/usb_upgrade.cpp` | `25ea99b10fd32b18f90dd0106c17d33293392a4bf4357d33c01e7a8d97536a50` |
| `web/device-ui/index.html` | `dc5cc96c60fed2b7035fbdfc5a54a8c22e7a40fed97b190e2a49d6c41507e934` |
| `web/device-ui/app.js` | `b29c1c3e158e8c31dd82b4ac86f00c9f9f5ee3cdcb2cd17bea2508c65b5a9e9b` |
| `web/device-ui/style.css` | `366aa9b60529bd8e978026c3a3240d4b6882c6f556218916bd15d2b1441a0b06` |
