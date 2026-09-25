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

## Implemented development behavior

Adhan C++ v1.0.2 is integrated with twelve presets, Standard/Hanafi Asr, three
high-latitude rules, and bounded minute offsets. The standalone scheduler adds
explicit timezone handling, five enable flags, durable duplicate prevention,
skip/cancel and stop controls. See the [scheduler guide](../../firmware/esphome/scheduler/README.md)
for the developer configuration, exact timing/persistence rules and tests.
Reference-device validation covered synthetic scheduling, persistent controls
and offline playback. Actual prayer-time observation, provisioning/settings UI
and release qualification remain.
