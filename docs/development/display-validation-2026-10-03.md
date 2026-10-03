# Optional status display — 2026-10-03

The reference development candidate adds a dim, always-on 128×128 GC9107
AtomS3R display. It observes existing scheduler/setup/network state and saved
timezone rules; it changes no prayer calculations, durable records, application
slots, shared audio, front-button controls or public HTTP payloads.
See the [display contract](../../firmware/esphome/components/openathan_display/README.md).
Connected screens leave the footer blank; Wi-Fi loss shows `Offline`, and
clock-unavailable states retain connection/synchronization guidance.
This capability is not in the published v0.2.1 release.

This report retains the original rotation-180 source and measurements. The
[orientation correction](display-orientation-validation-2026-10-03.md) changes
the reference default to rotation 0 so text faces the side opposite the
Pyramid's power/expansion ports. That direction awaits physical confirmation
at the next attended candidate installation.

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

The existing browser interface and HTTP contracts are unchanged. Browser
regressions passed in CI; live-device browser checks are recorded below.
Existing scheduler-only firmware variants do not include the optional display
profile.

## Attended isolated hardware acceptance

The 2026-10-03 session tested an isolated maintainer image based on display
source `679217f926e9e865a09150173c58e1146243dd62` on the previously verified
GC9107 AtomS3R + Voice Pyramid, using bottom-only power. The private image adds
an encrypted native telemetry API, FreeRTOS task-stack enumeration and RAM
redraw counters. Production excludes this instrumentation. Its actual OTA is
1,316,960 bytes, with 255,904 bytes of application budget remaining.

Observed results, separate from the host fixtures above:

- The operator confirmed upright text, normal-distance readability and
  comfortable dimness at the 10% default. Cold power-on showed clock waiting,
  followed by the synchronized saved Toronto time and disabled-prayer screen.
- The next-prayer screen, three-second front-button skip, seven-second cancel,
  playback screen and brief-tap stop passed on the device and in status reads.
- Both retained, approved recordings completed clearly with normal display
  transitions. Fajr telemetry spanned 270.001 seconds for its 268.069-second
  recording. The normal recording is 233.548 seconds; the first idle read was
  237.008 seconds after its isolated scheduled deadline.
- During a 90.042-second device-only Wi-Fi outage, the operator observed the
  Offline footer, advancing clock and scheduled normal playback before Wi-Fi
  returned. Reconnection retained clock/readiness, one boot identity, no fault
  and exactly one Asr consumption watermark advancement. No clock or history
  was rewound; the scheduled test used isolated location/offset settings.
- The actual device page loaded at a 390-pixel browser width during audio.
  Browser skip/cancel returned HTTP 200; Stop returned HTTP 200 and reached
  idle on readback. Stop acknowledgment can precede the asynchronous audio
  adapter's idle transition; the browser check waits for both.
- Across 1,840.018 seconds and 313 distinct telemetry samples, the image had
  54 redraws, no observed reset, no display/backlight failure and no reported
  audio glitch. The operator confirmed both complete recordings and returns.

The original orientation check did not define the Pyramid's ports as the back.
The operator subsequently reported that text faced the port side. Thus the
observations above establish readability at rotation 180 from the tested viewing
side; they do not qualify the corrected rotation-0 front-facing direction.

| Runtime measure, bytes | Initial | Minimum | Final quiet |
| --- | ---: | ---: | ---: |
| Sampled free internal heap | 242,740 | 215,704 | 239,036 |
| Cumulative minimum internal heap | 236,796 | 167,900 | 167,900 |
| Largest internal block | 196,608 | 172,032 | 192,512 |
| Free PSRAM | 8,355,820 | 7,214,860 | 8,354,128 |
| Largest PSRAM block | 8,257,536 | 7,077,888 | 7,208,960 |

Minimum unused stack headroom, in pinned ESP-IDF bytes: loop 3,054; HTTP 5,644;
audio reader 4,148; decoder 1,728; speaker 2,976. These are observed task-specific
high-water marks, not a guarantee for every possible load.

Free PSRAM returned close to baseline, but its largest block remained exactly
1 MiB below the initial value. Retain that fragmentation result; this bounded
run does not establish long-soak behavior or maximum-load production headroom.
The encrypted API/trace instrumentation changes allocations, so these numbers
qualify this isolated image rather than the ordinary production binary. No
physical fault injection or destructive setup/history change was used; those
screens remain covered by host previews. Newer ST7735 units remain outside the
profile's qualification.

Fresh paired USB reads before installation and after testing verified exact
image identity and preserved production record payloads, shared audio,
bootloader, OTA selection and inactive application. Test settings were returned
to their original all-prayers-off values with monotonic test history retained.
The captured exact published v0.2.1 application was restored through a guarded
application-only write and independent readback. No factory image, synthetic
recording, bootloader, partition, credential or historical NVS restore was used.

[Machine-readable hardware results](display-hardware-2026-10-03.json) retain the
source binding, runtime measurements and limits. Private evidence retains raw
traces, compatible recovery, readbacks, listener reports and corrected harness
oracles; no private credentials or recovery images are committed.
Firmware publication and website adoption remain separate release work. Use
this source-bound result with the [hardware runbook](../../firmware/esphome/provisioning/HARDWARE_TEST.md#optional-gc9107-status-display-acceptance)
when selecting future acceptance checks.
