# Automatic update recovery validation, 2026-11-13

These fixes keep a canceled or rejected release from becoming an automatic retry
after a storage fault or restart. They also retain a pending boot handoff until
read-only checks establish its outcome. The
[updater contract](firmware-upgrades.md) describes the resulting behavior.

Validation ran on November 12 and 13, 2026.

## Source and compatibility

Baseline: `847e98e89ca0da73f6d191b1e3ece6e15ad69109`.
Runtime fix: `b42692dbc391397e4d17ade4bfca5966e3a104c1`, bound to the source
hashes below. Regression commits `dc7906ca6e63a405a132b035021b50fe77615766` and
`483b942c853630ba372e4bbffbcbd7cf3ca0977d` precede the production fix.
The [earlier description-read report](automatic-update-description-retry-2026-11-12.md)
retains its source-specific results.

The existing request, automatic-policy, settings, audio, signed release and USB
protocol formats remain unchanged. A separate optional schema-1 `verify_hold`
record stores the version and SHA-256 before image checks can reject a release.
Suppression commits before the updater clears that record and its request.
Older applications ignore this record and do not provide its recovery guarantee.

## Reproduced failures and corrected behavior

| Trigger | Before the fix | Corrected behavior |
| --- | --- | --- |
| Cancellation with unreadable automatic policy | HTTP 200 cleared the request without saving the skip. | HTTP 503 retains the request until cancellation can save the skip. USB abort and rollback retain ownership on the same fault. |
| Transient metadata read after rollback | The handoff marker was cleared, allowing another download of the rejected release. | Failed reads preserve the marker. Positive rollback evidence saves the skip before cleanup. |
| Boot selection succeeds before its API reports an error | The handoff marker was cleared despite the selected candidate. | Read-only reconciliation retains selected or unresolved ownership. No repeat erase or selection occurs. |
| Rejection followed by a failed suppression write or interrupted cleanup | A normal queued request survived and retried the rejected release after restart. | The verification record survives. Healthy recovery skips that release, while an explicit manual installation can retry it. |
| Check followed by Cancel on a legacy empty-queue handoff | Check returned 200 and changed the display state, allowing Cancel to remove selected ownership. | Check and Cancel return 409 and retain the handoff record. |

Ordinary partial downloads and retryable description reads retain the five-minute
automatic retry policy. Interrupted verification requires an explicit manual
retry. An orphan verification record exposes the existing **Cancel queued update**
control so the owner can recover without a new endpoint.

## Automated checks

- All 60 recovery fault scenarios pass under UBSan. The suite covers suppression,
  verification and request commit cuts, corrupt or unreadable records, first-write
  validation, checksum rejection, both OTA sectors, CRC validation, write-before-error
  boot selection, manual ownership, USB ownership and legacy journals.
- The complete updater, USB and HTTP matrix passes all 15 host executables. Three
  incomplete qualification profiles fail compilation as required.
- All 26 core UBSan CTests and 180 pinned Python tests pass.
- All 24 focused Chromium and WebKit cases pass, with no skips or cancellations.
  These include automatic controls, failed verification cancellation, queueing,
  reconnect recovery and USB handoff. The original full browser results remain in
  the [initial report](automatic-updates-validation-2026-11-12.md).
- An independent review reran all 60 recovery scenarios, two legacy-journal probes,
  reversed slot and sector ordering, retryable first-write errors and late
  cancellation. It found no remaining correctness or actionable comment findings.
