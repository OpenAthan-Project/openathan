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
- optional prayer-light policy and independent light preferences;
- complete settings validation, versioned encoding and revision-checked saves;
- consumed-prayer and skip state independent of saved settings;
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

These product-distribution concerns belong in official firmware entry points
and their product packages, not in the generic OpenAthan package.
`packages/product.yaml` now shares provisioning, the local UI, clock, stored-audio
player and scheduler composition between the reference and isolated Waveshare V2
entry points. The Waveshare adapters own codec/amplifier power and panel/backlight
wiring; the reusable prayer engine does not acquire board dependencies.
See the [Waveshare development profile](waveshare-box-v2.md) for the tested board,
build entry points and separate public-release restrictions.

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

The current component accepts `time_id`, explicit initial prayer settings and a
`playback_id` implementing the portable `Playback` capability, including volume
request/readback. See the [scheduler guide](../../firmware/esphome/scheduler/README.md#controls-and-reuse).
Optional lights now use `light_output_id` and the portable `LightOutput` capability;
see the [light contract](lights.md). Light preferences have independent persistence
and revisions, and light failures never gate the scheduler.
The reference development candidate has an optional GC9107 AtomS3R status
display: a read-only presenter consumes existing scheduler/settings state,
while the C126 board layer owns SPI and its internal-bus backlight adapter.
Optional saved brightness uses `display_output_id` and the portable
`DisplayOutput` backlight capability, with independent persistence and revisions.
Backlight failures never gate scheduling or update health.
See the [display contract](../../firmware/esphome/components/openathan_display/README.md)
for its compatibility and acceptance limits. Broader display and control mappings remain planned;
`firmware/esphome/examples/custom-hardware.yaml` illustrates that future mapping
and is not a buildable release configuration.

### Settings and consumption

`DeviceSettings` and `SettingsService` own complete snapshots and revision checks.
The ESP32 adapter stores one checksummed settings blob at `openathan/settings`;
the existing `openathan/scheduler` consumption/skip record remains separate.
Successful saves commit before application or acknowledgment. A write failure
latches settings writes and automatic playback off for that boot without erasing
history or stopping current audio.

The bridge preflights schedule changes when time is valid, then rearms future
events without catch-up. Volume-only changes retain the armed schedule. Actual
volume readback gates new playback while dropped commands are retried. Stored
timezone rules affect OpenAthan's civil-date conversion without changing UTC or
other components' timezones. See the [settings contract](../../firmware/esphome/scheduler/SETTINGS.md).

The optional encrypted developer actions and reference local HTTP API are
transports for this service, sharing validation, revision, application-status
and persistence behavior. Prayer-settings edits must go
through this service, preserving consumption and reading back an uncertain save
before any retry.

### Device UI

The reference firmware embeds a responsive UI at a unique
`openathan-<suffix>.local` hostname, with an IPv4 fallback. The product layer
provides USB Wi-Fi/password provisioning, protected local access, and an explicit
setup gate. New devices persist an incomplete marker before settings seeding;
until activation they neither announce nor consume prayers. Recognized existing
settings migrate as configured. Generic/developer builds retain their existing
initialization behavior. HTTP work reaches settings and scheduling only on the
ESPHome main loop. See the [provisioning contract](../../firmware/esphome/provisioning/README.md)
for serial ownership, authentication, persistence, and hardware acceptance limits.

The generic ESPHome web UI may be useful during development but is not the intended permanent product interface.

### Upgrade qualification boundary

The production updater remains the single update engine. The separately selected
`openathan_upgrade_qualification` component owns private-feed trust, test
identity, diagnostic health exceptions, holds, counters, baseline initialization
and forced startup failure. Its C++ code refuses compilation without both
qualification and isolated-storage flags. Small conditional hooks connect it to
the engine; production builds have no qualification member, controls or component
dependency. Adjustments needed for repeated test holds are confined to that
component, preserving production reconciliation and persistence behavior.

Capacity checks inspect generated definitions, copied/compiled sources and actual
application payloads. Release validation rejects qualification and isolated
provisioning images; its defaults never enable the capacity check's test-image
exceptions. A discovered product defect requires a focused production fix and
regression coverage. Passing qualification-tool CI does not establish physical
acceptance or release readiness.

## Standalone operation

Core Athan scheduling must not require Home Assistant or a cloud service. Internet may be used for initial/periodic time sync, Quran streaming, firmware downloads, or optional services.

The generic ESPHome package does not configure Wi-Fi. Reference firmware provides USB provisioning, while ESPHome integrators keep their existing network configuration. The current reference build needs internet time synchronization after a cold boot; saved settings cannot establish the current time. With a valid clock, prayer calculation and stored-recording playback remain local.

## Public website boundary

The public website and browser installer live in the separate [`OpenAthan-Project/website`](https://github.com/OpenAthan-Project/website) repository. Firmware builds and release artifacts originate in this repository; the website consumes published artifacts and does not compile firmware.

The device UI remains in this repository and must function without the public website or cloud infrastructure. The deployed website can suggest a location and adopt approved stable releases after verification; neither is required for local controls or daily operation. Automatic website adoption does not request a speaker update: owners queue signed application updates through the device's Firmware section.

## Future migration

If ESPHome later limits audio, storage, UI, or performance requirements, the OpenAthan core and device-facing APIs should be reusable from native ESP-IDF firmware.
