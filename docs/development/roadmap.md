# Roadmap

This roadmap tracks available capabilities and future product work. See the
[latest published release](https://github.com/OpenAthan-Project/openathan/releases/latest)
for current downloads and notes. Checked items describe implementation, not
blanket physical qualification; release notes and dated validation reports
retain their evidence limits.

## Current milestone

Standalone Athan is available on AtomS3R C126 + Voice Pyramid A167, Waveshare
Box V2 and Waveshare AUDIO-Board, with local setup, prayer scheduling, stored
normal/Fajr audio, device controls and signed firmware updates. Releases include
MP3Quran streaming (v0.6.0), first-run phone setup and connection QR screens
(v0.7.0), optional offline dua after scheduled Athan (v0.8.0), AUDIO-Board
support (v0.9.0), speaker
names and the local finder (v0.10.0), and automatic updates (v0.11.0).
Compare the exact supported models in the [hardware guide](../../hardware/README.md).

First use needs internet time. Atom needs synchronization again after a cold
boot; AUDIO-Board and Box V2 support onboard PCF85063 clock restoration after a
powered restart. Unplugged retention, backup-battery operation and long-term
drift remain unqualified. Once the clock is valid, prayer calculation and
stored-recording playback work independently of the public website and Home
Assistant.

## Phase 0 — Product foundations

- [x] Reusable product core with hardware-specific adapters.
- [x] Shared recording storage and dual-slot firmware recovery.
- [x] Licensed release recordings with redistribution rights and attribution.

## Phase 1 — Standalone Athan MVP

- [x] Wi-Fi provisioning and saved runtime settings.
- [x] NTP, IANA timezone and recurring DST handling.
- [x] Local prayer calculation, calculation/Asr methods and high-latitude rules.
- [x] Per-prayer offsets and enabled-prayer settings.
- [x] Stored normal and Fajr Athan playback and saved volume.
- [x] Stop, skip/cancel and durable replay protection across saves and restarts.
- [x] First-run confirmation before automatic playback.
- [x] Optional encrypted developer settings/diagnostic console.
- [x] Optional LED countdown/status adapter with independent on/off and brightness.
- [ ] Optional touch playback/volume/dismiss adapter.
- [x] Optional GC9107 AtomS3R display-status adapter.
- [x] Waveshare Box V2 audio, round display and BOOT controls.
- [x] Waveshare AUDIO-Board audio, RGB lights and physical volume/Stop controls.

Audio is fundamental; optional peripherals do not gate the reusable scheduler.

## Phase 2 — Installation, local UI and updates

- [x] Browser installer at `openathan.com/install` consuming approved release artifacts.
- [x] Device-local setup, settings and playback controls.
- [x] First-run phone Wi-Fi setup, with manual entry or optional screen QR.
- [x] Button-accessible speaker webpage QR and hostname/IP information.
- [x] USB device-password setup and settings/history-preserving credential recovery.
- [x] Optional HTTPS location suggestions, reviewed and saved on the device.
- [x] Signed application-only update discovery, owner queue/cancel controls and safe-window installation.
- [x] Startup health confirmation and automatic rollback.
- [x] Documented USB migration paths for older firmware and bootloaders.
- [x] Automatic website adoption of approved stable releases.
- [x] Browser USB firmware updates that preserve saved data on compatible firmware.
- [x] Saved speaker names and a local browser finder.
- [x] Automatic signed updates between prayers, with a saved opt-out.

## Phase 3 — Quran, dua and adhkar

- [x] MP3Quran streaming with reciter, edition and surah selection.
- [x] Athan priority over Quran playback.
- [x] Optional offline dua after scheduled Athan, with recording installation for existing speakers.
- [ ] Additional streaming providers.
- [ ] Ayah playback controls.
- [ ] Resume/bookmark behavior.
- [ ] Morning/evening adhkar support.

## Phase 4 — Offline resilience and expansion

- [x] Onboard RTC restoration for Waveshare AUDIO-Board and Box V2.
- [ ] Backup-power timekeeping and long-term offline resilience.
- [ ] RTC support beyond the two supported Waveshare models.
- [ ] Removable audio storage, such as microSD or USB mass storage.
- [ ] Optional offline Quran library.
- [ ] Additional reference hardware if it reduces cost or improves availability.

## Phase 5 — Optional integrations

- [ ] Home Assistant integration.
- [ ] Documented APIs for third-party integrations beyond the current local UI contract.
- [ ] Additional automation hooks.
