# Curated Athan implementation validation

Date: 2026-11-16. This records implementation checks, not release or hardware
acceptance. See the [recording contract](athan-recordings.md) and
[catalog authoring procedure](athan-library.md).

## Automated behavior

The final recovery implementation passed `tools/check_athan.py` under UBSan for
production, isolated-storage and updates-disabled profiles. The production
service runs with deterministic flash, NVS, transport, task and playback adapters.
Checks include:

- Recovery from all 11 small-fixture flash operations, six torn-header prefixes,
  and seven sampled interruptions in a maximum pair ending at `0x300000`.
- Journal commit uncertainty, corrupt or unreadable startup metadata, unavailable
  partition lookup and firmware identity, and preservation of write ownership.
- Cancelled workers followed by a different, larger selection, without stale
  staging reuse or writes before verified intent.
- Same-firmware repair, pending-firmware exclusion, fresh playback bindings,
  preserved dua banks and the persisted dua startup requirement.

The cancellation and initial startup regressions failed before their fixes.
ASan plus UBSan also passed the recovery implementation before the final
partition-lookup guard was moved earlier. That last guard has a separate
before/after failing-to-passing regression under UBSan.

All 26 CTests passed after the final recovery fix. The earlier complete Python
suite passed 186 tests. A final repeat encountered sandbox restrictions in
localhost fixture servers and unavailable dependency downloads. Its five failures
and 17 errors are not counted as a pass. The six focused catalog tests passed
again on the final implementation.

Catalog tests execute real P-256 signing and the CLI, including rejection of an
uncommitted replacement public key. Firmware host tests substitute mbedTLS
signature verification; they do not establish the real device TLS/signature path.

The browser suite passed 672 Chromium/WebKit cases before the last storage-status
readback adjustment. All 24 focused Chromium/WebKit cases passed afterward with
the final UI assets. They use the production embedded assets and Digest verifier
with simulated device APIs. Desktop and narrow-screen captures were inspected.

## Matched application measurements

ESPHome 2026.9.0, ESP-IDF 5.5.5 and the same compiler, profile configuration,
synthetic media and non-development build identity were used on both sides.
The baseline contains firmware/UI from main `3145a51`. The measured feature
snapshot is `34ff453`, before the final recovery fixes. The fixed identity keeps
the installer enabled on both sides and avoids measuring the disabled
development path.

| Profile | Baseline OTA bytes | Feature OTA bytes | Delta | Remaining under 1.5 MiB |
| --- | ---: | ---: | ---: | ---: |
| AtomS3R | 1,437,872 | 1,464,928 | +27,056 | 107,936 |
| Waveshare Box V2 | 1,441,728 | 1,468,736 | +27,008 | 104,128 |
| Waveshare Audio Board | 1,405,328 | 1,432,640 | +27,312 | 140,224 |

`tools/check_feasibility.py` passed these artifacts. Both 2 MiB application slots
and the separate 3.5 MiB audio partition remain unchanged. These numbers are
pre-final measurements.

The final Atom firmware built successfully at **1,464,976 bytes**, an increase
of **27,104 bytes** over the matched baseline, with **107,888 bytes** remaining
under the application budget. Its SDK configuration matches the baseline and
its capacity checks passed. This build contains the firmware and UI source
before the review fixes below.

At that checkpoint, the final Box V2 and Audio Board rebuilds were skipped after
sandbox denial of SDK cache writes. The review-fix measurements below supersede
that pending build status.

## Review-fix revalidation

The review fixes restore developer-profile compilation and clear transient
connection feedback after healthy recording-status reads. Production retains
the same stopped-reader checks. Upstream profiles cannot authorize destructive
replacement without those checks.

The upstream compile regression failed with `no member named 'playback_stopped'`.
Both browser recovery cases failed in Chromium and WebKit
because old reconnect or unavailable text remained after healthy reads.
After the fixes, the upstream, Quran and dua-only adapter targets built, all
26 CTests passed, and all 34 focused Chromium/WebKit cases passed. The production
recovery checks, six catalog tests and seven audio-image tests also passed.
Independent correctness and comment reviews found no actionable issues.

Both previously failing scheduler profiles also compiled and passed capacity
checks. The device image is 1,130,896 bytes, and the validation image is
1,148,560 bytes. The validation build passed on a separate retry after the first
attempt encountered incomplete local component-cache files.

Matched production builds compare `2e846d1` with the corrected source in
`c110c4b`. Both sides use the pinned toolchain and the same non-development
measurement identity described above. SDK configuration, resolved dependencies
and partitions match within each pair.

| Profile | Before OTA bytes | After OTA bytes | Delta | Remaining under 1.5 MiB |
| --- | ---: | ---: | ---: | ---: |
| AtomS3R | 1,464,976 | 1,464,992 | +16 | 107,872 |
| Waveshare Box V2 | 1,468,768 | 1,468,784 | +16 | 104,080 |
| Waveshare Audio Board | 1,432,688 | 1,432,704 | +16 | 140,160 |

Capacity checks passed for all three production profiles. Static RAM did not
change. The measured images leave at least 628,368 free bytes in their 2 MiB
slots. These builds use synthetic compile-only media and do not establish
physical qualification.

## Storage and qualification limits

The current default pair occupies 2,887,932 bytes before the protected dua banks.
The largest prepared candidate combination occupies 2,930,012 bytes, below the
3,145,728-byte Athan boundary. Only the selected pair is retained. There is no
complete fallback copy during destructive replacement. The independent journal
is 820 bytes.

No device was flashed or accessed for this implementation. Physical interrupted
writes, offline restart, runtime internal heap, PSRAM fragmentation, task stacks,
playback coexistence and audible quality remain unqualified on supported boards.
Synthetic CI recordings must never be installed or played.

Candidate recordings remain unapproved. The unsigned review bundle is not a
published catalog. Provision the catalog repository, complete media review, and
publish an approved immutable signed release separately before enabling public
selection. Do not infer release readiness from host checks or application size.
