# OpenAthan

OpenAthan is an open-source, low-cost DIY Athan and Quran smart speaker project. It is intended for ordinary users who want a standalone device built from readily available, pre-assembled hardware without soldering, PCB design, Home Assistant, or embedded-development experience.

> **Project status:** early development. The files in this repository are an initial architecture scaffold, not installable or production-ready firmware.

## Intended experience

1. Buy the documented reference hardware.
2. Plug the pre-assembled modules together.
3. Connect the device over USB.
4. Visit [openathan.com/install](https://openathan.com/install).
5. Install OpenAthan and enter Wi-Fi credentials.
6. Complete the phone-friendly local setup wizard.
7. Use the device standalone.

Home Assistant may eventually be supported as an optional integration, but it will not be required for core operation.

## Initial reference hardware

The first reference build targets:

- **M5Stack AtomS3R C126** — ESP32-S3, 8 MB flash, 8 MB PSRAM, and a 0.85-inch display.
- **M5Stack Voice Pyramid A167** — speaker and audio hardware, microphone, RGB LEDs, capacitive touch controls, and enclosure.

M5Stack's current official documentation names SKU A167 **Echo Pyramid**. OpenAthan uses **Voice Pyramid** as its project-facing name for the same A167 product. See the [AtomS3R documentation](https://docs.m5stack.com/en/core/AtomS3R), [A167 documentation](https://docs.m5stack.com/en/atom/Echo_Pyramid), and [reference bill of materials](hardware/reference-builds/voice-pyramid/bom.md).

RTC, microSD, larger external speakers, and other modules are optional future expansions rather than v1 requirements.

## Architecture

```text
OpenAthan
├── openathan-core
│   ├── prayer calculations
│   ├── scheduling
│   ├── settings
│   └── playback abstractions
├── ESPHome integration
│   ├── networking and provisioning
│   ├── OTA and time synchronization
│   └── hardware plumbing
├── Voice Pyramid adapter
│   ├── audio
│   ├── LEDs and touch
│   └── AtomS3R display
└── local OpenAthan device UI
```

OpenAthan-specific behavior belongs in modular C++ components rather than one large YAML configuration. `openathan-core` should remain independent from ESPHome, Home Assistant, and the reference hardware wherever practical so it can later move to native ESP-IDF or another framework if necessary.

See [the development architecture](docs/development/architecture.md) for responsibility boundaries.

## Repository responsibilities

- This repository contains firmware, `openathan-core`, the device-local UI, hardware reference information, and development documentation.
- [`OpenAthan-Project/website`](https://github.com/OpenAthan-Project/website) contains the public website, public-facing documentation, and browser installer.
- Firmware release artifacts will be built and published from this repository. The website installer will consume those artifacts and will not compile firmware.
- The device UI remains here because its local API and firmware behavior evolve together. It must work without the public website or cloud infrastructure.

```text
lib/openathan-core/       Framework-independent product logic
firmware/esphome/         ESPHome integration and board scaffolding
web/device-ui/            Device-local OpenAthan interface
hardware/                 Reference builds and optional expansions
docs/development/         Architecture and engineering documentation
```

## Planned functionality

The standalone Athan MVP is planned to include local prayer-time calculation, calculation and Asr methods, prayer offsets, normal and Fajr Athan playback, prayer-status LEDs, touch playback controls, volume control, skip-next-Athan behavior, persisted settings, Wi-Fi/NTP synchronization, local configuration, and OTA updates.

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

OpenAthan is an independent community project and is not affiliated with M5Stack, ESPHome, Home Assistant, Bilal Speaker, Quran Foundation, or other third parties named in its documentation.
