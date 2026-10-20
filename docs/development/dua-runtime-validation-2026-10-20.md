# Dua bounded runtime validation — 20 October 2026

This attended session collected automated runtime evidence using approved recordings
and isolated settings, credentials and prayer history. It adds no new listening
confirmation: the earlier owner-confirmed listening, Stop, offline and interrupted
download results remain in the [attended report](dua-attended-validation-2026-10-20.md).

## Source and scope

| Board | Qualification profile | Source | Volume |
| --- | --- | --- | ---: |
| AtomS3R + Voice Pyramid | Existing private upgrade qualification | `5db9be65b61dc31e4f676933a5d7c44ba34f7838` | 70% |
| Waveshare Box V2 | Private dua qualification, firmware updates disabled | `7e3f2b268f55994d0c3a52a7d22beae471643ecd` | 50% |

Each board completed two distinct calculated scheduled normal Athan–dua events
during a 35-minute observation. Calculated events used isolated location, offset
and prayer-enablement settings. The system clock and production prayer records
were preserved. The private HTTPS feed offered the approved exact-version
recording and a same-version Atom update
descriptor. Firmware image routes were denied. CI media was never installed or played.

## Playback and admission

| Board / event | Observed Athan seconds | Pause seconds | Dua seconds | Observed start lateness seconds |
| --- | ---: | ---: | ---: | ---: |
| Atom 1 | 233.777 | 2.045 | 21.296 | 1.043 |
| Atom 2 | 233.444 | 1.835 | 21.347 | 0.791 |
| Waveshare 1 | 233.824 | 2.156 | 21.208 | 0.208 |
| Waveshare 2 | 233.297 | 1.972 | 21.427 | 0.876 |

The observed sequence was `idle → Athan → pause → dua → idle` twice per board.
Phase durations use polled transitions and include sampling/request uncertainty;
the pause observations are consistent with the two-second controller delay.
During each silent pause, `playing` was false while `playback_busy` remained true.
Atom firmware check/install requests returned 409 during download,
Athan, pause and dua; Waveshare USB join/password/scan requests returned busy
without credential or network-state changes. The idle same-version Atom check
completed. Its qualification counters recorded no firmware erase, write, finalize
or boot-selection attempt. Both boards retained readiness without an observed
scheduler fault, playback error or restart.

## Runtime memory and responsiveness

These are sampled bytes from private qualification builds under repeated
authenticated status requests. Sample counts and memory minima include
installation and playback; snapshot gaps cover the bounded runtime window.
They are not measurements of final signed production images.

| Board | Samples | Minimum free internal | Minimum largest internal block | Minimum free PSRAM | Maximum bounded snapshot gap seconds |
| --- | ---: | ---: | ---: | ---: | ---: |
| Atom | 6,714 | 188,476 | 143,360 | 7,219,060 | 1.443 |
| Waveshare | 6,727 | 187,324 | 143,360 | 6,974,688 | 1.516 |

Device-reported internal low watermarks since boot were 179,812 bytes for Atom
and 178,952 for Waveshare. Maximum observed status round trips, including
installation, were 1.353 and 0.608 seconds respectively.

Recovered idle medians compare the interval after the first sequence with the
final idle interval, excluding the first five seconds after playback:

| Board | Free internal change | Largest internal block change | Free PSRAM change | Largest PSRAM block change |
| --- | ---: | ---: | ---: | ---: |
| Atom | -184 | +0 | +4 | +0 |
| Waveshare | -140 | +0 | +0 | +0 |

The largest internal block recovered to a 196,608-byte idle median on both boards.
These samples include download and playback; the small recovered free-heap changes
above do not establish either a leak or long-term stability.

Completed-worker stack low watermarks were 4,112 bytes for Atom’s firmware
descriptor worker and 4,168 bytes for Waveshare’s dua downloader. They do not
measure Atom’s dua worker or either firmware image-write worker.

The private archive retains per-phase minima, medians, largest-block/PSRAM values,
monotonic transitions, request timings and complete traces. Snapshot gaps and
observed event lateness are responsiveness evidence; they are not direct scheduler
tick instrumentation or proof of an exact worst-case continuity bound. Two cycles
do not establish long-term leak freedom, extended streaming or maximum-output margin.

## Recovery and discovered issue

Atom initially rejected USB setup because a retained, checksummed empty USB update
journal was treated as active until audio health confirmation. On top USB alone,
Pyramid audio health cannot be established. A read-only isolated diagnostic identified
USB ownership while playback, dua and phone setup were idle. The fix at `5db9be6`
releases only journals with both queue and handoff empty, without a startup storage
write. New clears use schema 1. Real queues, handoffs, startup health and boot-selection
protections remain covered by regressions. The device then admitted setup normally.

Fresh paired complete flash captures and independent post-write readbacks protected
the inactive slot, boot metadata, production records and original Athan bytes.
Before restoration, each downloaded bank independently passed signature, hash, size,
commit-marker and canonical-padding validation: 97,700 approved bytes, generation 1.
Only the selected application and the session-owned bank were restored. Latest NVS
was retained; historical settings/history were never restored. Both speakers returned
to their original v0.7.0 application, source `9d7e87c`, with healthy ready/idle status,
unchanged production settings/history and the complete original audio hash. Atom
was verified on bottom power with top USB disconnected. Restoration completed before
the 15-minute safety boundary for the real Maghrib occurrence.

## Delivery limits

The [release preparation](v0.8.0-preparation-2026-10-20.md) records pinned builds,
byte budgets, browser/host checks and signed local consumer validation. Exact signed
release installation/update/recovery, production-origin TLS headroom, long soak and
Waveshare OTA-worker hardware coverage remain unqualified here. Waveshare updates
stayed disabled. Merge, tagging, upload, publication and website adoption remain
separate steps; this session performs none of them.
