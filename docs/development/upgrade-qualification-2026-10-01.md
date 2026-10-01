# Firmware upgrade qualification checkpoint — 2026-10-01

This checkpoint covers the maintainer qualification implementation on top of
merged upgrade-flow commit `ef913beac1c1e453704e667f301925435af87efe`.
The earlier attended run stopped after a v4 task-watchdog reset. Corrected
source-bound handoffs, both retained recordings, the reconciled 60-minute soak
and final preservation have now passed; see the
[corrected results](#corrected-shutdown-handoffs-and-retained-recording-soak).
The earlier interruption evidence remains tied to its tested source. Follow the
[qualification runbook](../../firmware/esphome/upgrades/VALIDATION.md) for the
private HTTPS session, scoped bootloader transition and production restoration.

## Earlier automated validation

- All 11 CTest suites passed with UndefinedBehaviorSanitizer.
- All 40 embedded-UI browser cases passed in Chromium and WebKit using the
  production C++ Digest verifier.
- All 82 Python tests passed without skips, including the explicit-port
  and durable-record guard regressions. Attended preflight found that SPI flash
  must be attached after loading the esptool stub and before flash identification.
  The helper now follows that official CLI sequence; an added regression retains
  rejection of unexpected flash sizes. This was found before any flash write.
- Repeated physical feed checks exposed an unfinished TLS connection blocking
  new clients. TLS handshakes now run in connection workers with bounded socket
  timeouts; a regression verifies another client can complete while the first
  never starts TLS. The stalled-feed attempts did not reach OTA erase/write and
  are not counted as passed signature or interruption cases.
- All nine configurations validated in a fresh disposable checkout with public
  CI fixtures. The real pinned website installer validator passed with Node
  24.19.0; additional qualification assets retain installer compatibility.
  Exact-head PR CI must also pass before hardware access.
- Updater host tests passed for three production versions, ordinary isolated
  install denial, healthy qualification and forced startup failure. They cover
  holds before/after selection, cancellation, repeat-loop behavior, safety during
  playback, erased/corrupt baseline metadata and startup rollback timing.
- Real local TLS tests verified private-CA trust, hostname checking, unsigned or
  corrupt/truncated responses, private-key route denial and public-release
  exclusion. USB tests use simulated device I/O; they do not establish physical
  flash safety or bootloader recovery.

## Earlier pinned application capacity

ESPHome 2026.9.0, ESP-IDF 5.5.5 and the same reference/isolated configurations
were used for before/after measurements. Applications are measured from actual
`firmware.ota.bin`, with a 1,572,864-byte budget in each 2,097,152-byte slot.
The shared audio partition remains 3,670,016 bytes and recordings are outside
the application. These compile-only checks used synthetic audio fixtures; those
fixtures were not installed or played.

| Variant | Before bytes | After bytes | Delta | Application budget remaining |
| --- | ---: | ---: | ---: | ---: |
| Production reference | 1,211,664 | 1,211,648 | -16 | 361,216 |
| Ordinary isolated provisioning | 1,213,568 | 1,213,568 | 0 | 359,296 |
| Healthy upgrade qualification v0.0.1 | — | 1,217,056 | +3,488 vs isolated | 355,808 |
| Failed-startup qualification v0.0.3 | — | 1,216,384 | +2,816 vs isolated | 356,480 |

All four dependency, layout, application/factory consistency and capacity checks
passed. The four complete 32 KiB bootloader regions match the reviewed
rollback-enabled allowlist hash
`91da841bab4020ca05049e3aef6307fd6fc2bb329932ffe366c3f49dcf940f0e`.
Static RAM is 113,351 bytes for production/ordinary isolated and 113,431 bytes
for both qualification variants. Runtime minimum heap, fragmentation and PSRAM
under concurrent audio/network activity still require physical observation.
Trust certificates and build identity affect bytes; source-bound hardware
candidates must be checked again before installation.

The USB diagnostic exception was compared using the same pinned toolchain,
trust material and embedded identity: healthy qualification increases from
1,217,088 to 1,217,104 bytes (+16; 355,760 bytes remaining), and failed-startup
qualification from 1,216,416 to 1,216,432 bytes (+16; 356,432 remaining).
Production and ordinary isolated application sizes are unchanged from the table.
All four capacity checks passed; these measurements do not establish a panic fix.

## Production and qualification separation

Instrumentation, private-feed TLS, diagnostic exceptions, holds, counters,
baseline initialization and injected startup failure now live in the separate
qualification C++ component. Production compiles only the original updater
engine. The two reconciliation/persistence adjustments needed by repeated holds
are test-adapter behavior; they no longer change the production path. Host tests
verify production snapshots expose no qualification controls and compilation
rejects the component without both isolated qualification flags. Repeated hold
loops are also checked for no additional NVS commits.

Build checks require generated definitions and inspect source inclusion plus
actual application payloads. Release packaging rejects multiple independent
qualification markers and isolated provisioning-storage markers. Qualification
capacity exceptions remain explicit and are never used by publishing defaults.

Same pinned settings and identity were used before/after this separation:

| Variant | Before separation bytes | After bytes | Delta | Application budget remaining |
| --- | ---: | ---: | ---: | ---: |
| Production reference | 1,211,648 | 1,211,664 | +16 | 361,200 |
| Ordinary isolated provisioning | 1,213,568 | 1,213,568 | 0 | 359,296 |
| Healthy qualification v0.0.1 | 1,217,104 | 1,218,944 | +1,840 | 353,920 |
| Failed-startup qualification v0.0.3 | 1,216,432 | 1,218,944 | +2,512 | 353,920 |

Production returns to its original main baseline (net zero bytes versus that
baseline). Test-only growth comes from moving methods across compilation units;
the failed-health method is no longer folded into the caller at compile time.
All four checks passed with unchanged static RAM and partitions. No hardware
installation occurred during this refactor, and it does not establish a panic fix.

## Captured overflow and separate product dependency

Descriptor-only USB diagnostics on source-bound qualification v0.0.1 from
`26ca275b0c705520a678557028b361ac9cc426d6` captured the FreeRTOS stack-overflow
hook for `oa_upgrade`. The worker's compiled frame reserved 4,736 of its 8,192
stack bytes even on this path. Fresh paired reads afterward confirmed unchanged
bootloader, control/NVS, inactive application and shared audio; protected
production payloads still matched the original baseline. No installation was
requested. The device was stopped in ROM.

[PR #12](https://github.com/OpenAthan-Project/openathan/pull/12) owns the product
fix: bounded, checked internal-heap HTTP buffers reduce the worker frame to 656
bytes. Qualification depends on that separately reviewed updater change. Only
qualification builds record the minimum unused worker stack across completed
operations; the production worker contains no measurement hook or result field.
The host test verifies that a later larger sample cannot overwrite an earlier
minimum. Publishing also rejects this telemetry marker independently.

All 82 Python tests and the six updater profiles plus negative compile cases
passed. Matched pinned before/after measurements for this dependency and telemetry:

| Variant | Before bytes | After bytes | Delta | Application budget remaining |
| --- | ---: | ---: | ---: | ---: |
| Production with product fix | 1,211,744 | 1,211,744 | 0 | 361,120 |
| Ordinary isolated provisioning | 1,213,600 | 1,213,600 | 0 | 359,264 |
| Healthy qualification v0.0.1 | 1,218,944 | 1,219,088 | +144 | 353,776 |
| Failed-startup qualification v0.0.3 | 1,218,944 | 1,219,088 | +144 | 353,776 |

All four capacity/profile-exclusion checks passed. Static RAM and partitions are
unchanged. Production gains no additional bytes from qualification instrumentation.
At that checkpoint, the combined fixed-engine image still needed exact-head CI
and a descriptor/TLS hardware retest before installation qualification resumed.

## Earlier attended evidence

The initial attended USB transition passed on the reference speaker: live
identity/security, exact layout, paired scoped reads, selected-slot application
and allowlisted bootloader readback were verified. Partition table, NVS, OTA
metadata, inactive slot and shared audio remained byte-identical during the
transition. Isolated USB enrollment and a private HTTPS release check succeeded
on v0.0.1, running VALID. The hardware applications were built from
`f2273032fbab3b9f16465bb5e3efb851b285eaf8`; later feed-tool changes do not change
firmware sources. These source-bound healthy images are 1,217,088 bytes
(355,776 bytes of application budget remaining). The 32-byte difference from
the ad-hoc table above comes from embedding the full source commit.

The speaker remains on qualification firmware. The operator deferred production
restoration; the old restoration plan must not be applied.

During descriptor checks, the qualification device reported `ESP_RST_PANIC`
and lost its in-memory offer/check state. OTA erase, write, finalization and
boot-selection counters remained zero. The installation matrix stopped; the
serial diagnosis later identified updater worker stack overflow. A subsequent bad-signature descriptor was rejected
with the expected verification error, but that does not clear the reset gate.
Atom-only USB diagnostics initially could not reproduce the check because the
Pyramid audio-health gate was unavailable. Qualification firmware now permits
only descriptor checks before health confirmation; installation and controls
retain the gate, as do production checks. Host tests cover these boundaries.
Serial capture and fresh preservation comparisons precede any further write.
Fresh paired scoped reads after the panic confirmed that all protected
production record payloads and the complete shared-audio hash still match the
original baseline. The selected v0.0.1 application remains VALID in the same slot.

## Earlier fixed-worker matrix and watchdog stop

All eight combined qualification CI checks passed on
`98470e448059d391c5c4ef9d86a8de8634be9a4e`. A browser dependency-download
timeout passed on a job-only retry. The source-bound image passed six physical
descriptor checks (three invalid signatures and three valid offers) without
reset or OTA operations. Minimum observed unused worker stack was 3,976 bytes.
Fresh paired reads afterward confirmed preservation before bottom-only testing.

On that exact source, the attended interruption matrix observed:

| Case | Physical result |
| --- | --- |
| Ten interrupted attempts | Two corrupt images, three short images, two transport disconnects, one cancellation during delayed headers and two cancellations after download progress; no boot selection or reset, VALID v1 retained. |
| Pre-selection power cut | Fully staged v2, zero selection attempts; old VALID v1 returned with its durable request and unchanged isolated settings/history. A controlled short retry failed safely and cancellation cleared the request. |
| Post-selection power cut | Exactly one v2 selection; actual v2 PENDING then VALID observed, successful request cleared, settings/history preserved. |
| Pending-v3 interruption | Actual v3 PENDING observed before the operator's second cut; bootloader returned to VALID v2 and reconciled the rejected request. |
| Failed-v3 startup | Without an operator cut, v3 remained PENDING and automatically returned to VALID v2 after its failed health window. |
| Healthy-v4 handoff | v4 first appeared PENDING with task-watchdog reset reason 6, then confirmed VALID. This handoff did not pass; the matrix stopped. |

The ten interruption snapshots retained at least 193,816 bytes of internal heap
and 3,968 bytes of unused worker stack. They do not establish concurrent audio
headroom. The operator confirmed the physical power sequences; observations
establish OTA states rather than an exact instruction boundary.

Fresh paired USB reads after the watchdog confirmed original protected production
record payloads, full shared audio and reviewed bootloader. Selected app0 contains
the exact VALID v4 image and inactive app1 retains v2. At that checkpoint the
device was stopped in ROM with Atom-only USB connected; no restoration or flash
write had followed the reset.

[PR #13](https://github.com/OpenAthan-Project/openathan/pull/13) owns a separate
product fix for a source-proven blocking HTTP shutdown wait consistent with this
reset. The physical timeout backtrace was not captured. Qualification depends
on that fix as well as PR #12; neither product PR was merged at that checkpoint.
New exact-head CI, source-bound builds and corrected physical handoffs were
required before continuing, and the audio/network soak had not started. The
corrected evidence below supersedes those pending tests. Production restoration
and a real production Athan remain deferred.

Keep raw USB snapshots, credentials, signing material and device responses in a
private archive outside Git. Publish sanitized observations with their actual
timing and limitations. GitHub-hosted delivery/redirects, broader network/browser
coverage, overnight soak and public release selection remain separate untested
gates at this checkpoint.

## Corrected shutdown handoffs and retained-recording soak

The corrected integration uses source
`6eb5572db7035ea59e767cc3a19e847eb5d9a12d`, including the separate product
stack and nonblocking-shutdown fixes in PRs #12 and #13. All eight
[qualification CI checks](https://github.com/OpenAthan-Project/openathan/actions/runs/36899681268)
passed on that exact source, alongside 83 Python tests without skips, eleven
UBSan CTest suites, six updater profiles and three rejected component profiles.
All six product-shutdown CI checks passed on
`7cef4f9b57d7eb6a2e64b15dbde7b2c57931a8d5`.

Matched ESPHome 2026.9.0 / ESP-IDF 5.5.5 measurements use the same
configuration, trust material and full source-identity length:

| Profile | Before OTA bytes | Corrected OTA bytes | Delta | Application budget remaining |
| --- | ---: | ---: | ---: | ---: |
| Production reference | 1,211,744 | 1,211,696 | -48 | 361,168 |
| Ordinary isolated provisioning | 1,213,600 | 1,213,536 | -64 | 359,328 |
| Each qualification candidate v0.0.1–v0.0.4 | 1,219,088 | 1,219,024 | -64 | 353,840 |

All six capacity/profile-exclusion checks passed. Production matches the product
dependency and gains zero bytes from qualification. Static RAM remains 113,351
bytes for production/ordinary isolated and 113,431 bytes for qualification.
Both 2 MiB application slots and the separate 3.5 MiB audio partition remain
unchanged. Hardware candidates retained the approved existing shared recordings;
compile-only synthetic fixtures were never installed or played.

Corrected bottom-only tests observed the following actual states under ordinary
HTTP polling:

| Case | Result |
| --- | --- |
| Healthy v2 handoff | PENDING to VALID, software reset reason 3, successful request cleared, isolated settings/history preserved. |
| Forced failed-v3 startup | PENDING, automatic return to VALID v2 without an operator cut, rejected request reconciled, settings/history preserved. |
| Healthy v4 handoff | PENDING to VALID, software reset reason 3, successful request cleared, settings/history preserved. |

The prior watchdog reset did not recur in these corrected handoffs. The source
hazard and before/after compiled regression support the shutdown fix, but the
original physical timeout backtrace was not captured. These runs do not repeat
the earlier source's ten interrupted attempts or physical power cuts.

One earlier v3 attempt with an artificial per-chunk feed delay paused before
finalization or selection. VALID v2 remained selected, and cancellation cleared
the request before the separately recorded undelayed startup-failure test. Its
generic paused-download response did not establish the low-level cause; that
attempt remains separate failed-attempt evidence.

The retained normal and Fajr recordings started at 14:21 and 14:31 Toronto at
saved volume 75. Observed playing intervals were 259.816 and 270.536 seconds,
consistent with recording metadata of 257.149 and 268.069 seconds within the
status-sampling interval. The attended listener accepted both as complete, clear
and comfortable. Isolated prayer-consumption watermarks advanced and never moved
backward; the test schedule returned to quiet operation with all announcements
disabled.

The uninterrupted bounded run lasted 3,601.579 seconds, from approximately
18:17 to 19:17 UTC, with 720 status samples and 48 visibly completed firmware
checks. Every sample retained the exact v4 source, VALID selection, reset reason
3, healthy scheduler/clock and empty installation queue. No OTA operations were
attempted. Isolated history stayed forward; cumulative heap and worker-stack
counters did not restart. The maximum status-sample gap was 13.409 seconds.

| Runtime measure | Sampled minimum bytes | Final five-minute idle median bytes |
| --- | ---: | ---: |
| Free internal heap | 199,748 | 245,192 |
| Largest internal block | 155,648 | 200,704 |
| Free PSRAM | 7,237,216 | 8,373,000 |
| Largest PSRAM block | 7,208,960 | 8,257,536 |
| Minimum unused updater-worker stack | 3,964 | 3,964 |

The device-reported cumulative internal-heap minimum was 192,272 bytes,
including peaks between samples. Idle largest blocks recovered after playback;
free memory returned near its initial level. These observations support this
bounded run and do not establish overnight headroom or all-network TLS behavior.

An optional third normal play was refused by the private unconsumed-occurrence
guard before a settings write. The uninterrupted observer's final assertion
expected that extra play and reported a stopped result. Its exact original
trace, summary and assertion remain preserved. Independently checked trace
reconciliation passed the runbook's full duration and both required retained
tracks; it corrected only the extra-play assertion, retaining all duration,
reset, source, health, history, memory, quiet-schedule and listener guards. No
consumption history was rewound to repeat a recording.

Fresh paired read-only USB snapshots after the soak matched the original
protected production-record payloads, complete shared audio and reviewed
bootloader. Selected app0 is exact source-bound v4, sequence 5 / VALID; inactive
app1 retains exact source-bound v2. The device was left stopped in ROM after
inspection. Production restoration and a real production-schedule listening
check remain deferred.

Corrected handoffs, the two accepted recordings, the reconciled bounded soak and
final preservation have passed with the limits above. Public GitHub delivery and
redirects, broader network/browser coverage and overnight operation remain
untested. Release-media rights and public release selection remain separate
release gates.

## Production restoration guard review follow-up

The production fixes in PRs #12 and #13 have merged; qualification is now based
on `main` at `17871919dbcb5ac787fb68efafc67302771f21a6`.

The restoration planner previously checked only the qualification identity.
Ordinary isolated firmware embeds the production version without that identity,
so it could be selected for production restoration. Planning and apply-time
revalidation now use the shared release-packaging test-material filter, rejecting
all qualification and isolated-storage markers. Correctly hashed plans from an
older helper are also rejected before scoped flash reads, writes or write-journal
creation. The explicit healthy qualification bootloader transition is unchanged.

The regression fails before the fix and passes afterward for each independent
test marker. All 84 Python tests passed without skips, all eleven UBSan CTest
suites passed, and all six updater profiles plus negative compile cases passed.
Host-only checks against retained compiled production, ordinary isolated and
qualification images accepted production and rejected both test variants;
production and ordinary isolated images both embed `v0.2.0`. These checks use a
simulated stopped snapshot and mock flash I/O.

This review fix changes host tooling, its regression and documentation. Firmware
sources, configuration and embedded UI are unchanged. Earlier build sizes and
physical results remain bound to their recorded source revisions. The speaker
was not accessed or restored during this follow-up.

## USB write retry review follow-up

The scoped write journal prevented a second helper invocation but did not prevent
esptool's internal retry. In esptool 5.3.1, an interrupted write could reconnect
and reflash without repeating the helper's identity, security or snapshot guards.
The connected stub now uses one flash attempt; esptool's global default is
unchanged. A failed write leaves its journal in place for reconciliation.

Regressions exercise the real esptool write and verification paths with mocked
USB transport. Disconnects during initialization and data transfer, at both
application and bootloader addresses, fail before the fix and pass afterward:
one write attempt, no reconnect/reflash and no subsequent verification. A failed
apply retains its journal and blocks another invocation. Successful writes retain
both hash verification steps and independent readback; failures at each
verification stage never trigger another write.

All 88 Python tests passed without skips, including twelve scoped USB tests;
all eleven UBSan CTest suites also passed. This is host-only validation. No
speaker access or production restoration occurred; firmware sources and
configuration are unchanged, and previous physical evidence remains source-bound.
