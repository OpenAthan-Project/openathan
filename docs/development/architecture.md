# Architecture

## Direction

OpenAthan starts with ESPHome as an embedded framework, not as a Home Assistant dependency. It supports both an official precompiled reference build and a reusable package that can be composed with other ESPHome hardware.

The architecture keeps product logic, ESPHome integration, and hardware adaptation separable:

```text
local API / device UI
        │
hardware-independent OpenAthan component
        ├── openathan-core
        └── capability interfaces
              ├── reference-hardware adapters
              └── user-supplied ESPHome adapters
```

The scheduler requests capabilities and receives control events through these interfaces; it never reaches through an adapter to manipulate Voice Pyramid hardware.

## Responsibilities

### openathan-core

Framework-independent C++ logic for:

- prayer-time calculation;
- scheduling;
- prayer offsets;
- calculation and Asr methods;
- playback policy;
- state model;
- future Quran/adhkar scheduling abstractions.

It should not know about ESPHome entities, Home Assistant, or the Voice Pyramid.

### Hardware-independent OpenAthan component

`firmware/esphome/components/openathan/` adapts `openathan-core` to ESPHome and exposes OpenAthan state and actions. It must depend on capability interfaces rather than concrete reference hardware.

Audio playback is a required capability for Athan functionality. Status lights, a display, buttons or touch controls, microphone input, RTC, removable storage, and Home Assistant integration are optional capabilities. Missing optional capabilities must not prevent the scheduler from operating.

### Reusable OpenAthan package

`firmware/esphome/packages/openathan.yaml` packages the hardware-independent integration for use by existing ESPHome configurations. It must not choose a board, own Wi-Fi credentials, require the ESPHome native API, or assume Home Assistant. The consuming configuration supplies its platform, networking, and hardware entities.

### Official firmware layer

Responsible for:

- Wi-Fi provisioning;
- OTA;
- time synchronization;
- networking;
- filesystem/storage plumbing;
- composing the generic OpenAthan and reference-hardware packages;
- exposing OpenAthan services/state.

These product-distribution concerns belong in `firmware/esphome/openathan.yaml` or packages used only by that entry point, not in the generic OpenAthan package.

### Reference-hardware layers

`boards/m5stack-atom-s3r-voice-pyramid.yaml` contains only verified low-level configuration for the C126-based reference build.

`components/voice_pyramid/` is limited to A167-specific integration such as verified audio, microphone, RGB LED, touch, power, and initialization behavior. It does not own the C126 display and contains no prayer or scheduling policy.

`packages/hardware/voice-pyramid.yaml` composes the reference board and A167 integration, then maps their entities onto OpenAthan capabilities. The reusable scheduler and `components/openathan/` never depend on this package.

### Custom hardware

An ESPHome user may provide equivalent capabilities from their existing configuration. For example:

- a media player or speaker-backed adapter for required audio playback;
- any ESPHome light for optional status output;
- buttons, touch sensors, or other inputs for optional controls;
- any compatible display, microphone, RTC, or storage component.

The capability-binding schema has not been implemented yet. `firmware/esphome/examples/custom-hardware.yaml` documents the intended boundary without claiming a working API.

### Device UI

A purpose-built OpenAthan web UI served locally by the device, targeted at `openathan.local`.

The generic ESPHome web UI may be useful during development but is not the intended permanent product interface.

## Standalone operation

Core Athan scheduling must not require Home Assistant or a cloud service. Internet may be used for initial/periodic time sync, Quran streaming, firmware downloads, or optional services.

The generic ESPHome package does not configure Wi-Fi. The official image will provide an ordinary-user provisioning flow, while ESPHome integrators keep their existing network configuration.

## Public website boundary

The public website and browser installer live in the separate [`OpenAthan-Project/website`](https://github.com/OpenAthan-Project/website) repository. Firmware builds and release artifacts originate in this repository; the website consumes published artifacts and does not compile firmware.

The device UI remains in this repository and must function without the public website or cloud infrastructure.

## Future migration

If ESPHome later limits audio, storage, UI, or performance requirements, the OpenAthan core and device-facing APIs should be reusable from native ESP-IDF firmware.
