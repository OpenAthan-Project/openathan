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
