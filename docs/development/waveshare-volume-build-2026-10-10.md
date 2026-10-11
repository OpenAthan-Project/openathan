# Waveshare volume build and attended validation — 2026-10-10

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

## Attended physical validation

One Box V2 unit with 16 MiB flash and 8 MiB PSRAM was tested with approved real
recordings and the isolated diagnostic application from source
`264b4f734bc9dbb3499bc36caa8dab30d8ab04b8`, whose tree matches merged revision
`96670c1e30a9d4c16ca522ac976f41682acb3ccd`. These observations apply to the
new volume mapping on that development source.

| Listening check | Owner-confirmed result |
| --- | --- |
| Normal Athan at 20% | Audible and clear |
| Live increase to 50% | Louder and clear |
| Normal Athan at 80% | Good volume and clear compared with before the fix |
| Live increase to 100% | Clear, no distortion |
| Live mute to zero while playback remained active | Completely silent |
| Complete Fajr recording at 80% after cold restart | Clear throughout, including the Fajr-specific wording |

Normal playback used bounded 60- and 180-second samples. Stop returned the player
to idle. The full Fajr recording completed naturally before its watchdog Stop
command. After all power was disconnected for ten seconds, a fresh power-on boot
returned idle at the saved 80%; settings, revision, prayer history and preferences
matched the fresh pre-cut baseline. Clock synchronization returned through SNTP;
this does not establish RTC retention after power loss.

Across the audio windows, 40 diagnostic telemetry samples and 52 authenticated
status polls recorded no codec/display faults, unexpected resets, scheduler faults
or failed status requests. Minimum sampled internal free/largest block was
215,564 / 172,032 bytes; minimum sampled free PSRAM was 6,972,840 bytes. Two
`openathan_device` operation warnings (64 ms and 51 ms) occurred without a failed
request or playback error. These memory measurements cover the diagnostic image.

## Clean development installation and limits

Fresh matching full-flash reads, live identity/partition/security checks, zero
NVS integrity errors and the approved shared-audio hash preceded an application-only
clean installation using the [hardware runbook](../../firmware/esphome/provisioning/HARDWARE_TEST.md).
Complete independent readback verified the candidate and every byte outside its
sector erase extent. The retained application identities are:

| Application | OTA bytes | SHA-256 |
| --- | ---: | --- |
| Attended diagnostics | 1,349,792 | `9cde040ee8fec0231f5e0708a679591e74752a0961796d469e547ce8bc513c42` |
| Final clean development | 1,262,864 | `3b9fd7bddeecb27b1d876bf922b69243812b5bcce88421ffef90567ae4891825` |

The final clean application booted ready and idle with the original 80% settings
restored, settings/history/preferences and shared audio preserved, and the native
diagnostic API closed. Fresh compatible application recovery and private evidence
remain outside Git. Historical full-flash restoration would also restore old
settings/history and is not routine recovery.

The complete normal recording, physical stop button, real scheduled announcement
and a new long soak were not repeated in this focused volume session. Clean-image
runtime heap/fragmentation/PSRAM remains unmeasured. Earlier button and scheduled
playback evidence remains separate, as do unresolved intermittent HTTP timeouts.
These results establish focused development acceptance; they do not qualify the
later v0.5.0 production storage, signed updates or recovery. Those separate
checks are recorded in the [release qualification report](release-validation-2026-10-10.md),
which reuses this listening evidence within its tested source and scope.
