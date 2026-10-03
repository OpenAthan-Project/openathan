# ESPHome Firmware

ESPHome is the preferred initial embedded framework for OpenAthan.

> **Status:** precompiled reference releases provide USB setup/recovery, local settings, scheduled offline Athan, optional prayer lights and signed application updates. See the [current release evidence and limits](../../docs/development/release-validation-2026-10-03.md). The [scheduler development build](scheduler/README.md) and [manual feasibility build](feasibility/README.md) remain separate development configurations.

Runtime configuration uses the optional encrypted [settings console](scheduler/SETTINGS.md)
or the reference firmware's [device-hosted settings interface](provisioning/README.md).
The latter adds USB Wi-Fi/password setup and recovery, local access protection,
and explicit activation while retaining the settings service and prayer history.

## Two supported paths

### Precompiled reference firmware

Ordinary users install official releases from [openathan.com/install](https://openathan.com/install) without YAML or ESPHome knowledge. [`openathan.yaml`](openathan.yaml) is the release entry point and composes the reusable OpenAthan package with the C126 + A167 reference-hardware package. Official-only provisioning, networking, OTA, and release settings belong at that entry-point layer, not in the generic package. Fresh installation erases saved data; existing speakers use the [preserving upgrade path](../../docs/development/firmware-upgrades.md).

### Reusable ESPHome integration

ESPHome contributors can compose [`packages/openathan.yaml`](packages/openathan.yaml) from a local checkout and bind OpenAthan to compatible components. The generic package must not select a board, define GPIOs, configure Wi-Fi credentials, require the native API, or assume Home Assistant.

Remote package consumption remains unvalidated. Its intended configuration shape is:

```yaml
packages:
  openathan:
    url: https://github.com/OpenAthan-Project/openathan
    ref: v1.0.0
    files:
      - firmware/esphome/packages/openathan.yaml
```

The tag above is illustrative and does not exist. Current builds require a local
checkout so the external components can locate the sibling core and vendored
library. Remote package consumption is not validated for this milestone. See
[`examples/custom-hardware.yaml`](examples/custom-hardware.yaml) for the planned
capability-mapping boundary.

## Capability contract

Audio playback is the only fundamental hardware capability for Athan playback.
The portable `Playback` interface supplies readiness, activity, start with
replacement, and stop. The current adapter uses an ESPHome speaker media player
and a separately provisioned audio partition. Adapters used by the settings
bridge also implement volume requests and actual volume readback. Networking
and the encrypted developer API remain optional to the reusable component.

The following capabilities remain optional:

- RGB or other status lighting;
- display output;
- physical, capacitive-touch, or other controls;
- microphone input;
- RTC;
- removable storage;
- Home Assistant or another external automation system.

The scheduler and hardware-independent OpenAthan component may request playback or publish state, but must never manipulate Voice Pyramid hardware directly. Hardware-specific behavior belongs behind a capability adapter.

## Directory responsibilities

- [`components/openathan/`](components/openathan/) — hardware-independent OpenAthan behavior and ESPHome/core integration.
- [`components/voice_pyramid/`](components/voice_pyramid/) — A167-specific integration only.
- [`packages/openathan.yaml`](packages/openathan.yaml) — reusable, hardware-neutral package.
- [`packages/hardware/voice-pyramid.yaml`](packages/hardware/voice-pyramid.yaml) — convenience mapping for the official C126 + A167 combination.
- [`boards/`](boards/) — verified low-level reference-board configuration only.
- [`examples/custom-hardware.yaml`](examples/custom-hardware.yaml) — documented integration shape for user-owned ESPHome hardware.
- [`openathan.yaml`](openathan.yaml) — official reference release entry point.
