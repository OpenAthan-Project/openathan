# Roadmap

Status as of **2026-10-03**, following the v0.2.1 reference release and automatic
website release adoption. Checked items describe implemented or published
capabilities, not blanket physical qualification. See the [current release
evidence and limits](release-validation-2026-10-03.md), [earlier device results](feasibility-report.md)
and [upgrade qualification](upgrade-qualification-2026-10-01.md).

## Current milestone

The standalone Athan reference experience is available on AtomS3R C126 +
Voice Pyramid A167: precompiled USB installation, local setup/settings,
scheduled stored normal/Fajr audio, durable skip/replay protection, optional
prayer lights, USB credential recovery and owner-requested signed application
updates. The website verifies and adopts approved stable releases; it never
compiles firmware or updates a speaker on its owner's behalf.

The current firmware needs internet time synchronization after a cold boot.
Once the clock is valid, calculations and stored-recording playback operate
without the public website or Home Assistant. Compatible firmware updates
preserve installed audio; replacing recordings is separate from application OTA.

## Phase 0 — Hardware and release validation

- [x] Verify AtomS3R C126 + Voice Pyramid A167 initialization.
- [x] Verify complete normal/Fajr Athan playback and practical volume on the reference unit.
- [x] Verify both LED groups and integrated prayer/status output in bounded tests.
- [x] Verify audio playback with Wi-Fi active and offline after time synchronization.
- [x] Measure application capacity and source-bound runtime heap/PSRAM/stack behavior.
- [x] Approve release recordings and document redistribution rights and attribution.
- [ ] Resolve lower-right touch nonresponse and implement optional touch controls.
- [ ] Qualify broader hardware/browser/network coverage and longer runtime operation.

Measurements and physical results remain tied to their tested images. The exact
v0.2.1 factory bundle has not received a new physical fresh-install test, and its
maximum-length update-buffer allocation has not been measured during concurrent
playback. Earlier accepted evidence is retained with these limits.

## Phase 1 — Standalone Athan MVP

- [x] Wi-Fi provisioning and saved runtime settings.
- [x] NTP, IANA timezone and recurring DST handling.
- [x] Local prayer calculation, calculation/Asr methods and high-latitude rules.
- [x] Per-prayer offsets and enabled-prayer settings.
- [x] Stored normal and Fajr Athan playback and saved volume with readback/retry.
- [x] Stop, skip/cancel and durable replay protection across saves and restarts.
- [x] First-run confirmation before automatic playback.
- [x] Optional encrypted developer settings/diagnostic console.
- [x] Optional LED countdown/status adapter with independent on/off and brightness.
- [ ] Optional touch playback/volume/dismiss adapter.
- [x] Optional GC9107 AtomS3R display-status adapter (development candidate;
      [bounded isolated hardware acceptance recorded](display-validation-2026-10-03.md)).

Audio is fundamental; optional peripherals do not gate the reusable scheduler.

## Phase 2 — Installation, local UI and updates

- [x] Browser installer at `openathan.com/install` consuming approved release artifacts.
- [x] Device-local setup/settings/control interface at a unique `openathan-<suffix>.local` address, with an IP fallback.
- [x] USB device-password setup and settings/history-preserving credential recovery.
- [x] Optional HTTPS location suggestions, reviewed and saved on the device.
- [x] Signed application-only update discovery, owner queue/cancel controls and safe-window installation.
- [x] Startup health confirmation, compatible bootloader checks and automatic rollback.
- [x] Document preserving USB transitions for older firmware/bootloaders.
- [x] Automatic website adoption of published stable latest releases after validation.

The fresh installer erases settings, credentials and prayer history. Existing
speakers use compatible device-page updates or a reviewed preserving USB
transition. Older partition layouts need a deliberate migration; ordinary
application OTA cannot change them. Historical NVS restores old consumption
history and is not routine recovery.

The signed updater's interruption, handoff and rollback tests use a separate
maintainer qualification build. Production excludes its private feed, isolated
records, instrumentation and fault injection. Successful real public descriptor
discovery is separate from public OTA application-transfer acceptance.

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
- [ ] Additional reference hardware if it reduces cost or improves availability.

## Phase 5 — Optional integrations

- [ ] Home Assistant integration.
- [ ] Documented APIs for third-party integrations beyond the current local UI contract.
- [ ] Additional automation hooks.
