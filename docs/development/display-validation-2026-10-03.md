# Optional status display — 2026-10-03

The reference development candidate adds a dim, always-on 128×128 GC9107
AtomS3R display. It observes existing scheduler/setup/network state and saved
timezone rules; it changes no prayer calculations, durable records, application
slots, shared audio, front-button controls or public HTTP payloads.
See the [display contract](../../firmware/esphome/components/openathan_display/README.md).
Connected screens leave the footer blank; Wi-Fi loss shows `Offline`, and
clock-unavailable states retain connection/synchronization guidance.
This capability is not in the published v0.2.1 release.

## Matched firmware measurements

Before/after builds use main `bf3b200e549dbfc9b216a27df0e04ae982855481`,
Python 3.13.0, ESPHome 2026.9.0, ESP-IDF 5.5.5,
`esp-14.2.0_20260121` and tzdata 2026.4. Both sides use the same configuration
and project/version labels except the intended display additions. Disposable
checkouts contain public compile-only settings and synthetic audio fixtures.
The qualification/rollback variants retain isolated records and private-test
trust profiles. None of these measurement images is installable release media.

Actual `firmware.ota.bin` bytes, not linked-size estimates:

| Variant | Before | After | Delta | Remaining 1.5 MiB budget |
| --- | ---: | ---: | ---: | ---: |
| Reference | 1,211,680 | 1,249,424 | +37,744 | 323,440 |
| Isolated provisioning | 1,213,504 | 1,251,200 | +37,696 | 321,664 |
| Upgrade qualification | 1,218,992 | 1,256,640 | +37,648 | 316,224 |
| Forced startup rollback | 1,218,992 | 1,256,640 | +37,648 | 316,224 |

Every affected before/after image passes the existing capacity checker:
dependency pins, factory/OTA consistency, production/isolated component
exclusion and unchanged dual-2-MiB-slot/shared-3.5-MiB-audio partition contract.
The candidate reference has 847,728 free bytes in each application slot.
Reference static RAM increases from 113,351 to 115,123 bytes (+1,772).
The two qualification variants increase from 113,431 to 115,195 bytes (+1,764).
The current-main measurement baseline differs from the historical published
v0.2.1 bytes; the deltas above use matched builds rather than subtracting a
previously published artifact.

[Machine-readable measurements](display-build-2026-10-03.json) retain OTA hashes,
source/config digest, toolchain identity, budgets and static RAM for each pair.
The runtime-source digest binds the measured component/configuration bytes,
excluding documentation. Later source changes require new measurements.

## Automated and visual validation

- After removing the connected footer, all 12 CTests and both real settings
  adapter tests were repeated successfully, alongside all four firmware builds.
- All 12 CTests pass with UndefinedBehaviorSanitizer.
- All 103 Python tests pass without skips, including real production/isolated
  settings adapters, pinned timezone conversion and generated-display wiring.
- All nine CI configuration schemas validate; four affected candidate firmware
  variants and their matched baselines compile and pass capacity checks.
- The real renderer generates 128×128 fixture previews for idle, playback,
  offline, skip, setup, waiting-time, disabled-prayer, missing-time, not-ready
  and storage/audio/schedule faults. Visual review found no clipping or blocking
  copy/readability defect. This is framebuffer review, not physical dimness.
- Adapter scenarios cover spring/autumn Toronto DST, midnight, unchanged
  refreshes, continued readiness when offline and scheduled playback with a
  failed optional display. Backlight tests stop on each failed I2C write.
- Code-generation regression requires the explicit drawing callback and 8-bit
  framebuffer, prevents an implicit test card, and rejects brightness outside
  1–100. Brightness defaults to 10% at build time.

The existing browser interface and HTTP contracts are unchanged; local browser
tests were not repeated for this framebuffer-only change. Existing scheduler-only
firmware variants do not include the optional display profile.

## Hardware limits and next acceptance

No device was accessed, installed, restarted, played or modified for this work.
Earlier source-bound evidence established that this unit's GC9107 screen and
LP5562 helper could display diagnostic rows. It does not establish this new
integrated adapter's dimness, orientation, stability or audio concurrency.
Newer ST7735 AtomS3R revisions are outside this profile's qualification.

The driver allocates a 16 KiB 8-bit framebuffer. Static RAM figures do not prove
runtime heap, largest free block/fragmentation, PSRAM or stack headroom during
audio, networking and redraws. Those observations remain pending in a separately
coordinated isolated-maintainer session. Follow the [hardware runbook](../../firmware/esphome/provisioning/HARDWARE_TEST.md#optional-gc9107-status-display-acceptance),
preserve actual current settings/history/audio/recovery and use an application-only
transition. Do not install synthetic CI fixtures or restore historical prayer
history. Firmware publication and website adoption remain separate release work.
