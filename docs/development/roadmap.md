# Roadmap

This roadmap is intentionally high-level and will evolve as the reference hardware is tested.

Status as of 2026-09-25. Completed items below describe developer firmware and
the tested reference unit, not an end-user release. Detailed outcomes and limits
are in the [validation report](feasibility-report.md).

## Current milestone

The scheduler, offline Athan playback, time synchronization, durable replay/skip
state and [saved runtime settings](../../firmware/esphome/scheduler/SETTINGS.md)
are implemented. Settings and consumed-prayer history survived the documented
physical power cuts; consumed, disabled and skipped prayers did not replay when
moved later. Precise write/commit failures and wider timezone/clock cases are
covered by automated tests.

Next is phone-based setup and ongoing local settings. The initial direction is
USB installation followed by Wi-Fi provisioning through the same connection,
USB recovery for changed network credentials, and a handoff to local setup on a
phone or computer. Manual location entry is sufficient initially. Local access
will use a device password chosen during USB setup, with recovery through USB.
These are accepted design choices; the protocol and first-run confirmation still
need implementation. Provisioning and recovery must preserve prayer settings
and consumption.

## Phase 0 — Hardware validation

- [x] Verify AtomS3R C126 + Voice Pyramid A167 initialization.
- [ ] Verify speaker quality and practical volume for Athan/Quran.
- [ ] Verify LED and touch control access.
- [x] Verify stable audio playback while Wi-Fi is active.
- [x] Measure flash/RAM/PSRAM headroom.

Athan playback and practical volume passed on the reference unit; Quran and
release recording qualification remain open. LED checks passed, but lower-right
touch nonresponse remains unresolved. Memory measurements cover developer
builds; repeat them as setup and other features are added.

Additional items:

- [x] Verify sustained offline Athan playback and playback after a cold power cycle.
- [ ] Select and license release recordings and complete release audio qualification.

## Phase 1 — Standalone Athan MVP

- [ ] Wi-Fi provisioning.
- [x] NTP/timezone handling.
- [x] Local prayer calculation.
- [x] Calculation method and Asr method selection.
- [x] Per-prayer offsets.
- [x] Normal and Fajr Athan playback.
- [x] Persisted settings.
- [ ] Reference-build adapters for optional LED status and touch playback/volume/dismiss controls.

Additional items:

- [x] Runtime location and IANA timezone changes.
- [x] High-latitude method and enabled-prayer settings.
- [x] Saved volume with player readback and retry.
- [x] Stop, skip/cancel and durable replay protection across settings changes and restarts.
- [x] Encrypted developer console for reading and changing complete settings snapshots.
- [ ] First-run setup confirmation before automatic playback.
- [ ] Reference-build adapter for optional display status.

## Phase 2 — Installation and local UI

- [ ] Browser installer at `openathan.com/install`.
- [ ] Local setup wizard.
- [ ] Local control/configuration interface at `openathan.local`.
- [ ] OTA update flow.
- [ ] Recovery/documented reflashing process.

Developer OTA and recovery procedures exist; the complete product update flow
and ordinary-user recovery guidance remain unfinished.

Additional items:

- [x] Developer application OTA with retained audio/settings/consumption and a
  reference-device test of automatic rollback after an unconfirmed startup failure.
- [ ] Device password setup through USB and protected access to local settings.
- [ ] USB Wi-Fi and password recovery that preserves prayer settings and consumption.
- [ ] Integrate qualified firmware release artifacts into the installer and update flow.

Verified private recovery backups and developer procedures exist. Older layouts
need a deliberate USB partition migration; ordinary application OTA cannot
migrate them. Restoring historical NVS also restores historical consumption and
must not be used as routine Wi-Fi or settings recovery.

## Phase 3 — Quran and adhkar

- [ ] Quran streaming with selectable reciter/source.
- [ ] Surah/ayah playback controls.
- [ ] Resume/bookmark behavior.
- [ ] Athan interruption/resume policy.
- [ ] Morning/evening adhkar support.

## Phase 4 — Offline resilience and expansion

- [ ] Evaluate RTC expansion.
- [ ] Evaluate microSD or USB mass storage.
- [ ] Optional offline Quran library.
- [ ] Additional reference hardware if it meaningfully reduces cost or improves availability.

## Phase 5 — Optional integrations

- [ ] Home Assistant integration.
- [ ] Local APIs for third-party integrations.
- [ ] Additional automation hooks.
