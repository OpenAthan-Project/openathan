# Prayer Calculation Requirements

OpenAthan should calculate prayer times locally on the device.

The implementation should support at minimum:

- configurable latitude/longitude or location-derived coordinates;
- timezone and daylight-saving handling;
- multiple established calculation methods;
- Standard and Hanafi Asr methods;
- configurable high-latitude handling where required;
- per-prayer minute offsets;
- Fajr, sunrise, Dhuhr, Asr, Maghrib, and Isha times;
- independent enable/disable controls for Athan playback by prayer.

The chosen calculation library must have a license compatible with the project and must be testable against known reference values before a stable release.

## Implemented behavior

Adhan C++ v1.0.2 is integrated with twelve presets, Standard/Hanafi Asr, three
high-latitude rules, and bounded minute offsets. The standalone scheduler adds
explicit timezone handling, five enable flags, durable duplicate prevention,
skip/cancel and stop controls. See the [scheduler guide](../../firmware/esphome/scheduler/README.md)
for the developer configuration, exact timing/persistence rules and tests.
Reference-device validation includes synthetic scheduling, persistent controls,
offline playback and real scheduled prayer observations. The released reference
firmware includes local setup/settings and timetable previews. See the
[dated device results](feasibility-report.md) and [current release evidence](release-validation-2026-10-03.md)
for the tested images and remaining coverage limits; implementation and host
reference values do not establish every location/timezone on physical hardware.
