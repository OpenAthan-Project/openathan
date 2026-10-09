# USB application-update hardware observations — 2026-10-08–09

Actual Chrome and Edge transfers, bottom-power startup, failed-startup rollback,
and final production-data preservation passed on one reference ESP32-S3 speaker.
The speaker was restored to normal production operation. **Qualification remains
incomplete:** a Chrome status timeout and a slow authenticated exchange are
unexplained, and the interruption and concurrent-activity limits below remain.
Public USB updating stays disabled; this report does not authorize a release or
website activation.

Firmware source was merged commit
`9b32b9068a073ae2fd0c8325f06d52cd556df4de`; companion website source was
`25cdee3cccacb040741a1b69ea62699b0a4eb70d`. Earlier host, simulated-browser and
matched development-build results remain in the
[development validation report](usb-update-validation-2026-10-08.md).

## Test boundary and application identity

All qualification applications used isolated storage and ephemeral signing/TLS
trust. The localhost harness used the website's real service, serial transport,
protocol and UI with qualification trust and release selection. Actual browser
device choosers and physical USB were used; device I/O was not simulated.
Fresh installation was disabled, and factory images, audio and private keys were
not served. These tests do not establish delivery or trust acceptance of a future
published release.

Fresh identity, security, 8 MiB capacity and the exact dual-slot/shared-audio
layout were checked before transitions. Paired scoped reads retained compatible
application recovery. The already-compatible, allowlisted bootloader was kept;
no erase-all, partition-table, NVS or audio write was used for the transitions.
Production namespaces remained separate from qualification settings, credentials
and update journals. No synthetic CI recording was installed or played.

The pinned build environment was Python 3.13.0, ESPHome 2026.9.0, ESP-IDF 5.5.5,
compiler `esp-14.2.0_20260121` and tzdata 2026.4. All four qualification capacity
checks passed. Each actual OTA image is 1,281,248 bytes, leaving 291,616 bytes
under the 1,572,864-byte budget and 815,904 bytes in its 2 MiB slot. Static RAM
is 115,467 bytes; runtime measurements are separate below.

| Private candidate | Behavior | OTA SHA-256 |
| --- | --- | --- |
| v0.0.1 | Healthy baseline | `0b252ccfd1a48dc65ac4bc323ae051bdb6d08b15ed37d265e390ae8be539d5bb` |
| v0.0.2 | Healthy Chrome update | `af3c303d4bba0e92970f00ce492522e3ee2ebff7411e1b237d81a1e8a5699fcf` |
| v0.0.3 | Injected startup-health failure | `6c770e9f2d70ad018a81a63b4aaceb1ef7d70b623c9d7ed8811663a6dbfbbcc2` |
| v0.0.4 | Healthy Edge update | `a42593821e049c1a647529279ea2afba24f7a84636d84e0fee9194855cc0e7f3` |

## Observed physical behavior

| Case | Observation and scope |
| --- | --- |
| Legacy installed application | The normal installer showed existing-device Wi-Fi/maintainer guidance and recovery. No fresh installation was invoked. |
| Framing and session ownership | Twenty-one individual frame/session cases passed: invalid BEGIN sizes, busy ownership, wrong/zero tokens, invalid chunks, duplicate/out-of-order descriptor chunks, incomplete VERIFY, blocked credential/scan mutations, available INFO, explicit ABORT and stale data after abort. The runner's later additional BEGIN stopped before verification; its original stop and read-only reconciliation are retained. |
| Signature and descriptor idle timeout | A bad ECDSA signature was rejected before application transfer/selection. An idle descriptor session expired and released ownership. An idle-only observer assertion stopped when the automatic network checker became active; read-only reconciliation confirmed empty queue and unchanged VALID selection. |
| Truncated application | Sending 65,536 bytes of the signed 1,281,248-byte image caused FINISH rejection. The old v0.0.2 application remained VALID and selected. One explicit discard cleared the incomplete request. |
| Corrupt application | All 1,281,248 bytes were sent with one body byte changed and the original signed hash retained. FINISH rejected it and cleared the rejected request; the old VALID application remained selected. |
| Descriptor reconnect | Closing/reopening USB after 242 of 564 descriptor bytes caused an observed USB/JTAG hardware restart. The old application returned VALID with no queued image. This was not a manually timed physical power cut. |
| Near-final physical interruption | An owner power cut disconnected USB after acknowledgement of 1,277,408 of 1,281,248 bytes, before FINISH. Restart reported `usb_interrupted`; the old VALID application remained selected and one explicit discard cleared the retained journal. The cut was within the final 3,840 bytes, not at an instrumented selection instruction. |
| Actual Chrome healthy transfer | One complete signed v0.0.2 transfer reported written/verified and released USB. Independent status showed the old application still running, the new slot selected and `awaiting_power`, with no automatic reboot. Owner Atom-unplug/bottom-only power then produced healthy VALID v0.0.2. |
| Browser ownership exclusion | Actual Edge connection was refused while Chrome owned USB; the reciprocal Chrome attempt was refused while Edge owned it. The owning browser kept its connection. |
| Actual failed-startup rollback | Chrome wrote signed v0.0.3 once. After owner bottom-only power, actual PENDING v0.0.3 and injected health failure were observed, followed by software rollback to VALID v0.0.2, `rolled_back` and an empty journal. The existing health/rollback timing was unchanged. |
| Actual Edge healthy transfer | One complete signed v0.0.4 transfer reported written/verified and released USB. Independent status confirmed old v0.0.2 still running and the candidate selected/`awaiting_power`. Owner bottom-only power then produced healthy VALID v0.0.4 and cleared the journal. |
| Competing local operations | During Chrome transfer, authenticated update-check returned 409 and exact empty-object Stop returned 200. The device was idle; this establishes endpoint availability, not audible playback being stopped. |
| Preservation | Fresh paired checkpoints after rejection, physical interruption and final Edge startup matched original production record payloads and the full shared-audio image. Bootloader and partition table were unchanged. Isolated saved settings/preferences and monotonic history were also checked through successful startup and rollback. |

