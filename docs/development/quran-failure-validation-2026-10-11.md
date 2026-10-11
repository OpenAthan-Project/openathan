# Quran failure harness validation — 2026-10-11

The controlled failure harness prepares an isolated Waveshare development profile
and private HTTPS fixtures. This report covers automated validation and actual
OTA capacity; the new profile has not been installed or physically qualified.
Use the [attended failure runbook](quran-failure-qualification.md) for that later
session. This change does not establish release readiness.

## Automated validation

- CMake/CTest: all 20 host cases passed under UndefinedBehaviorSanitizer after
  rebasing onto `main` at `f12c15ecaa546edaae7267a74051e4b06598827f`.
- The complete pinned Python suite passed all 135 tests without skips.
- All 14 CI ESPHome configurations passed schema validation.
- The production Quran worker passed UBSan fault tests for HTTP initialization,
  connection/header failures, malformed and incomplete JSON, disconnected and
  stalled reads, slow trickles exceeding the absolute deadline, HTTP 503, Stop
  during reads, absence of automatic retries and explicit fresh-Play recovery.
- A separate UBSan executable passed private-origin routing, preserved logical
  redirect restrictions, redirect-loop bounds, client cleanup, private trust
  selection and stack-observation aggregation.
- Python tests exercised real loopback TLS with trusted, untrusted and wrong-host
  certificates; unfinished handshakes; HTTP 503, malformed/oversized metadata,
  partial bodies, stalls, MIME/decode fixtures and logical redirect targets.
  They also checked private-file permissions, full MP3 validation, media hash
  immutability, actual profile generation, production component exclusion,
  required isolated storage/disabled updates and release rejection.
- The updater host suite passed with the extended shared HTTP test adapter,
  including production, isolated, disabled-update and upgrade qualification
  variants.

The HTTP audio payloads used by host tests are transport fixtures. They are
neither installed nor played. Decoder failure and audible recovery are pending
physical results; the fixture server's response is not such a result.

## Normal fixture transfer regression

The original whole-body write treated five seconds as a limit on sending an
entire recording. The fixture server now sends 4 KiB memoryview slices through
its unbuffered writer, with a five-second timeout per write and no total recording
deadline. Handshake and request-read timeouts remain five seconds; the eight-worker
limit, media validation/hash checks, headers, truncation and intentional stalls
are preserved.

Real loopback TLS tests generate a valid **4,802,394-byte** MP3 outside Git and
select it through complete FFmpeg validation. Accepted-socket send buffers and
client receive buffers request 16 KiB; kernels may adjust the actual allocation.
The client reads at 128 KiB/s for eight seconds, then drains the response. The
updated server returns HTTP 200, the full declared length, identical bytes and
a matching SHA-256. Restoring the original whole-body write in the same test
fails: only **1,097,728 of 4,802,394 bytes** arrive.

A reader that stops after the headers releases its worker within the five-second
write bound plus two seconds of scheduling tolerance, leaving an incomplete body.
Another client can fetch metadata while that audio worker is blocked. Disconnecting
an unread transfer releases the worker within two seconds, and subsequent metadata
requests succeed. Each scenario logs one audio request, with no automatic retry.
The existing TLS rejection, redirect, media-immutability, truncation and stall
tests continue to pass.

This correction changes host tooling, tests and documentation only. The firmware
measurements below retain their original `2cea368` baseline; the rebase also
includes the separately measured [PR #44 audio changes](atom-volume-fence-2026-10-11.md).
Those dated comparisons are not new measurements of the rebased firmware. Current
PR CI rebuilds all affected variants and enforces the existing capacity contract.

## Matched pinned firmware measurements

The baseline is merged `main` at
`2cea36869831f3e16f692bb2737cdea2b6fe0887`. Before/after source snapshots use
identical public compile-only settings, configurations, substitutions and build
identity, Python 3.13.5, ESPHome 2026.9.0, tzdata 2026.4, ESP-IDF 5.5.5, the reviewed
Xtensa toolchain and resolved component pins. Builds use separate disposable
output directories. Actual `firmware.ota.bin` payloads are measured; synthetic
shared-audio fixtures are compile-only and must never be installed or played.

The existing capacity checker passed every image, including dependency locks,
factory/OTA consistency, exact partition table, qualification component exclusion
or inclusion, image identity and audio separation. Both 2,097,152-byte application
slots and the separate 3,670,016-byte shared-audio partition remain intact.

| Configuration | Before OTA bytes | After OTA bytes | Delta | Remaining application budget | Free slot bytes | Static RAM bytes (delta) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Atom reference | 1,301,472 | 1,301,472 | 0 | 271,392 | 795,680 | 115,675 (0) |
| Waveshare production | 1,309,728 | 1,309,728 | 0 | 263,136 | 787,424 | 116,171 (0) |
| Waveshare diagnostics | 1,374,800 | 1,374,800 | 0 | 198,064 | 722,352 | 116,831 (0) |

The new Quran qualification profile is **1,377,264 bytes**, adding **2,464 bytes**
over ordinary diagnostics. It retains **195,600 bytes** against the existing
1,572,864-byte application limit and **719,888 bytes** in its 2 MiB slot. Static
RAM is **116,839 bytes**, an eight-byte increase over diagnostics. Production and
ordinary diagnostic images have no OTA or static RAM growth from these guarded
hooks. Different build timestamps and paths
can change hashes without changing image size; these measurements are tied to
this comparison and do not replace prior dated measurements.

The qualification image contains a session-specific public CA and observations;
its measurements are specific to that fixture configuration. Static RAM is
separate from runtime heap, fragmentation, PSRAM and stack headroom. No new
runtime or recovery claim follows from a successful build.

No device connection, reset, flash write, listening, schedule change or release
operation occurred in this harness implementation session. Continuous real-provider
soak and attended device failure/recovery acceptance remain separate work.
