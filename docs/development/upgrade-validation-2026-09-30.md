# Firmware upgrade validation — 2026-09-30

Implementation candidate based on main `1e5596a87ea6545ed9fb8ec2ac04f63fe63f7109`.
These are development measurements, not release qualification. Public v0.1.0
and the physical speaker were not changed.

## Application capacity

Baseline and candidate use the same configuration and pinned stack: Python 3.13,
ESPHome 2026.9.0, ESP-IDF 5.5.5, compiler esp-14.2.0_20260121 and tzdata 2026.4.
Builds use synthetic, non-playable audio fixtures outside the repository.

| Variant | Baseline OTA bytes | Candidate OTA bytes | Delta | Application budget remaining | Slot free |
| --- | ---: | ---: | ---: | ---: | ---: |
| Official reference | 1,136,752 | 1,210,528 | +73,776 | 362,336 | 886,624 |
| Isolated provisioning | 1,138,800 | 1,211,984 | +73,184 | 360,880 | 885,168 |

Both affected variants passed dependency/partition/capacity checks, exact
factory/OTA application consistency and absence of fixture audio in applications.
The added cost includes HTTPS certificate validation, ECDSA verification, OTA
transport/state management, bootloader compatibility checks and compressed UI.
The two 2 MiB slots and separate 3.5 MiB audio region are unchanged.
Static RAM figures do not establish runtime heap, fragmentation or PSRAM safety.

The candidate factory bootloader region (32 KiB, including padding) hashes to
`91da841bab4020ca05049e3aef6307fd6fc2bb329932ffe366c3f49dcf940f0e` in both builds.
Its generated configuration enables application rollback. The baseline region
hash is `1e9fc7c1ec17cecb3d8fb2520669802851b93f04a92062f84b0f5952d8bffe65` and its
configuration disables rollback. Firmware accepts installation only with a
reviewed rollback-enabled bootloader hash.

## Automated evidence

- Eleven CTest suites pass under UndefinedBehaviorSanitizer.
- Sixty-five Python tests pass without skips, including real P-256 signature
  verification, tampering/wrong-key/identity failures and seven-asset bundle
  validation preserving the schema-1 installer contract.
- Production updater host tests pass with deterministic transport, task, flash
  and NVS adapters: queue persistence, duplicate requests, safe prayer windows,
  cancellation during download, prayer starting during transfer, hash/image
  failures, storage failures, invalid signatures, failed startup rollback,
  previous-version recovery, corrupt records and unsupported bootloaders.
  Signature success is injected in these runtime tests; the Python tests exercise
  real cryptography. They do not simulate physical flash timing or power loss.
- Thirty Chromium/WebKit browser cases pass, covering authentication, edits,
  uncertain responses, cancellation, reconnect outcomes and phone layout.
- The pinned public-site validator accepts producer output and rejects mutated
  audio using Node 24.19.0. The fresh-install schema remains unchanged.
- Desktop and phone captures were inspected. The new controls fit narrow screens;
  the mechanical detector also reported pre-existing header typography warnings.

## Independent-review fixes

The review baseline was `cdf5e4cead4cc222898c15a0867a000d8fa6e30b`.
Using the same pinned toolchain/configuration, these fixes add 144 application
bytes to the reference image (1,210,384 → 1,210,528) and 192 bytes to isolated
provisioning (1,211,792 → 1,211,984).

Startup confirmation now rejects every scheduler fault. The real scheduler test
reproduces failed consumption persistence while settings, activation and audio
remain healthy; a healthy scheduler waiting for clock synchronization still passes
the startup policy. Updater adapters verify the pending image remains unconfirmed
and requests rollback on scheduler faults, with no internet/clock dependency.

Transport callbacks inject playback/cancellation during open and read. The
production worker tests assert zero erase/write after an open callback and zero
writes after a read callback. Task creation failures keep the durable queue intact,
remain cancellable, and cannot retry before five minutes; browser tests exercise
the visible Cancel control. All affected local checks pass without hardware I/O.

## Physical qualification still required

No device connection, recording playback, flash write, production release or
website deployment occurred. Do not infer physical rollback or runtime memory
headroom from these builds and host tests.

Before an existing-speaker transition, inspect live bootloader/partition/OTA
state and preserve current settings, credentials, consumption/skip records,
light preferences and audio. An application-only update cannot enable rollback
in the old bootloader; any bootloader transition needs a separately reviewed
preserving procedure. Historical full-flash backups must not replace current
history. No new full-flash backup is required by this implementation.

Before publication, validate actual Wi-Fi slot switching, network/power
interruption, failed startup recovery, queued-request cancellation/restoration,
and concurrent networking/audio heap/fragmentation. Keep dated private evidence
outside Git. See the [upgrade guide](firmware-upgrades.md).