No uncertain mutation was automatically resent. Original stopped attempts remain
separate from their reconciliation: a cut window that expired before the owner
removed power is not counted as an interrupted-write pass; bounded startup
observers that expired before owner handoff are not counted as startup failures
or passes. Fresh owner-confirmed observations establish the later startup results.

## Runtime observations and unresolved responsiveness

Times below measure the complete Digest challenge plus authenticated status
exchange, not a single GET response. Receiving samples cover portions of the
transfer and do not constitute continuous device instrumentation.

| Actual receiving transfer | Successful receiving samples / acknowledged-byte range | Maximum complete exchange | Minimum free internal bytes | Minimum largest internal block | Cumulative internal low-water mark | Minimum free PSRAM bytes |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Chrome v0.0.3 | 84 / 2,662–1,275,582 | 17.483 s | 241,564 | 196,608 | 199,964 | 8,355,460 |
| Edge v0.0.4 | 94 / 1,210–1,264,692 | 2.088 s | 241,744 | 196,608 | 200,424 | 8,355,488 |

Those two observers completed 93 and 120 successful exchanges overall, with no
exceptions or unexpected reset during their runs. Earlier Chrome v0.0.2 sampling
recorded 22 successful mid-transfer reads followed by a TimeoutError while USB
continued; a fresh post-transfer read recovered. The timeout and later 17.483 s
Chrome exchange remain unexplained. Edge's measured request/header/body phases
and smaller maximum do not establish their cause or clear that limitation.

No new audible concurrent-USB playback, due-prayer interruption or long-soak
acceptance is claimed. Physical cuts at the exact durable-marker/boot-selection
boundary and failure injection into physical selection reads/writes were not
performed; those cases retain automated coverage only. Prior Wi-Fi/audio
qualification remains separate evidence. Static capacity and the sampled runtime
figures do not establish maximum concurrent-load heap/fragmentation headroom.

## Final production preservation and restoration

After healthy Edge startup, fresh paired reads verified selected app0 VALID and
matched all original protected production record payloads, the complete
3,670,016-byte audio partition, bootloader and partition table. The audio SHA-256
remained `ca74b33e3e8a504b42c414ba8361438101d41d7ac458d6f4367dccaf1981f228`.

A new reviewed application-only plan bound that stopped-device snapshot to the
merged-main production application. One write at the freshly selected `0x10000`
offset passed official verification and independent readback. Complete control
storage, credentials, settings, prayer history, OTA metadata, inactive application,
bootloader and audio remained byte-identical. No historical NVS/full-flash restore
or bootloader transition occurred.

The restored production application is 1,274,192 bytes, SHA-256
`c9a7570bd5f077786f6746f4cc0f826fef9ba7f2d3076cc728a96c6c50df7cbc`.
Against the same pinned published-v0.4.0 reference of 1,265,792 bytes, growth is
8,400 bytes; 298,672 budget bytes and 822,960 slot bytes remain. Static RAM is
115,395 bytes. This unpublished merged-main build still identifies as v0.4.0;
it does not replace or requalify the already-published v0.4.0 assets.

Owner Atom-unplug/bottom-only power was followed by read-only normal-hostname,
`test_mode: false`, exact source/version and production-authentication checks.
All saved production settings and display/light/time-format preferences matched
the fresh snapshot, revision 13 was retained, history never moved backward,
and active setup, applied settings, synchronized clock, fault-free automatic
readiness and a future scheduled prayer stayed healthy for 32.564 seconds.
No catch-up playback was observed in this bounded startup window. This is not
new listener acceptance of a scheduled recording.

The speaker was left running its retained production schedule on bottom power.
Private test servers were stopped. Signed test artifacts, original stops, raw
observations, screenshots, paired snapshots, recovery applications and restoration
journals remain in the private diagnostic archive outside Git; no credential,
private key or recovery image is included in this documentation.

The [USB qualification contract](usb-firmware-updates.md#required-qualification)
and [hardware runbook](../../firmware/esphome/provisioning/HARDWARE_TEST.md)
remain authoritative. Resolve or explicitly review the remaining qualification
limits before capable release preparation and public activation. Firmware
publication and website enablement remain separate reviewed actions.
