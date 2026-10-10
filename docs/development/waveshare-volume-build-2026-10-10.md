# Waveshare volume build checks — 2026-10-10

The Waveshare Box V2 volume change replaces the inherited 60% codec ceiling
with fixed 0 dB codec output and ESPHome software attenuation. Codec gain and
unmute writes must succeed before the amplifier is enabled. Saved settings and
shared audio retain their existing formats; the reference Voice Pyramid mapping
is unchanged.

## Automated validation

All 18 CTests passed with UndefinedBehaviorSanitizer, and all 107 Python tests
passed with the pinned ESPHome environment and resolved ArduinoJson headers.
Adapter tests cover gain/unmute failures, amplifier gating, volume requests and
playback readiness. Generated configuration checks cover all three Waveshare
profiles and the reference profile's existing 60% ceiling and DAC binding.

The before builds use main revision `1378dc2`; the after builds add the volume
change. Both use the same configurations, compile-only fixtures, ESPHome
2026.9.0, ESP-IDF 5.5.5 and pinned compiler. Each variant passed
`tools/check_feasibility.py`, including dependency, partition and image checks.

| Variant | Before OTA bytes | After OTA bytes | Delta | Remaining 1.5 MiB budget |
| --- | ---: | ---: | ---: | ---: |
| Audio | 1,212,816 | 1,212,912 | +96 | 359,952 |
| Display | 1,262,784 | 1,262,864 | +80 | 310,000 |
| Diagnostics | 1,349,712 | 1,349,792 | +80 | 223,072 |

Both 2 MiB application slots and the separate 3.5 MiB shared audio partition
remain unchanged. Compile-only fixture builds must never be installed or played.

## Physical validation pending

These checks do not establish audible loudness, distortion, sustained playback
or runtime heap/PSRAM headroom. Attended testing must use the approved real
recordings and a separately verified application-only candidate, starting at low
volume before testing 50%, 80% and 100%. Verify silence at zero, volume changes,
Stop, concurrent network/display activity and cold-restart persistence.

Reidentify the live unit and preserve settings, prayer history, shared audio and
fresh compatible recovery according to the
[hardware runbook](../../firmware/esphome/provisioning/HARDWARE_TEST.md).
The earlier dated Waveshare listening results cover the previous volume mapping.
