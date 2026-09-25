# Firmware capacity report — 2026-09-24

**The compile-time capacity target passed.** ESPHome, released Adhan C++, and
the shared audio adapter fit comfortably in each 2 MiB OTA slot. Both supplied
MP3 recordings fit unchanged in the 3.5 MiB data partition. The initial builds
were compile-only; subsequent device validation is recorded below.

## Measured results

The three builds use the same C126 + A167 board, MP3 speaker pipeline, Wi-Fi,
logger, OTA settings and partition table. Home Assistant/native API, device UI,
provisioning and scheduling are absent. See the [build instructions](../../firmware/esphome/feasibility/README.md).

| Variant | Actual OTA `.bin` bytes | Linked image bytes | Static RAM bytes | Free bytes per 2 MiB slot |
| --- | ---: | ---: | ---: | ---: |
| ESPHome/audio baseline | 927,616 | 927,503 | 111,047 | 1,169,536 |
| Plus prayer calculations | 990,400 | 990,287 | 111,247 | 1,106,752 |
| Plus shared audio and diagnostic controls | 998,912 | 998,791 | 112,983 | 1,098,240 |

Calculations add **62,784 application bytes**, including exception support and
all twelve preset factories. Shared audio and diagnostic controls add **8,512
bytes**. The full application is **573,952 bytes below the 1.5 MiB target**, and
leaves about **1.05 MiB** in each OTA slot. These are measured feasibility-build
sizes, not a forecast of the completed product.

Static RAM excludes runtime heap, task stacks, decoder buffers and dynamic
PSRAM allocations. Compilation alone did not prove sufficient MMU capacity;
the later device validation below established successful mapping and playback.

| Recording | Bytes | Sample rate / channels |
| --- | ---: | --- |
| Normal compressed MP3 | 1,246,749 | 44.1 kHz / stereo |
| Fajr compressed MP3 | 1,768,332 | 44.1 kHz / stereo |
| Total recordings | 3,015,081 | |
| Image used, including metadata and alignment | 3,019,180 | |
| Free audio-partition bytes | 650,836 | |

The padded audio image is exactly 3,670,016 bytes. Recordings and generated images
remain outside Git. No re-encoding occurred; content, stereo-to-mono playback
quality and distribution rights were not assessed by this build.

## Dependencies and evidence

- ESPHome **2026.9.0**, its recommended ESP-IDF **5.5.5**, and compiler
  **esp-14.2.0_20260121**. The chosen policy is latest compatible stable,
  rather than forcing newer upstream IDF 6.1 into this ESPHome release.
- Adhan C++ **v1.0.2**, commit `8d65a2906dbdb90a84d30d385a1b5f84e38d0c1f`.
  Vendored source and MIT license match the release checkout byte-for-byte.
  The original Batoul Apps MIT notice is also retained. C++20 and exceptions
  are enabled; no astronomy or upstream error-handling changes were made.
- ESPHome-selected managed components: esp-audio-libs **3.2.1**, micro-mp3
  **0.4.0**, mdns **1.12.0**. Exact content hashes are recorded in the
  [dependency manifest](../../firmware/esphome/feasibility/dependencies.json).
- The image inspector passed for all three builds: exact dependency versions
  and hashes, compiler pin, partition-table equality, application budget,
  matching factory/OTA application payload, and no supplied recording data
  blocks found in the application. The full real audio image also passed the
  C++ validator.

Full application SHA-256:
`91b9e24e8a6b5c9f1123587ba8f48fbdb99b544c2f0fd8f3aae9cad8ee5476cc`

Padded audio image SHA-256:
`c3a14d93e5521cefb8a4cbea86075403abcb153ec93c5481e651ea447873392b`

## Tests and limits

Host C++ reference tests passed for published prayer times, all twelve presets,
Standard/Hanafi Asr, rejected invalid/non-finite inputs and absent polar events.
Seven Python integration tests passed against the actual C++ image parser and
audio adapter. They cover deterministic unchanged payloads, missing/empty/large
inputs, bad hashes, truncation, erased flash, overflow/overlap, wrong IDs/codecs,
invalid partitions, mapping failures and persistent player-file pointers.
The same suite passed with UndefinedBehaviorSanitizer enabled.

