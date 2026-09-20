# Roadmap

This roadmap is intentionally high-level and will evolve as the reference hardware is tested.

## Phase 0 — Hardware validation

- Verify AtomS3R C126 + Voice Pyramid A167 initialization.
- Verify speaker quality and practical volume for Athan/Quran.
- Verify LED and touch control access.
- Verify stable audio playback while Wi-Fi is active.
- Measure flash/RAM/PSRAM headroom.

## Phase 1 — Standalone Athan MVP

- Wi-Fi provisioning.
- NTP/timezone handling.
- Local prayer calculation.
- Calculation method and Asr method selection.
- Per-prayer offsets.
- Normal and Fajr Athan playback.
- LED prayer-status indication.
- Touch controls for playback/volume/dismiss.
- Persisted settings.

## Phase 2 — Installation and local UI

- Browser installer at `openathan.com/install`.
- Local setup wizard.
- Local control/configuration interface at `openathan.local`.
- OTA update flow.
- Recovery/documented reflashing process.

## Phase 3 — Quran and adhkar

- Quran streaming with selectable reciter/source.
- Surah/ayah playback controls.
- Resume/bookmark behavior.
- Athan interruption/resume policy.
- Morning/evening adhkar support.

## Phase 4 — Offline resilience and expansion

- Evaluate RTC expansion.
- Evaluate microSD or USB mass storage.
- Optional offline Quran library.
- Additional reference hardware if it meaningfully reduces cost or improves availability.

## Phase 5 — Optional integrations

- Home Assistant integration.
- Local APIs for third-party integrations.
- Additional automation hooks.