- All 21 checks pass in the [runtime-fix CI run](https://github.com/OpenAthan-Project/openathan/actions/runs/43366448352).
- Whitespace, JavaScript syntax and changed-document relative links pass.

The regression commits fail on the preceding implementation for the expected
behavioral assertions. A direct legacy handoff probe changes from
`check=200, cancel=200, saved_expected=''` to
`check=409, cancel=409, saved_expected='v0.3.0'` while the candidate remains selected.
The browser regression also reproduced the missing Cancel control before its fix.

The [CI instructions](../../.github/workflows/README.md) give pinned prerequisites
and commands. The updater matrix runs with
`python tools/check_upgrade_runtime.py --arduinojson PATH`, where `PATH` is the
resolved pinned ArduinoJson 7.4.3 source directory. Focused browser commands are in
the [device UI guide](../../web/device-ui/README.md#run-focused-browser-cases).

## Matched firmware measurements

Both stages use Python 3.13.0, ESPHome 2026.9.0, ESP-IDF 5.5.5, Xtensa GCC
14.2.0 `esp-14.2.0_20260121` and tzdata 2026.4. Each pair has identical SDK
configuration bytes and public compile-only fixtures. Both stages retain the
declared v0.10.0 version and development build identity.

The retained baseline applications match their capacity receipts, generated
updater sources and embedded UI assets. Baseline receipts record toolchain
versions, but do not contain a complete historical compiler and SDK hash manifest.
The comparison therefore establishes matching pinned versions and configuration,
with verified baseline application bytes and current generated source.

All 15 affected variants pass the dependency, partition, factory and application
payload equality and capacity checks. Generated component sources match the
frozen candidate, and each component C++ source appears in its compilation database.
All six compressed UI assets match source and actual OTA bytes. Public
qualification fixtures match where required. Application and bootloader command
audits preserve the baseline compiler and linker settings, with the path alias
mapping described below.

| Variant | Before OTA bytes | After OTA bytes | Delta | Remaining 1,572,864-byte budget | Static RAM delta |
| --- | ---: | ---: | ---: | ---: | ---: |
| atom | 1,426,640 | 1,433,600 | +6,960 | 139,264 | +24 |
| audio-board | 1,394,128 | 1,401,072 | +6,944 | 171,792 | +24 |
| audio-board-development | 1,357,040 | 1,357,424 | +384 | 215,440 | +24 |
| audio-board-startup-failure | 1,395,008 | 1,402,048 | +7,040 | 170,816 | +24 |
| audio-board-upgrade | 1,395,008 | 1,402,048 | +7,040 | 170,816 | +24 |
| box | 1,430,496 | 1,437,424 | +6,928 | 135,440 | +24 |
| dua-qualification | 1,395,856 | 1,396,224 | +368 | 176,640 | +24 |
| provisioning-validation | 1,417,616 | 1,424,528 | +6,912 | 148,336 | +24 |
| quran-qualification | 1,483,120 | 1,483,504 | +384 | 89,360 | +24 |
| upgrade-qualification | 1,427,520 | 1,434,544 | +7,024 | 138,320 | +24 |
| upgrade-startup-failure | 1,427,520 | 1,434,544 | +7,024 | 138,320 | +24 |
| waveshare-audio | 1,329,776 | 1,330,144 | +368 | 242,720 | +24 |
| waveshare-diagnostics | 1,480,672 | 1,481,040 | +368 | 91,824 | +24 |
| waveshare-display | 1,393,344 | 1,393,712 | +368 | 179,152 | +24 |
| waveshare-dua-qualification | 1,399,680 | 1,400,032 | +352 | 172,832 | +24 |

All applications stay below 1,572,864 bytes. The smallest budget margin is
89,360 bytes, leaving 613,648 bytes free in that image's 2 MiB slot.
Reference production growth is 6,928 to 6,960 bytes. Static RAM increases by
24 bytes in every profile. Most recovery code is discarded in configurations
that disable updates.

The isolated macOS SDK copies initially exposed temporary source paths because
`/tmp` and `/private/tmp` aliases did not match the SDK prefix map. Those images
are excluded from the table. The corrected builds map both aliases to the same
`/IDF` path as the baseline. The local runner also propagates CMake policy
`CMP0126=NEW` to application and bootloader configuration to retain the generated
toolchain path. These changes affect the local build runner only. Firmware
source, dependency versions and SDK configuration remain unchanged.

Both 2 MiB application slots and the separate 3.5 MiB shared audio partition are
retained. Synthetic audio fixtures must never be installed or played.

## Runtime source hashes

| File | SHA-256 |
| --- | --- |
| `firmware/esphome/components/openathan_device/upgrade.h` | `f75e2be741e396e41a0c9b7b7de055acfd5e97826ee23ca935ac325b11a7d8ad` |
| `firmware/esphome/components/openathan_device/upgrade.cpp` | `cd96f30584c46895160ec85ccc0d7f5549d0d5a12fb4b1faad0ee781675c8890` |
| `firmware/esphome/components/openathan_device/automatic_updates.cpp` | `10e3865a11960c0c566c4d3593d0b34ef97ef26d5c7433d35b354827f0cc084f` |
| `firmware/esphome/components/openathan_device/usb_upgrade.cpp` | `de78eb12382f37e906d9a3dd3c80c97879e3d11253f20b923f6c4bf6fcbcdd45` |
| `firmware/esphome/components/openathan_device/upgrade_recovery.cpp` | `efecf613cc70ddd31ee9719155eaf5c48234cc0c52dfd765096ff7dd9c12701e` |
| `web/device-ui/app.js` | `7db118e99683440285549fbcfaf06ac16f5478ea1c18ac8c189a44fab227c6d2` |

## Hardware and runtime limits

No speaker was accessed, updated, reset or played for this work. The new record
retains one release identity and limits stored-record reads to 256 bytes. Its
JSON parsing and NVS commits add allocation and flash activity at verification
boundaries. Runtime heap, fragmentation, worker stack, PSRAM and concurrent
playback behavior remain unmeasured for this change.

Host fault injection establishes the simulated storage and selection boundaries.
It does not establish a physical power cut at the same instruction or physical
update, rollback, preservation or audible-playback acceptance. The
[attended upgrade runbook](../../firmware/esphome/upgrades/VALIDATION.md) retains
those checks. These compile-only images do not establish release readiness.
