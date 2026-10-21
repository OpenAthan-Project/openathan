# Product

<!-- impeccable:product-schema 1 -->

## Platform

web

## Users

The primary users are Muslim households who want a standalone Athan speaker
they can set up and control from a phone without coding or Home Assistant.
The intended reference experience uses pre-assembled modules and precompiled
firmware, without requiring soldering, PCB design, YAML, or embedded-development
experience. The public USB installer distributes precompiled reference releases
for [supported speakers](hardware/README.md), currently AtomS3R C126 + Pyramid
A167 and Waveshare Box V2.

ESPHome makers are a secondary audience. They should be able to reuse OpenAthan
with compatible hardware while retaining control of their board configuration,
networking, credentials, and optional peripherals.

## Product Purpose

OpenAthan brings scheduled Athan playback into the home through an open-source,
low-cost DIY speaker. The immediate product is standalone Athan: calculate prayer
times locally, play the appropriate normal or Fajr recording, and let the
household review and change its settings through a local browser interface.
Published firmware also provides owner-requested MP3Quran surah streaming and
optional dua after scheduled Athan. Broader adhkar remains future scope. Release
availability and physical qualification are separate; see the
[current release notes](docs/releases/v0.8.0.md) for evidence and limitations.

Success means a household can configure its location and prayer conventions,
understand when announcements will play, and control playback with confidence.
Settings changes, restarts, and recovery must preserve the history that prevents
unwanted repeat announcements.

## Positioning

The product combines an approachable reference build with reusable,
hardware-independent prayer and scheduling logic. Core operation does not depend
on Home Assistant, the public website, or a cloud service. Audio is fundamental;
the display, LEDs, touch controls, microphone, and other peripherals are optional
capabilities rather than prerequisites.

## Operating Context

This repository owns firmware, release artifacts, and the device-local browser UI.
The separate [public website repository](https://github.com/OpenAthan-Project/website)
owns public documentation and the browser installer. It consumes published
firmware artifacts and does not compile firmware.

Supported speakers have their own parts/setup guides in the hardware index.
USB provisioning establishes Wi-Fi and a device password. First-run phone setup,
available since v0.7.0, offers a temporary open setup network for fresh devices,
with automatic page opening and a manual local-address fallback. Either path leads to the
device's unique `openathan-<suffix>.local` address, or its IPv4 address,
from a phone on the same local network. USB continues to provide Wi-Fi and
password recovery; configured-device outages do not reopen enrollment.

First-run setup asks for location, timezone, and calculation settings. The user
can preview the timetable before explicitly finishing setup. Ongoing use includes
checking today's schedule and readiness, adjusting prayer settings and volume,
stopping playback, and skipping or restoring the next announcement.

Local calculations and stored audio support standalone operation once the clock
is valid. Time synchronization still needs a usable time source; saved settings
alone cannot establish the current time after a cold boot. Atom requires internet
time; Waveshare can restore initialized onboard-clock time across powered
restarts, with unplugged retention and battery behavior still unqualified.

## Capabilities and Constraints

- Reference firmware implements prayer calculations, calculation and Asr methods,
  high-latitude handling, prayer offsets, enabled prayers, normal/Fajr playback,
  persistent settings, volume, and durable skip/replay protection.
- The reference UI implements local setup, schedule preview, status, settings,
  and playback controls. Access uses the device password. Incomplete first-run
  setup does not announce or consume prayers.
- Preserve unsaved edits during status refreshes. Respect settings revisions,
  distinguish saved from applied settings, and reconcile uncertain writes by
  reading device state before retrying. Previews must not change durable state.
- The UI is plain HTML, CSS, and JavaScript compressed into firmware. Its assets
  must work without a public website, CDN, or external font service. npm tooling
  currently supports browser tests rather than a separate application build.
- Keep each actual reference OTA application image at or below 1,572,864 bytes
  in its 2 MiB slot, retaining both slots and the separate 3.5 MiB shared audio
  partition. UI changes must be measured against this firmware budget; runtime
  heap and PSRAM need separate assessment. See [repository guidance](AGENTS.md).
- Keep reusable product logic independent of ESPHome and reference hardware.
  Board-specific behavior belongs in adapters. Missing optional peripherals
  must not prevent core scheduling or local controls.
- Settings, prayer-consumption history, shared audio, and recovery capability
  must survive compatible updates and recovery. Historical full-flash images
  are not routine settings or credential recovery mechanisms.
- Pre-1.0 reference releases include the public installer, phone/USB setup,
  approved stored recordings, optional displays/lights, and owner-requested
  signed application updates. Website release adoption does not update speakers
  automatically. Atom's display profile targets GC9107; ST7735 revisions remain
  unqualified. The screen is optional to the reusable core and phone enrollment
  offers manual network/address entry without QR scanning.
- [MP3Quran streaming](docs/development/quran-streaming.md), published in v0.6.0,
  plays one surah at a time; Athan interrupts without automatic resume.
  [Dua after Athan](docs/development/dua.md), published in v0.8.0, is off by default
  and uses an installed offline recording. Reuse physical evidence only within
  its recorded source and scope; [release notes](docs/releases/v0.8.0.md) link to
  qualification results and remaining limits. Broader adhkar, touch adapters,
  expanded storage and Home Assistant remain future scope in the
  [roadmap](docs/development/roadmap.md).
- Software, documentation, and original hardware designs retain their respective
  [Apache-2.0](LICENSE), [CC BY 4.0](docs/LICENSE.md), and
  [CERN-OHL-P-2.0](hardware/LICENSE.md) licensing boundaries. Third-party recordings
  require compatible redistribution rights before bundling.
- Open decisions: supported launch languages, Arabic/right-to-left support,
  and a formal accessibility conformance target have not been established.
  The current local interface is in English.

## Brand Commitments

The product name is **OpenAthan**. Use **Voice Pyramid** as the project-facing
name for A167, while explaining that M5Stack documents that SKU as **Echo Pyramid**
when needed for hardware identification. OpenAthan is an independent community
project; references to third-party hardware, software, and content providers do
not imply affiliation.

## Evidence on Hand

- [README](README.md): intended user paths, product scope, reference hardware,
  and project status.
- [Architecture](docs/development/architecture.md): standalone operation,
  capability boundaries, settings ownership, and repository responsibilities.
- [Device UI guide](web/device-ui/README.md) and its adjacent implementation:
  existing local interactions, copy, and browser-test instructions.
- [Provisioning contract](firmware/esphome/provisioning/README.md): setup,
  authentication, activation, recovery, and local API behavior.
- [Capacity and validation report](docs/development/feasibility-report.md): dated
  automated and physical evidence, with the tested scope and remaining limits.
  A passing build or bounded hardware check does not establish release readiness.

Keep credentials, private diagnostic archives, recovery images, and personal
device configuration outside the repository. Do not turn untested capabilities
or unqualified recordings into product claims.

## Product Principles

1. Keep the household's core Athan experience local and standalone.
2. Make setup and everyday controls understandable without development knowledge.
3. Preserve settings and prayer history across changes, interruption, and recovery.
4. Treat hardware as replaceable capabilities, with audio as the essential one.
5. Make capability and readiness claims only to the extent supported by evidence.
