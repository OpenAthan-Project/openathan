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

## Conflict-recovery correction

An unreleased name draft could encounter a concurrent rename through status
polling or after a rejected save. **Retry with my edits** previously read the
saved name without submitting that draft. Retry now validates and captures the
name at the click, reads authoritative state and queues that captured value
through the serialized writer. Later edits remain unsaved; a name already
persisted needs no duplicate write. **Check saved state** remains a read action.

The correction was compared with PR head
`f450b63733a592a9f47dc4455980e05e1f327168` using freshly rebuilt baselines and
the same pinned environment, source location, per-board configuration and
development commit identity for each pair. Generated sdkconfig bytes and
dependency locks matched. All three existing capacity checks passed.

| Production profile | Before correction OTA bytes | Corrected OTA bytes | Delta | Budget remaining | Slot free |
| --- | ---: | ---: | ---: | ---: | ---: |
| AtomS3R + Voice Pyramid | 1,406,144 | 1,406,416 | +272 | 166,448 | 690,736 |
| Waveshare V2 | 1,409,952 | 1,410,240 | +288 | 162,624 | 686,912 |
| Waveshare AUDIO-Board | 1,373,872 | 1,374,160 | +288 | 198,704 | 722,992 |

Static RAM did not increase relative to the original naming implementation.
The original application budget, both slots and shared audio are unchanged.
All 25 UBSan host tests and 176 pinned Python tests passed without skips,
including production and isolated JSON API tests. All 80 focused naming and
prayer-review browser cases passed without skips. New Chromium/WebKit cases
cover unreleased conflicts, normalization and validation, edits during readback,
queued and active saves, failed/stale readback, storage faults, duplicate-write
prevention, repeated conflicts, uncertain saves and saved-value recovery.
No physical validation or runtime memory measurement was added by this correction.
