# openathan-core

This directory contains framework-independent C++20 OpenAthan logic.

- `calculate_prayers` validates finite coordinates, a civil date (1900–2100),
  and one of 12 calculation presets, then returns six optional UTC timestamps.
  Standard/Hanafi Asr, three explicit high-latitude rules plus `auto`, and
  -120 to +120 minute offsets are supported. Available events within each day
  must remain strictly ordered; the scheduler handles adjacent Isha/Fajr conflicts.
- `validate_audio_image` validates the versioned two-track storage format using
  an injected hash function. It exposes a catalog only after both tracks pass.
- `Scheduler` owns future-event selection, duplicate prevention, durable skip
  state, and playback policy. Clock/local-date conversion, calculations, storage,
  and playback are injected. `scheduler_state` provides a stable checksummed
  record format. See the [scheduler guide](../../firmware/esphome/scheduler/README.md).

`DeviceSettings` and `SettingsService` validate, encode and durably save complete
revision-checked settings snapshots independently of consumption. Platform and
playback adapters apply timezone rules and volume.

The released MIT Adhan C++ dependency is in `third_party/adhan-cpp`, unchanged.
The root CMake project tests the same sources on a host; this directory's
CMake file registers the core as an ESP-IDF component.

Prayer calculation, scheduling, settings and playback policy remain independent
of ESPHome, Home Assistant and any particular hardware board. New setup or UI
transports should use the existing settings service and retain consumption
before playback, including both identities of a shared Isha/Fajr occurrence.

Keeping this layer independent is intentional: it should be reusable if OpenAthan later migrates from ESPHome to native ESP-IDF or another embedded framework.
