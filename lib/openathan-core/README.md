# openathan-core

This directory contains framework-independent C++20 OpenAthan logic.

- `calculate_prayers` validates finite coordinates, a civil date (1900–2100),
  and one of 12 calculation presets, then returns six optional UTC timestamps.
  Standard/Hanafi Asr, three high-latitude rules and -120 to +120 minute offsets
  are supported. Available events must remain strictly ordered.
- `validate_audio_image` validates the versioned two-track storage format using
  an injected hash function. It exposes a catalog only after both tracks pass.
- `Scheduler` owns future-event selection, duplicate prevention, durable skip
  state, and playback policy. Clock/local-date conversion, calculations, storage,
  and playback are injected. `scheduler_state` provides a stable checksummed
  record format. See the [scheduler guide](../../firmware/esphome/scheduler/README.md).

The released MIT Adhan C++ dependency is in `third_party/adhan-cpp`, unchanged.
The root CMake project tests the same sources on a host; this directory's
CMake file registers the core as an ESP-IDF component.

The core should contain prayer calculation, scheduling, settings models, and playback policy without depending directly on ESPHome, Home Assistant, or a particular hardware board.

Keeping this layer independent is intentional: it should be reusable if OpenAthan later migrates from ESPHome to native ESP-IDF or another embedded framework.
