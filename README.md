# OpenAthan

OpenAthan is an open-source, low-cost DIY Athan and Quran smart speaker project. It supports an ordinary-user reference build and a reusable ESPHome integration for people who already have compatible hardware. The reference experience is intended to work without soldering, PCB design, Home Assistant, or embedded-development experience.

> **Project status:** pre-1.0 reference releases are available. [v0.3.0](https://github.com/OpenAthan-Project/openathan/releases/tag/v0.3.0) adds a dim clock, next-prayer and playback display to standalone scheduled Athan, phone-friendly setup and settings, USB Wi-Fi/password recovery, optional prayer lights and owner-requested firmware updates for AtomS3R C126 + Pyramid A167. See the [current release evidence and limits](docs/development/release-validation-2026-10-04.md); newer ST7735 display revisions, broader hardware/network coverage and long-term stability remain unqualified.

Use the [public setup guide](https://openathan.com/docs/getting-started/) for a
new speaker and the [firmware update guide](docs/development/firmware-upgrades.md)
for an existing one. Fresh installation erases existing settings, credentials
and prayer history. Compatible application updates and USB credential recovery
preserve them and the stored recordings.

The reference firmware includes [local setup and settings](firmware/esphome/provisioning/README.md).
The encrypted [developer settings console](firmware/esphome/scheduler/SETTINGS.md)
is an optional development tool. [Release tooling](docs/development/releases.md)
builds exact source commits and packages the [approved normal and Fajr recordings](AUDIO-LICENSES.md).

## Supported paths

### Ordinary users

1. Buy the documented reference hardware.
2. Plug the pre-assembled modules together.
3. Connect the device over USB.
4. Visit [openathan.com/install](https://openathan.com/install).
5. Install OpenAthan and enter Wi-Fi credentials.
6. Complete the phone-friendly local setup wizard.
7. Use the device standalone.

Precompiled reference releases are available through [openathan.com/install](https://openathan.com/install). This path requires neither YAML nor ESPHome knowledge. Use only the Atom USB-C data connection during setup, then disconnect it and use only Pyramid bottom power for normal playback. After a cold boot, the current reference firmware needs internet time synchronization before automatic playback can resume.

### ESPHome users

ESPHome contributors can compose the reusable OpenAthan package from a local checkout and connect it to compatible audio and optional interface components. Their configuration remains responsible for its board, networking, credentials, and any hardware entities it exposes. Remote package consumption and additional hardware require validation; see the [integration guide](firmware/esphome/README.md).

The reusable package treats hardware as capabilities. An audio playback endpoint is fundamental to Athan playback; RGB LEDs, a display, physical or touch controls, a microphone, RTC, removable storage, and Home Assistant integration are optional. See the [ESPHome firmware documentation](firmware/esphome/README.md) and [custom-hardware example](firmware/esphome/examples/custom-hardware.yaml).

Home Assistant may eventually be supported as an optional integration on either path, but it will not be required for core operation.

## Initial reference hardware

The first official precompiled reference build targets:

- **M5Stack AtomS3R C126** — ESP32-S3, 8 MB flash, 8 MB PSRAM, and a 0.85-inch display.
- **M5Stack Voice Pyramid A167** — speaker and audio hardware, microphone, RGB LEDs, capacitive touch controls, and enclosure.

SKU A167 has also been called **Echo Pyramid**; verify the SKU when purchasing. See the [AtomS3R documentation](https://docs.m5stack.com/en/core/AtomS3R), [Voice Pyramid documentation](https://docs.m5stack.com/en/atom/Voice_Pyramid), and [reference bill of materials](hardware/reference-builds/voice-pyramid/bom.md).

This C126 + A167 combination is the designated reference target, not a requirement of the reusable OpenAthan package. RTC, removable storage, larger external speakers, and other modules are optional capabilities rather than core requirements.

## Architecture

```text
OpenAthan
├── openathan-core
│   ├── prayer calculations
│   ├── scheduling
│   ├── settings
│   └── playback abstractions
├── reusable ESPHome integration
│   ├── hardware-independent OpenAthan component
│   └── required and optional capability interfaces
├── hardware integrations
│   ├── Voice Pyramid A167 adapter
│   └── user-supplied ESPHome components
├── official reference firmware
│   ├── validated AtomS3R C126 board configuration
│   ├── C126 + A167 capability mapping
│   └── provisioning, networking, OTA, and release configuration
└── local OpenAthan device UI
```

OpenAthan-specific behavior belongs in modular C++ components rather than one large YAML configuration. `openathan-core` and the prayer scheduler must depend on capability interfaces, never on Voice Pyramid entities or GPIOs. They should remain independent from ESPHome, Home Assistant, and the reference hardware wherever practical so they can later move to native ESP-IDF or another framework if necessary.

See [the development architecture](docs/development/architecture.md) for responsibility boundaries.

## Repository responsibilities

- This repository contains firmware, `openathan-core`, the device-local UI, hardware reference information, and development documentation.
- [`OpenAthan-Project/website`](https://github.com/OpenAthan-Project/website) contains the public website, public-facing documentation, and browser installer.
- Firmware release artifacts are built and published from this repository. The website installer verifies and consumes published artifacts; it never compiles firmware. Its approved-release adoption is separate from a speaker owner's request to install an update.
- The device UI remains here because its local API and firmware behavior evolve together. It must work without the public website or cloud infrastructure.

```text
lib/openathan-core/       Framework-independent product logic
firmware/esphome/         Reusable ESPHome package, integrations, examples, and reference entry point
web/device-ui/            Device-local OpenAthan interface
hardware/                 Reference builds and optional expansions
docs/development/         Architecture and engineering documentation
```

## Available functionality and next milestones

Reference firmware implements local prayer-time calculation, calculation and Asr
methods, prayer offsets, scheduled normal/Fajr playback, durable skip and replay
protection, saved volume and settings, time synchronization, and application OTA.
Settings can be changed through the authenticated local interface.

The reference build includes phone-friendly setup, local configuration,
USB Wi-Fi/password recovery, explicit first-run activation, prayer-status LEDs
and signed application updates. The [roadmap](docs/development/roadmap.md)
separates shipped features from remaining work and validation limits. Touch
and display controls remain future optional adapters; neither is required by
the reusable scheduler.

Quran streaming, reciter and passage selection, resume position, adhkar, offline Quran storage, RTC expansion, and optional Home Assistant integration are later work. Streaming audio should use PSRAM for buffering; Quran audio is not expected to fit in the AtomS3R's flash.

## Development safety

- Do not add hardware pin mappings until they have been verified against official M5Stack documentation or source and physical hardware.
- Do not present scaffold files as installable firmware.
- Do not commit Wi-Fi credentials or signing secrets.
- Do not bundle third-party Athan or Quran recordings without explicit, compatible redistribution rights.

See [CONTRIBUTING.md](CONTRIBUTING.md) before proposing substantial changes.

## Licensing

- OpenAthan-authored software: [Apache License 2.0](LICENSE).
- OpenAthan-authored documentation: [CC BY 4.0](docs/LICENSE.md).
- Future original hardware design sources: [CERN-OHL-P-2.0](hardware/LICENSE.md), unless stated otherwise.
- Third-party dependencies, hardware, and media remain under their respective terms.

OpenAthan is an independent community project and is not affiliated with the third-party hardware, software, or content providers referenced in its documentation.
