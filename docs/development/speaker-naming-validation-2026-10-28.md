# Speaker naming — build and validation report, 2026-10-28

This feature adds an optional browser-facing speaker name without changing the
network hostname or physical display. Names save separately from prayer settings,
activation and consumption history. The baseline is main
`b72be7f7a9900874e291341291be2ad07a1809a5`; the candidate is the speaker-naming
implementation accompanying this report. These are compile-only development
builds, not release artifacts or physical qualification.

## Matched production builds

Both sides used pinned ESPHome 2026.9.0, the repository's reviewed ESP-IDF/compiler
and component pins, the same per-board configuration and development commit
identity. Generated sdkconfig bytes and managed-component pins matched for every
pair. The existing capacity checker passed the actual factory/OTA images, pins,
partition contract and application budget for all three candidates.

| Production profile | Baseline OTA bytes | Candidate OTA bytes | Delta | Budget remaining | Slot free |
| --- | ---: | ---: | ---: | ---: | ---: |
| AtomS3R + Voice Pyramid | 1,401,440 | 1,406,144 | +4,704 | 166,720 | 691,008 |
| Waveshare V2 | 1,405,312 | 1,409,952 | +4,640 | 162,912 | 687,200 |
| Waveshare AUDIO-Board | 1,369,168 | 1,373,872 | +4,704 | 198,992 | 723,280 |

The limit remains 1,572,864 bytes per actual OTA application within each
2,097,152-byte slot. Both slots and separate shared audio storage are retained.
Static RAM increased by 56 bytes per profile. The name payload is bounded to
128 UTF-8 bytes, stored in a 144-byte independent record; the browser assets
remain gzip-compressed with no new font, framework, image or dependency.

## Automated coverage and limits

All 25 UBSan host tests and all 176 pinned Python tests passed without skips,
including real JSON API tests in production and isolated storage modes. Browser
scenarios exercise explicit saving, independent setup, Arabic/emoji and limits,
safe text rendering, defaults, drafts, conflicts, interrupted responses, edits
during a save, storage faults and old firmware. Desktop and phone captures
were inspected, including a 32-character unbroken name. The PR records the full
Chromium/WebKit regression and CI results for its exact revision.

No speaker was connected, flashed, restarted or played during this work. Runtime
heap, fragmentation, PSRAM and concurrent networking/audio headroom were not
measured; static RAM and build success do not establish those limits. Physical
restart/update retention remains pending attended validation using the relevant
runbook and fresh device/recovery evidence.