The adapter contains no flash-write path. ESPHome's application OTA backend
selects the inactive OTA partition, while audio is a separate data partition.
The generated factory image ends before shared audio. This source/build
inspection supported the storage design. The later device tests below establish
slot switching and audio preservation. The dated rollback test below separately
verifies recovery from an unconfirmed application startup failure.

The reusable core, image builder, storage adapter and hardware configuration
can carry forward into the product. Fixed-date logs, measurement variants and
button diagnostics are temporary. The original next milestone was the standalone
prayer scheduler; its implementation and subsequent device results are recorded
below, including the separate automatic-rollback fault test.

## Scheduler integration — 2026-09-24

The [scheduler development build](../../firmware/esphome/scheduler/README.md) now
includes local calculation settings, SNTP/timezone integration, automatic
Fajr/normal playback, durable consumed-event watermarks, persistent skip/cancel,
stop controls, and state/fault logging. It uses the same pinned stable stack,
two 2 MiB application slots and 3.5 MiB shared audio partition. No firmware
dependencies were added. This initial integration was compile-only; later
installation and runtime testing are recorded in the next section.

| Initial scheduler build | OTA `.bin` bytes | Static RAM bytes | Free bytes per OTA slot |
| --- | ---: | ---: | ---: |
| Scheduler development | 1,009,344 | 112,143 | 1,087,808 |

The scheduler application is **563,520 bytes below the 1.5 MiB target**, leaving
about **1.04 MiB per OTA slot**. Its linked image size is 1,009,231 bytes. The
unchanged audio image still leaves 650,836 bytes free in its separate partition.
The size inspector confirmed dependency/component hashes, compiler pin, partition
layout, matching factory/OTA application payloads and excluded recording data.
The separate manual audio configuration also rebuilt successfully after the
playback-interface changes and passed the same capacity/dependency checks.

Scheduler application SHA-256:
`ef1da47b86b1478a9928e0999f7e720201aade4017386563c75bf157f9ec2eef`

The original table above records the earlier feasibility milestone, before the
scheduler and playback-interface changes. Subsequent runtime, playback,
persistence and OTA checks are recorded in the device-validation section below.

Three CTest targets pass: prayer reference calculations and input validation,
scheduler scenarios, and the production NVS adapter under injected storage
errors. Eleven Python tests pass, covering the existing audio format/adapter and
the real ESPHome schema, including required/matching timezones, finite location,
method/offset validation, typed playback bindings and action registration. The
C++ tests and host audio adapter also pass with UndefinedBehaviorSanitizer.

Scheduler scenarios cover both recordings, save-before-play ordering, active
audio replacement, stop without restart, missed events, normal lateness versus
clock correction, subsecond boundaries, first valid time, restarts, backward
date changes, altered offsets for consumed events, DST, month/year boundaries,
offsets across midnight, durable skips/cancellation, missing solar events, failed
calculations, corrupt/read-failed state, failed writes/commits and unavailable or
rejected playback. A real Adhan reference case is exercised through the scheduler.

## Integrated device validation — 2026-09-24

The reference AtomS3R/Voice Pyramid completed full normal and Fajr playback with
listener confirmation, including Fajr content. The user selected a 60% player
control level within the existing 60% output ceiling; ESPHome remaps these values,
so this corresponds to a 36% speaker setting. Both recordings completed without
decoder errors or resets, and heap/PSRAM were released after playback.
After validation, the requested firmware default was increased to a 70% player
control level (42% speaker setting within the same output ceiling). The listening
results above describe the tested 60% level.

