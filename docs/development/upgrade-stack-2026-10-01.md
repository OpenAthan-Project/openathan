# Firmware updater stack checkpoint — 2026-10-01

An isolated reference-device descriptor check reproduced an unexpected reset.
Serial capture from qualification commit
`26ca275b0c705520a678557028b361ac9cc426d6` reported:

```text
***ERROR*** A stack overflow in task oa_upgrade has been detected.
```

The exact retained ELF maps the abort to the FreeRTOS stack-overflow hook. The
trace after that hook is corrupted; it does not identify the deepest TLS or
signature-verification call. Installation was not requested. Fresh paired scoped
reads confirmed unchanged bootloader, control/NVS, inactive application and
complete shared audio. Protected production payloads still matched the original
baseline. The diagnostic application remained VALID; the device was stopped.

## Product fix

The production updater creates an 8,192-byte worker task. Its compiled `run_`
frame reserves 4,736 bytes even for a descriptor-only check, because descriptor
and image-download buffers share that function. TLS and signature verification
then use the remaining stack.

The buffers now use bounded, checked internal-heap allocations: 1,024 bytes for
checks and 4,096 bytes for downloads. Their lifetime is limited to the worker
operation and they are freed on success, cancellation and error. Internal memory
remains accessible when flash/PSRAM caches are disabled during OTA writes.
Allocation failure occurs before networking or OTA erase, retains an installation
request and permits a later retry. The task remains 8,192 bytes; the compiled
worker frame is now 656 bytes, a reduction of 4,080 bytes.

This fix changes reusable updater resource handling. Qualification controls,
private trust and telemetry remain in the separate
[qualification work](https://github.com/OpenAthan-Project/openathan/pull/11).

## Automated validation and capacity

The low-memory regression failed against the previous implementation and passes
with the fix. All three production-version updater host scenarios pass UBSan,
including preservation and delayed retry after allocation failure. Existing
success, error, cancellation and safety scenarios verify buffer release. All
65 Python tests passed without skips.

ESPHome 2026.9.0 / ESP-IDF 5.5.5, matching configuration and embedded identity:

| Variant | Before OTA bytes | After OTA bytes | Delta | Application budget remaining |
| --- | ---: | ---: | ---: | ---: |
| Production reference | 1,211,696 | 1,211,744 | +48 | 361,120 |
| Ordinary isolated provisioning | 1,213,600 | 1,213,600 | 0 | 359,264 |

Both affected capacity checks passed against the approved shared audio. Static
RAM remains 113,351 bytes. Application slots remain 2 MiB and shared audio remains
3.5 MiB. The additional transient internal-heap allocation is bounded above;
static RAM and the smaller frame do not establish runtime headroom.

## Physical limitations

The captured overflow establishes the original failure. Fixed isolated source
`98470e448059d391c5c4ef9d86a8de8634be9a4e` passed six descriptor/TLS checks,
ten interrupted attempts, physical pre/post-selection cuts, v2 confirmation and
both pending/startup rollback cases. A later handoff watchdog stopped that run.

Corrected integration
`6eb5572db7035ea59e767cc3a19e847eb5d9a12d`, including the separate nonblocking
shutdown fix, passed corrected v2/v4 software handoffs, automatic failed-v3
rollback, both retained recordings and the reconciled bounded 60-minute soak.
Minimum observed unused worker stack was 3,964 bytes during the final soak. Fresh
paired readback preserved protected production records, complete shared audio and
reviewed bootloader. See the
[dated qualification evidence and runtime measurements](upgrade-qualification-2026-10-01.md#corrected-shutdown-handoffs-and-retained-recording-soak),
including the extra-play observer correction and the paused-download limitation.
Production restoration, public GitHub delivery, broader/overnight coverage and
release readiness remain separate gates.
