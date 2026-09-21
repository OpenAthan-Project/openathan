# OpenAthan

OpenAthan is an open-source, low-cost DIY Athan and Quran smart speaker project. It supports an ordinary-user reference build and a reusable ESPHome integration for people who already have compatible hardware. The reference experience is intended to work without soldering, PCB design, Home Assistant, or embedded-development experience.

> **Project status:** early development. The files in this repository are an initial architecture scaffold, not installable or production-ready firmware.

## Supported paths

### Ordinary users

1. Buy the documented reference hardware.
2. Plug the pre-assembled modules together.
3. Connect the device over USB.
4. Visit [openathan.com/install](https://openathan.com/install).
5. Install OpenAthan and enter Wi-Fi credentials.
6. Complete the phone-friendly local setup wizard.
7. Use the device standalone.

Stable reference releases will be distributed as precompiled firmware through [openathan.com/install](https://openathan.com/install). This path will not require users to write YAML or understand ESPHome.

### ESPHome users

Users with an existing ESPHome configuration will be able to import the reusable OpenAthan package and connect it to their own audio and optional interface components. Their configuration remains responsible for its board, networking, credentials, and any hardware entities it exposes.

The reusable package treats hardware as capabilities. An audio playback endpoint is fundamental to Athan playback; RGB LEDs, a display, physical or touch controls, a microphone, RTC, removable storage, and Home Assistant integration are optional. See the [ESPHome firmware documentation](firmware/esphome/README.md) and [custom-hardware example](firmware/esphome/examples/custom-hardware.yaml).

Home Assistant may eventually be supported as an optional integration on either path, but it will not be required for core operation.

## Initial reference hardware

The first official precompiled reference build targets:

- **M5Stack AtomS3R C126** — ESP32-S3, 8 MB flash, 8 MB PSRAM, and a 0.85-inch display.
- **M5Stack Voice Pyramid A167** — speaker and audio hardware, microphone, RGB LEDs, capacitive touch controls, and enclosure.

M5Stack's current official documentation names SKU A167 **Echo Pyramid**. OpenAthan uses **Voice Pyramid** as its project-facing name for the same A167 product. See the [AtomS3R documentation](https://docs.m5stack.com/en/core/AtomS3R), [A167 documentation](https://docs.m5stack.com/en/atom/Echo_Pyramid), and [reference bill of materials](hardware/reference-builds/voice-pyramid/bom.md).

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
- Firmware release artifacts will be built and published from this repository. The website installer will consume those artifacts and will not compile firmware.
- The device UI remains here because its local API and firmware behavior evolve together. It must work without the public website or cloud infrastructure.

```text
lib/openathan-core/       Framework-independent product logic
firmware/esphome/         Reusable ESPHome package, integrations, examples, and reference entry point
web/device-ui/            Device-local OpenAthan interface
hardware/                 Reference builds and optional expansions
docs/development/         Architecture and engineering documentation
```

## Planned functionality

The standalone Athan MVP is planned to include local prayer-time calculation, calculation and Asr methods, prayer offsets, normal and Fajr Athan playback, volume control, skip-next-Athan behavior, persisted settings, time synchronization, local configuration, and OTA updates. The official reference build may add prayer-status LEDs, touch controls, and its display through optional capability adapters; those features are not requirements of the reusable scheduler or package.

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
