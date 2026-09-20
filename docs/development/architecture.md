# Architecture

## Direction

OpenAthan starts with ESPHome as an embedded framework, not as a Home Assistant dependency.

The architecture should keep OpenAthan-specific logic separable from framework-specific plumbing:

```text
openathan-core
    ↑
ESPHome adapter ── hardware adapters
    ↑
local API / device UI
```

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

### ESPHome layer

Responsible for:

- ESP32 platform integration;
- Wi-Fi provisioning;
- OTA;
- time synchronization;
- networking;
- filesystem/storage plumbing;
- audio pipeline integration;
- exposing OpenAthan services/state.

### Voice Pyramid adapter

Responsible for hardware-specific details such as:

- audio codec / I2S configuration;
- RGB LEDs;
- capacitive touch controls;
- AtomS3R display;
- hardware-specific power and initialization behavior.

### Device UI

A purpose-built OpenAthan web UI served locally by the device, targeted at `openathan.local`.

The generic ESPHome web UI may be useful during development but is not the intended permanent product interface.

## Standalone operation

Core Athan scheduling must not require Home Assistant or a cloud service. Internet may be used for initial/periodic time sync, Quran streaming, firmware downloads, or optional services.

## Public website boundary

The public website and browser installer live in the separate [`OpenAthan-Project/website`](https://github.com/OpenAthan-Project/website) repository. Firmware builds and release artifacts originate in this repository; the website consumes published artifacts and does not compile firmware.

The device UI remains in this repository and must function without the public website or cloud infrastructure.

## Future migration

If ESPHome later limits audio, storage, UI, or performance requirements, the OpenAthan core and device-facing APIs should be reusable from native ESP-IDF firmware.