The isolated device timetable passed scheduled Fajr/normal selection, durable
skip across physical power loss, cancel, repeated-skip idempotence, suppression
of the selected event, front-button stop without replay, scheduled replacement
of another track, restart without replay, and scheduled playback with Wi-Fi
disabled. A cold boot recorded 30 samples with invalid time and inactive audio
before connectivity returned. Software restart may retain the clock and is not
equivalent to this cold-start check.

Three application-only encrypted OTA transfers exercised both slots. The complete
3,670,016-byte audio partition hash remained unchanged. The real-schedule
developer build used at the end of validation excludes the synthetic component
and controls; its OTA image is
1,076,240 bytes, leaving 1,020,912 bytes in each 2 MiB app slot. Static RAM is
113,459 bytes. The image SHA-256 is
`fa78c7fd802189b0b121776d7c9c50d55335ff7b7a51cd607050fd2a1022ca0d`.
The subsequent 70% default-volume build has the same size figures and SHA-256
`63ede8bb3847914c1365aa16b3df319c38f249f7d2382c3bc0882e42b4bad7bd`.
Its fourth application OTA completed successfully; the saved 70% setting was
verified after reboot. These hashes identify the private device-validation
images, not later CI builds made with public compile-only settings.

Device-specific images, credentials, recovery backup, settings and raw evidence
remain in a private dated archive outside the repository. At this stage, actual
prayer-time observation and deliberate automatic-rollback fault injection had
not been performed.
See the [reusable validation procedure](../../firmware/esphome/scheduler/VALIDATION.md).

## Real scheduled Fajr observation — 2026-09-25

The listener confirmed that Fajr started at 05:49 EDT, matching the calculated
Barrie schedule with ISNA, Standard Asr, middle-of-night and zero offsets. The
recording played completely and clearly, and the 70% player volume was comfortable.
This completes the first real prayer-time playback observation; the earlier
full-recording listening results at 60% remain separate evidence.

Read-only encrypted diagnostics joined at 05:50:29 EDT and reported a valid
clock, automatic playback ready, active audio, 70% volume, no scheduler fault,
and Dhuhr at 13:11 EDT as the next event. The player returned to idle at
05:53:28.757 EDT, consistent with the approximately 268-second Fajr recording.
Monitoring continued through 05:54:28 EDT without replay or a scheduler fault.
Exact start latency was not captured because logging began after playback started.

The observation used the previously installed 70% device-validation image
identified above. No controls, clock changes, restart or firmware upload were
sent during observation. It does not establish device validation of the later
high-latitude conflict fixes.

## Automatic OTA rollback — 2026-09-25

The reference device passed a deliberate unconfirmed-startup failure test using
ESPHome 2026.9.0 and ESP-IDF 5.5.5. Two identical full-flash reads were archived
before testing. The working 70% image identified above occupied `app0` and was
marked `VALID`; the existing bootloader and partition layout were retained.

A separate 681,824-byte diagnostic application was installed through OTA into
`app1`. It contained no audio player or scheduler. USB logs confirmed
`PENDING_VERIFY` and an available rollback target, followed by a direct
`esp_restart()` about five seconds into startup, before the 60-second successful
boot confirmation. The probe neither confirmed its image nor selected another
partition. The original `app0` application reported `VALID` about 2.5 seconds
after the injected reset, without recovery flashing.

Full-flash readback confirmed `app1` was `ABORTED`, the entire original `app0`
slot was unchanged, and the bootloader, partition table, shared audio and
36-byte scheduler record were unchanged. After a bottom-port cold power cycle,
encrypted diagnostics again reported `app0` valid, 70% volume, no scheduler
fault, no duplicate Fajr playback and Dhuhr at 13:11 EDT as the next event. The
complete audio-partition hash also matched the pre-test value.

The listener then confirmed clear, comfortable normal-Athan audio at the saved
70% volume and a successful front-button stop. This approximately 55-second
check streamed the exact normal recording extracted from the verified backup
over the local network, because the installed real-schedule image exposes no
manual stored-track play control. It verifies restored audio output and stop
handling; it is not an additional scheduled or offline-playback test.

