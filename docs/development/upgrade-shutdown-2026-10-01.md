# Firmware handoff shutdown validation — 2026-10-01

A healthy candidate can be selected while an HTTP handler is waiting for a
queued main-loop request. `Device::loop` processes upgrades before that queue;
`App.safe_reboot` invokes shutdown hooks on the same loop. The original hook
called `httpd_stop`, which waits for the handler. That handler can wait 5,000 ms
for the now-stopped loop, matching the pinned five-second task-watchdog timeout.

Keep the shutdown hook nonblocking after cancelling updater work. System
restart or power-down terminates the HTTP tasks and sockets. Normal startup
error cleanup still stops an unsuccessfully initialized server. The method is
compiled in its own source file so the actual lifecycle can be exercised against
a pending-request adapter without replacing the production implementation.

## Automated validation

The compiled pending-request regression fails with the original method and
passes with this fix, including updater cancellation and safe-window revocation.
All 66 product Python tests, 11 UBSan CTest suites and three production updater
profiles passed. Both affected firmware profiles pass the existing capacity and
dependency checks. No new dependency, partition, required hardware or media is
introduced.

Matched ESPHome 2026.9.0 / ESP-IDF 5.5.5 builds compare the stack-fix dependency
`81193478e167dcbdd9effa2be293aa305431bbe7` with product code
`014f0b95ec9fbfe84737252f09a8045b02610a7a`, using the same configuration and full
commit-identity length:

| Profile | Before OTA bytes | After OTA bytes | Delta | Application budget remaining |
| --- | ---: | ---: | ---: | ---: |
| Reference production | 1,211,744 | 1,211,696 | -48 | 361,168 |
| Ordinary isolated provisioning | 1,213,600 | 1,213,536 | -64 | 359,328 |

Static RAM remains 113,351 bytes. Both 2 MiB application slots and the separate
3.5 MiB shared-audio partition are unchanged. These build figures do not establish
runtime audio/network headroom. Comparison images use compile-only fixtures and
are not hardware candidates.

## Observed hardware and remaining checks

Qualification source `98470e448059d391c5c4ef9d86a8de8634be9a4e` passed ten interrupted
install attempts, pre/post-selection cuts, healthy v2 confirmation, pending-boot
rejection and automatic failed-v3 startup rollback on AtomS3R C126 + Voice Pyramid
A167. Its subsequent v4 handoff reported `ESP_RST_TASK_WDT` (reason 6), then booted
PENDING and confirmed VALID. That handoff is not a passed clean restart.

The physical timeout backtrace was not captured; the shutdown wait is a
source-proven hazard consistent with the reset, rather than a confirmed physical
callsite. Fresh paired scoped reads preserved protected production records,
complete shared audio, the bootloader and the retained VALID v2 application.

The corrected isolated integration
`6eb5572db7035ea59e767cc3a19e847eb5d9a12d` subsequently passed v2/v4
PENDING-to-VALID software handoffs under HTTP polling and automatic failed-v3
rollback, with reset reason 3 and preserved settings/history. Both retained
recordings received attended listening acceptance during a 3,601.579-second run
with 720 status samples and 48 completed firmware checks. The original observer's
extra-third-play assertion was reconciled separately; its original stopped result
remains preserved. Fresh paired USB reads confirmed protected production records,
full shared audio, the reviewed bootloader and exact VALID v4 / retained v2.

See the [source-bound qualification results and runtime measurements](upgrade-qualification-2026-10-01.md#corrected-shutdown-handoffs-and-retained-recording-soak).
The earlier ten interrupted attempts and physical cuts remain evidence for their
original source. Production restoration, long-duration/network coverage and
public-release acceptance remain deferred. No blanket release readiness is
implied.
