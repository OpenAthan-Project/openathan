# Discovery during playback: Box V2, 2026-11-11

This focused physical check covers the v0.10.0 finder candidate at
`1dfd0f0829b506f33cdc5015070b4da40505cf79` on the confirmed Waveshare Box V2,
`openathan-601c54.local`. It supplements the
[release preparation record](v0.10.0-preparation-2026-11-10.md).

## Clean candidate playback

The installed release application was tested first, without diagnostic changes.
The device loaded its real MP3Quran catalog and streamed Ibrahim Al-Akdar,
recording edition 1, surah 2. Two bounded playback windows of at least 90 and
45 seconds included Stop and an explicit replay. The existing saved volume was
retained.

All 29 finder/authenticated-status request pairs passed, including 20 during
playback. Each finder response listed the Box once. Status retained the same
boot identifier, no scheduler fault, and unchanged settings, settings revision,
prayer-consumption history, setup, speaker name and display/light/time-format
preferences. Both Stop operations returned to idle. The owner confirmed clear
audio, successful Stop and a clean replay.

## Temporary measurement probe

Production status does not expose detailed heap or stack counters. A private
source export of the same candidate therefore added read-only measurements to
the existing authenticated Quran status response. It sampled ESP-IDF heap
information and heap integrity, and recorded the discovery and Quran metadata
workers' stack low-water marks from inside those workers. It retained the
production audio, discovery, networking and storage behavior. No test clock,
schedule, credentials, recording or durable probe state was written.

The probe and its build inputs, patch, logs and binary remain outside Git and
the release bundle. Its marker was `OPENATHAN_DISCOVERY_LOAD_PROBE_V1`; its
application SHA-256 was
`e01630a6c44e76cf84365c0c1a89b705e459d9a1a8c99eb7d6350db9142956d6`.
The same pinned Python 3.13 / ESPHome 2026.9.0 / ESP-IDF 5.5.5 toolchain and
Box V2 production configuration passed configuration and capacity checks.

| Measurement | Clean candidate | Probe | Delta |
| --- | ---: | ---: | ---: |
| OTA application bytes | 1,425,392 | 1,427,280 | +1,888 |
| Static RAM bytes | 117,663 | 117,687 | +24 |
| Remaining 1.5 MiB application budget | 147,472 | 145,584 | -1,888 |

Both retain the dual 2 MiB application slots and separate 3.5 MiB audio region.

## Measured combined workload

The instrumented run repeated the catalog, Play, Stop and replay sequence with
the finder active. All 29 request pairs passed, including 20 during playback.
All 29 sampled heap-integrity checks passed, the boot identifier stayed constant,
and no playback error or scheduler fault occurred. The discovery worker advanced
from round 7 to round 43, and four Quran metadata jobs completed.

| Runtime observation | Bytes |
| --- | ---: |
| Minimum sampled free internal RAM | 180,556 |
| SDK minimum free internal RAM since boot | 173,264 |
| Largest free internal block, lowest sample | 139,264 |
| Minimum sampled free PSRAM | 6,911,160 |
| Largest free PSRAM block, lowest sample | 6,815,744 |
| SDK minimum free PSRAM since boot | 6,901,300 |
| Discovery worker minimum unused stack | 3,064 |
| Quran metadata worker minimum unused stack | 3,156 |

ESP-IDF reports these stack values in bytes. SDK minimum counters include startup
and catalog/TLS work; sampled free/largest-block values describe status snapshots.
The measurements include the probe's own overhead.

After each Stop, sampled free internal RAM returned to approximately 229 KB and
free PSRAM to 8.1 MB. Largest free blocks settled below the initial idle values:
143,360 internal bytes and 6,946,816 PSRAM bytes after both playback cycles. The
second cycle retained those block sizes. This is evidence of available headroom
and recovery during this bounded workload, not proof of zero fragmentation or
long-term leak freedom.

## Preservation and scope

Two fresh full 16 MiB recovery reads matched before installing the probe. Its
application-only write and full readback preserved every byte outside the
selected application erase extent, including OTA metadata, credentials, settings,
prayer history, the previous application and shared audio.

After testing, the exact fresh clean-candidate application-slot backup was
restored. Independent full-flash readback matched all 16 MiB of the pre-probe
backup byte for byte, including the application-slot tail. Application SHA-256
returned to
`de0433aa8c73b4fe7b5faed00fef8ed6c571e1cb9f519b100e73b5f5f4cd4dbd`.
No historical settings/history backup was restored.
After restart, USB health was confirmed, clock and scheduling were ready, playback
was idle, and the diagnostic fields were absent. Three final finder/authentication
checks passed; settings, their revision, prayer history and preferences matched
the beginning of this session.

These results cover the stated short workload on one Box V2. They do not establish
maximum-load or long-duration behavior, scheduled Athan timing/preemption, phone
browser behavior or two-speaker takeover. The newer Quran UI from PR #72 is
absent from this candidate.