This establishes automatic recovery from an unconfirmed startup failure on the
previously installed device-validation image. It does not validate later
high-latitude firmware changes on hardware or recovery from faults after an
application has already been confirmed healthy. Backups, the diagnostic source
and image, and raw evidence remain in the private validation archive.

## Saved runtime settings — 2026-09-25

The AtomS3R/Voice Pyramid reference device was tested with ESPHome 2026.9.0,
ESP-IDF 5.5.5 and the pinned settings implementation. Application-only OTA
retained the existing partition layout. The synthetic timetable uses separate
settings and consumption records from the production schedule.

| Hardware-tested settings build (`b371b65`) | OTA `.bin` bytes | Static RAM bytes | Free bytes per 2 MiB slot |
| --- | ---: | ---: | ---: |
| Real schedule with encrypted developer controls | 1,117,488 | 113,651 | 979,664 |
| Isolated synthetic validation timetable | 1,141,200 | 114,363 | 955,952 |

Both builds passed the 1.5 MiB application target and retained the 3.5 MiB shared
audio partition. The settings JSON bridge adds pinned ArduinoJson 7.4.3. These
measurements precede any phone UI, provisioning or access-control implementation;
static RAM figures do not establish their future runtime headroom.

Physical bottom-power cuts preserved every settings field, decoded timezone
rules, revision and consumed-prayer watermark. Settings and volume were already
applied while the clock was invalid; the 30-second cold-start gate stayed silent.
An update interrupted before acknowledgment restored its complete new snapshot
without resending the write or losing consumption. The exact instant of the
flash write/commit relative to the cut was not observed.

After cutting power during a consumed Fajr, its time was moved three minutes
into the future. The clock synchronized before that time, and no replay occurred.
The next unconsumed prayer played on time after a volume-only save two seconds
before it was due. The listener confirmed silence at the moved time and clear,
continuous subsequent playback through volume and scheduling changes. Invalid
values, invalid ordering and stale revisions were rejected without mutation;
unchanged saves retained their revision, and skip identity survived reconfiguration.

Disabled and skipped prayers were consumed silently and did not replay after
being re-enabled or moved later. The device finished on the real schedule with
its intended settings, all prayers enabled, zero offsets and 70% volume. Its
production consumption history was retained, the next real prayer was correct,
and the complete audio-partition digest matched the pre-test image.

Automated validation passed four C++ suites under UBSan, 23 Python tests,
the production JSON bridge against ArduinoJson 7.4.3, six schema checks, and both
firmware builds with dependency, partition and capacity checks. Host injection
covers precise write/commit failures, shared Isha/Fajr identities, timezone/DST
changes, backward clocks and dropped volume requests. These cases were not all
repeated physically. Firmware hashes, complete readbacks, recovery backups and
raw device evidence are retained privately. The phone settings UI and release
qualification remain separate milestones.

Subsequent review found coordinate rounding in the JSON transport. The follow-up
preserves full stored precision during both parsing and export. Automated
regressions cover repeated export/import without writes, a volume-only save at
a prayer's due time, boundary coordinates and uncertain-response reconciliation.
This transport fix has not been repeated on the physical device; the hardware
results and build measurements above describe the earlier tested firmware.

## USB provisioning and local settings — 2026-09-25

The local implementation builds on saved-settings commit `b371b65`. Before
implementation, the copied worktree files and documentation handoff were compared
against the source checkout and reconciled to that exact commit. The source
checkout was retained unchanged. The reference `openathan.yaml` now builds USB
Wi-Fi/password provisioning, a bundled local settings UI/API, Digest access
protection, and a separate first-run activation record.

| Build | Application `.bin` bytes | Static RAM bytes | Free bytes per 2 MiB slot |
| --- | ---: | ---: | ---: |
| Provisioning and local UI reference candidate | 1,126,832 | 113,991 | 970,320 |
| Existing real-schedule developer configuration | 1,118,208 | 113,651 | 978,944 |

