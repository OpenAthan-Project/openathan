# Update download validation — 2026-10-02

The v0.2.1 candidate sizes the HTTP transmit buffer from each validated URL:
URL length plus 512 bytes, with a maximum of 4,608 bytes. The earlier 512-byte
buffer could not hold GitHub's signed asset request line. These automated results
establish the request-formatting correction; physical public-feed acceptance and
runtime memory qualification remain pending.

## Request and updater regressions

The reference build's resolved ESP-IDF 5.5.5 request formatter and header sender
are compiled unchanged against host transport/event adapters. Its actual
`http_header.c` and `http_utils.c` are compiled directly from the SDK. The tests
require the complete request line, default User-Agent, Host and terminating blank
line to reach the transport, rather than accepting only a successful return code.

- Short URLs, synthetic 905-byte URLs with queries, and 4,096-byte URLs with
  queries or long paths pass for all three allowed release hosts with adaptive sizing.
- The old 512-byte buffer fails long-query requests. A fixed 4,096-byte buffer
  fits the maximum GitHub request line but leaves only three bytes for headers;
  the SDK transmits no request. The 4,608-byte buffer transmits it completely.
- Header continuation across writes and transport write failure are covered.
- Production updater tests cover latest → tagged → asset redirects, all five
  supported redirect statuses, the four-redirect limit, unsafe/relative hosts,
  a 4,097-byte rejection, missing Location, HTTP failure and failed reads.
- Each redirect recomputes capacity and releases the previous client first.
  Client allocation/open/header failures clean up and permit subsequent checks.
  Installation failures retain the queue without erasing or selecting an image;
  a later retry can stage the verified payload.
- v0.2.1 checking a v0.2.0 descriptor with signature success injected reports
  current and refuses to queue the older version. Real cryptography has separate
  Python coverage. Existing cancellation, persistence, storage
  isolation, startup confirmation and rollback regressions pass.

The new updater regression fails against the old production sizing and passes
with the correction. No live signed query is stored in the fixtures.

The 11 CTest suites and seven updater build profiles pass with UBSan. All 88
Python tests, nine ESPHome configuration checks, the pinned installer contract
and 40 Chromium/WebKit device UI cases pass. The reference CI job now runs the
SDK regression after compiling firmware:

```sh
python tools/check_upgrade_http.py --build-dir /tmp/openathan-reference/openathan
python tools/check_upgrade_runtime.py --arduinojson /path/to/resolved/arduinojson/src
```

## Matched firmware capacity measurements

Both sides use ESPHome 2026.9.0, ESP-IDF 5.5.5 and
`esp-14.2.0_20260121`, with the same configuration, build directory, generated
public compile-only settings and dependency locks. Production version declarations
are v0.2.1 on both sides. Source is based on main `a869a03254af9ff08d1f42c9379ed0eb4a296abb`;
that same 40-character build identity is held constant to isolate the TX change.
The baseline uses `buffer_size_tx = 512`; the candidate uses URL length + 512.
These are measurement builds, not exact-commit release artifacts. Synthetic audio
fixtures remain outside the applications and must never be installed or played.

| Affected configuration | Before OTA bytes | After OTA bytes | Delta | Budget remaining | Free bytes per 2 MiB slot |
| --- | ---: | ---: | ---: | ---: | ---: |
| Reference production | 1,211,696 | 1,211,712 | +16 | 361,152 | 885,440 |
| Isolated provisioning | 1,213,536 | 1,213,536 | 0 | 359,328 | 883,616 |
| Healthy upgrade qualification | 1,219,024 | 1,219,024 | 0 | 353,840 | 878,128 |
| Forced startup-failure qualification | 1,219,024 | 1,219,024 | 0 | 353,840 | 878,128 |

All four variants pass the existing capacity, dependency, partition,
factory/OTA consistency and build-profile exclusion checks. Static RAM stays at
113,351 bytes for reference/provisioning and 113,431 bytes for qualification.
The scheduler developer configurations do not compile the changed updater.
Both application slots and the separate 3.5 MiB audio partition are retained.

| Measurement candidate | OTA SHA-256 |
| --- | --- |
| Reference | `0f177145cf1c75e2909a49087093b246d6702dd63168fa8ec5666b9f016ff044` |
| Provisioning | `91a0476bcc42fe4123bf8d6d2d488f9e41c33d17c7373e205b1269a675bbf3d6` |
| Healthy qualification | `e565b676e26aef66fabf009aed6a21c266f41e1d9a5e4bb0fa5e7edc413e2a0c` |
| Startup failure | `3da2dbe85b4815d681fc04cc2c03c9a5595c3db94bb9f6ec5a1e9ee68b9535ad` |

## Runtime and release limits

A 905-byte URL requests 1,417 TX bytes, 905 more than the previous buffer.
The maximum requests 4,608 bytes, 4,096 more than before. ESP-IDF allocates this
buffer dynamically; it is not added to the updater stack. Host cleanup assertions
show only one redirect client's TX allocation at a time. They do not establish
device heap headroom, fragmentation or PSRAM placement under concurrent playback.
No production instrumentation is introduced.

No device installation or physical test occurred for this correction. After review
and merge, build the exact selected source and coordinate an attended,
application-only USB transition using fresh preservation evidence. Verify the real
public update check while idle and during scheduled audio, then confirm settings,
prayer history and shared audio preservation. Do not queue an older published
release. Measure heap/fragmentation and PSRAM separately from static capacity;
retain any diagnostic instrumentation in an isolated maintainer build.

Published v0.2.0 assets are unchanged. v0.2.1 publication, latest selection and
website selection remain separate release gates.
