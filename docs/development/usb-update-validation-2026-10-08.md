# USB application-update development validation, 2026-10-08

This report covers the additive USB updater and device-local status presentation
in the `codex/usb-updates` change based on `310b978`. These are development builds,
not release candidates or physical acceptance. The companion website retains
`usbUpdateEnabled: false` until new qualification and a capable release.

## Automated behavior

- CMake/CTest: all 16 host tests passed.
- Required Python runner: all 103 tests passed without skips, using the resolved
  ArduinoJson headers and built audio/settings adapters.
- Production updater runtime checks passed with UBSan for existing version
  reconciliation, the new USB sender/receiver contract, ordinary isolated-build
  rejection and qualification startup success/failure.
- USB cases cover bad signatures before erase, exact token/offset and duplicate
  rejection, conflicting Wi-Fi updates, offline-clock transfer with healthy
  services, unhealthy-service refusal, hash mismatch, idle abort, a power cut
  between durable handoff and selection, startup success/rollback, and selection
  errors both before and after metadata changed. USB interruption never starts
  an HTTP download; verified selection never automatically reboots.
- Chromium and WebKit device UI cases cover USB transfer/handoff messages and
  blocked network update/cancel controls. Browser endpoints are simulated with
  the production Digest verifier.
- Companion website validation covers actual published v0.4.0 signed descriptor
  and OTA import, signature/image corruption rejection, session ownership,
  ambiguous finalization, disabled public writes, whole-flow desktop/mobile
  browser behavior and production simulator exclusion. It consumes artifacts
  without compiling firmware.

## Initial USB implementation measurements

Baseline and candidate use identical configuration, compile-only synthetic audio,
qualification trust material and pinned Python 3.13.0, ESPHome 2026.9.0, ESP-IDF
5.5.5 and Xtensa compiler 14.2.0_20260121. The startup-failure variant uses
qualification version v0.0.3 and its existing health-failure switch. Each build
passed `check_feasibility.py`, including image/partition and dependency checks.
No synthetic recording was installed or played.

| Variant | Baseline OTA bytes | Candidate OTA bytes | Delta | Remaining 1.5 MiB budget | Free 2 MiB slot bytes |
| --- | ---: | ---: | ---: | ---: | ---: |
| Reference | 1,265,760 | 1,273,472 | +7,712 | 299,392 | 823,680 |
| Provisioning isolated | 1,267,552 | 1,274,848 | +7,296 | 298,016 | 822,304 |
| Upgrade qualification | 1,272,928 | 1,280,400 | +7,472 | 292,464 | 816,752 |
| Startup failure | 1,272,928 | 1,280,400 | +7,472 | 292,464 | 816,752 |

Candidate static RAM is 115,395 bytes for reference/provisioning and 115,467 bytes
for qualification variants. These figures do not establish runtime heap,
fragmentation or PSRAM headroom. Both application slots, the 3.5 MiB shared audio
partition and bootloader allowlist remain unchanged.

Initial candidate OTA SHA-256 identities from these measured development builds:

| Variant | SHA-256 |
| --- | --- |
| Reference | `964b5517d4be6ad32bc0d3e9c0ce8a2b62f308d51f71f0b7e2d83941cb7b2fa2` |
| Provisioning | `5f871b4ecff9b7c6d0b8b53159e1b0567ef30c06fc5af124c668654965e5636f` |
| Qualification | `5721e64e734ec62ce6687525dbf4c290a7f17b227d8ece49126b1ffdd24d264d` |
| Startup failure | `c53b8f67f36023a36919471fcc0541f78180809629a6d63dc50c491acc94625e` |

## USB reconciliation correction

The correction compares exact initial PR head `4143747` with the shared USB
selection classifier and fresh interrupted-discard guard. Both stages used the
same pinned toolchain, configurations, synthetic audio, qualification trust and
`development` build identity described above. All four before/after builds passed
capacity, dependency, partition and image-consistency checks.

The regression failed against the original updater: unreadable selection became
`usb_interrupted` even though the candidate remained selected. Corrected UBSan
runtime checks cover failed boot/description/state reads, absent state entries,
mismatched versions and unexpected partitions after successful or ambiguous
FINISH; repeated loops/restart; requests with an already-empty handoff marker;
read recovery; changed selection at discard time; incomplete unselected images;
true rejection, healthy/unhealthy startup and superseded USB requests whose
older candidate is still selected. Uncertain or selected requests
retain their journal and USB ownership without writes, new selection, reboot or
HTTP requests. A positive current-slot selection permits explicit incomplete
transfer discard, including absent inactive metadata. Existing Wi-Fi, ordinary
isolated and qualification profiles also pass.

CMake/CTest passed all 16 tests; the complete required Python suite passed all 103
without skips. The resolved SDK request/header regressions passed. Focused
Chromium/WebKit checks passed both USB-status cases, including uncertainty
keeping network actions disabled and cancellation hidden. The complete device UI
rerun passed all 466 tests without skips.

| Variant | Before OTA bytes | Corrected OTA bytes | Delta | Remaining 1.5 MiB budget | Free 2 MiB slot bytes |
| --- | ---: | ---: | ---: | ---: | ---: |
| Reference | 1,273,472 | 1,274,144 | +672 | 298,720 | 823,008 |
| Provisioning isolated | 1,274,848 | 1,275,568 | +720 | 297,296 | 821,584 |
| Upgrade qualification | 1,280,400 | 1,281,184 | +784 | 291,680 | 815,968 |
| Startup failure | 1,280,400 | 1,281,184 | +784 | 291,680 | 815,968 |

Static RAM remains 115,395 bytes for reference/provisioning and 115,467 bytes for
qualification variants. The correction introduces no new dependency or persistent
allocation. Runtime heap/fragmentation and PSRAM remain physically unqualified.
Both application slots and shared audio retain their existing layout.

Corrected development-build OTA identities:

| Variant | SHA-256 |
| --- | --- |
| Reference | `5e371b03e527b76b6b74a162c7b71e6d53db0fc27c0f1497fbb5934578b8895b` |
| Provisioning isolated | `663f9a09d770c0bc90f6118e4498e856dde60749f98af6264c0350078b4baa2c` |
| Upgrade qualification | `c2c19a64f5c300c730b9500cbddcced06aaf98938f4781e3953e9edfcc3dc627` |
| Startup failure | `2bfc1d9601d9699a0f392c611f0feeb4058dd097ed28f8106625a86e1ba89c89` |

## Physical and runtime limits

No hardware was accessed or changed. Atom-only USB health/transfer, bottom-power
startup, actual credential/settings/history/audio preservation, interruption
recovery, failed-startup rollback and concurrent audio/network heap/fragmentation
remain untested for this transport. Existing Wi-Fi qualification cannot substitute
for these results. Follow the [USB qualification contract](usb-firmware-updates.md)
and current hardware runbooks before activation. Firmware publication and public
USB enablement require separate review and approval.