Both images passed pinned-dependency checks, the 1.5 MiB application target,
partition boundaries, and factory/application payload consistency. The compatible
shared-audio format was checked using generated CI audio; no release recordings
were selected or qualified. These static RAM figures do not measure live heap
headroom during simultaneous HTTP, Wi-Fi and audio operation.

Local validation passed:

- Five C++ suites under UBSan, including protocol framing, Digest expiry/replay,
  credential storage and interrupted activation boundaries.
- Thirty Python tests, including the production JSON API with ArduinoJson 7.4.3,
  scheduler/settings regressions, timezone/DST behavior and the USB console.
- All seven ESPHome configuration checks and both builds listed above, using
  ESPHome 2026.9.0, ESP-IDF 5.5.5 and tzdata 2026.4.
- Four browser tests across Chromium and WebKit, using a simulated device for
  native Digest login, first-run setup, stale edits, lost responses, failed
  readback, invalid time, occurrence-bound actions and mobile layout.
- A comparison confirming that the roadmap retains its original phase order
  and task wording; only implementation status and explanatory notes changed.

The actual local API and authentication implementation are host-tested separately
from the browser fixture. Wi-Fi join/failure/retry and serial reconnection are
state-machine/protocol tests, not evidence of radio or USB acceptance. This
candidate has **not been flashed**. Physical power cuts, USB/password recovery,
actual phone browsers, compatible migration and concurrent audio/network behavior
remain separately coordinated acceptance work. The public website, installer,
publication and release-media qualification are outside this implementation.

See the [installer and developer handoff](../../firmware/esphome/provisioning/README.md)
for commands, HTTP/USB contracts, fresh-install artifacts and the remaining checks.

## Isolated provisioning acceptance preparation — 2026-09-25

Provisioning was checkpointed locally as `a8af597`, based on `b371b65`. The
subsequent preparation adds an existing-device test image using real prayer
calculations with isolated settings/history, activation and credential namespaces.
Production defaults and record formats are unchanged. The older synthetic
scheduler configuration and its explicit `oa_validation` storage remain unchanged.

| Build | Application `.bin` bytes | Static RAM bytes | Free bytes per 2 MiB slot |
| --- | ---: | ---: | ---: |
| Production provisioning candidate | 1,127,200 | 113,991 | 969,952 |
| Isolated provisioning validation | 1,129,200 | 113,515 | 967,952 |
| Existing real-schedule developer image | 1,118,224 | 113,651 | 978,928 |
| Existing synthetic scheduler validation | 1,141,936 | 114,363 | 955,216 |

All four builds passed pinned-dependency, partition, factory/application payload,
shared-audio format and 1.5 MiB growth-target checks. Audio checks used the generated
CI fixture; no new media qualification is implied. Generated feature flags were
checked: only the isolated image enables provisioning-test storage.

Seven C++ suites under UBSan, 36 Python tests, four Chromium/WebKit browser tests,
and all eight ESPHome configuration checks passed. Storage tests cover both
production and isolated builds, exact namespace rejection, unrelated-record
preservation, cleanup faults, and power loss at every cleanup write/commit boundary.
The actual settings/API adapter runs in both modes; maintenance latches further
settings, activation and scheduler writes off until restart. Browser tests check
that production hides the banner and test mode displays it at a phone viewport.
The runbook's offline extraction script selected a confirmed application slot and
rejected an unconfirmed slot in generated fixtures.

The [hardware runbook](../../firmware/esphome/provisioning/HARDWARE_TEST.md) records
build commands, USB-only test cleanup, private baseline/recovery evidence,
application-only upgrades, production-record comparisons, and stop conditions.
The namespace cleanup command is absent from production's serial dispatcher and
has no HTTP endpoint. Cleanup first commits an incomplete marker, then clears test
prayer/network records, and removes that marker last. Uncertain cleanup outcomes
require inspection and restart, never automatic write retries.

No physical device was accessed or flashed for this preparation. Actual USB/radio
behavior, phone browsers, cold-power interruptions, simultaneous audio/network
headroom, migration and final restoration remain pending coordinated acceptance.
