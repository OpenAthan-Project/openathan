# Speaker finder handoff validation

## Behavior

When two finder owners meet, the speaker that yields continues serving the
read-only directory while its delegated hostname is being removed. After the
mDNS library confirms removal, the directory remains available on the same
eligible Wi-Fi address for 125 seconds. This covers the library's 120-second
address TTL plus a five-second margin.

After at least 1.1 seconds, the former owner sends an explicit multicast A query
for `openathan.local` on each of two discovery rounds. The surviving owner's
existing responder supplies the cache-flush address record. This also covers a
winner that never observed the competing owner. Network changes, reacquisition
and shutdown cancel the handoff. Finder responses close their HTTP connection
after sending so a pooled connection cannot outlive the transition.

## Automated validation

The worker regression failed before the fix with
`Cached finder client lost its directory as soon as the speaker yielded`.
It passes after the fix, including deferred withdrawal, the settle interval,
two refresh attempts, query failure, clock wrap, grace expiry, cancellation and
normal/stalled shutdown during a refresh with allocated results.

The HTTP harness executes the production request handler. Production,
mDNS-disabled and isolated variants pass checks for the finder route boundary,
per-device authentication, the Connection header, session closure, send failure
and close failure.

All 26 UBSan CTests and 180 pinned Python tests passed. All 17 CI firmware
profiles compiled and passed their capacity checks. The resolved-library
pipeline-completion, Dua, updater, phone HTTP, MP3 and ESP-IDF HTTP checks passed.

The complete Chromium/WebKit run finished with 621 of 622 tests passing. The
Chromium speaker-name-during-setup case timed out while clicking
`#setup-next`. Focused reruns passed in both browsers, including the six
setup-name and Dua cases rerun after rebasing.

The local branch was rebased onto main
`15d4a77f70efcbb44fcaae085214584d0417ef98`. Its CI-policy tests and the complete
Python suite passed after the rebase. A byte comparison confirmed that all 266
checked firmware inputs match the snapshot used for the firmware builds.

## Matched firmware builds

Both sides used ESPHome 2026.9.0, ESP-IDF 5.5.5, the pinned dependency manifest and
the same compile-only synthetic audio preparation. The baseline is PR #68 head
`d6b7e96482a2402a45055eef54f4e78c4507491f`. The candidate contains this handoff
change. All three production profiles passed `tools/check_feasibility.py`.

| Profile | Baseline OTA bytes | Candidate OTA bytes | Delta | Remaining 1.5 MiB budget |
| --- | ---: | ---: | ---: | ---: |
| AtomS3R / Voice Pyramid | 1,416,736 | 1,417,184 | +448 | 155,680 |
| Waveshare Box V2 | 1,420,496 | 1,420,976 | +480 | 151,888 |
| Waveshare AUDIO-Board | 1,384,288 | 1,384,768 | +480 | 188,096 |

Static RAM increased by eight bytes on each profile. The partition layout retains
both 2 MiB application slots and the 3.5 MiB shared audio partition.

## Physical validation

Phone cache recovery, on-wire packet capture, power-loss failover, OTA restart
and runtime heap, fragmentation, PSRAM and worker stack headroom during playback
remain pending. Follow the
[speaker discovery hardware checklist](../../firmware/esphome/provisioning/HARDWARE_TEST.md#speaker-discovery)
with attended, preservation-compatible firmware and approved recordings.
