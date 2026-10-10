# Waveshare V2 RTC implementation — 2026-10-10

The isolated Waveshare profiles include startup restoration from a previously
initialized PCF85063A and verified UTC writes after SNTP synchronization.
The [profile](waveshare-box-v2.md#rtc-and-offline-restart) records the protocol,
source references and fallback behavior. This is development implementation;
offline restart and RTC operation on physical hardware remain unqualified.

## Automated validation

Host regressions exercise the production RTC adapter against a register-level
bus fixture: initialization markers, strict BCD/calendar/weekday validation,
oscillator loss, stopped/test/12-hour modes, missing devices, startup time races,
partial writes, readback failure, UTC/year limits and restart restoration.
Offset, alarm, timer and unrelated control-register preservation are checked.
The RTC/scheduler handoff regression restores readiness and an upcoming occurrence,
rejects catch-up/replay after corrections, preserves a skip through reset and
resumes scheduling on the next date. The pinned timezone bridge checks saved
timezone/DST conversion separately.

- All 18 C++ CTests passed with UndefinedBehaviorSanitizer.
- All 106 Python tests passed, including the production/isolated settings bridge
  against the resolved ArduinoJson library and pinned ESPHome timezone code.
- All 12 firmware configurations passed schema validation. Generated configuration
  checks connect RTC to the selected SNTP source and shared bus with polling disabled.
- Three paired Waveshare builds passed dependency pins, partition/image-header,
  factory/OTA consistency, shared-audio and capacity inspection.

### Network synchronization guard

ESPHome 2026.9.0 can emit its first SNTP callback from already valid system time,
including RTC-restored time without a network response. RTC writes now require
ESP-IDF's completed network synchronization status. Reading that status consumes
completion, so inferred startup and duplicate notifications leave RTC registers
and save diagnostics untouched. This adapter is the sole status consumer in the
pinned profiles, which use immediate SNTP synchronization.

The new regression compiles the pinned production SNTP implementation with the
RTC adapter. It fails against the original PR head `6abeafb` and passes with the
guard: the inferred startup callback performs no RTC bus access, injected write
failures cannot damage retained time, and another offline restart still restores it. Genuine
updates before/after the first loop, deferred/duplicate notifications, incomplete
synchronization and recovery after a genuine interrupted write are also covered.

## Firmware capacity

Baseline main was `681687f1b33e3fefda9202b3ed903fe1e023f39b`. The initial RTC
builds and synchronization-guard follow-up used identical public compile-only
configuration/fixtures, ESPHome 2026.9.0,
ESP-IDF 5.5.5 and the pinned Xtensa toolchain. These synthetic fixtures must never
be installed or played.

| Variant | Baseline OTA bytes | RTC OTA bytes | Delta bytes | Remaining application budget bytes |
| --- | ---: | ---: | ---: | ---: |
| Waveshare audio only | 1,210,736 | 1,212,816 | +2,080 | 360,048 |
| Waveshare round display | 1,260,752 | 1,262,784 | +2,032 | 310,080 |
| Waveshare encrypted diagnostics | 1,347,360 | 1,349,712 | +2,352 | 223,152 |

The guard was separately measured against original PR head `6abeafb`, using
before/after builds in the same directories and configurations. It adds 64 bytes
to audio-only and 80 bytes to display/diagnostics, with zero static RAM growth.
All six guard-comparison builds passed capacity inspection; the table above
includes this correction in the total RTC cost relative to baseline main.

Static RAM grew by 40 bytes for audio/diagnostics and 48 bytes for display. This
does not establish runtime heap, fragmentation or PSRAM headroom. The application
limit remains 1,572,864 bytes; both 2 MiB slots and separate shared audio are
unchanged. Atom profiles do not include the RTC adapter; no new Atom build or
browser/hardware acceptance is claimed by these measurements.

## Pending attended acceptance

1. Follow the [preservation runbook](../../firmware/esphome/provisioning/HARDWARE_TEST.md).
   Reidentify the V2 unit, retain current settings/history/audio and compatible
   application recovery. Verify the RTC model, `0x51` responder and register layout
   on GPIO10/11 before allowing the first RTC write; an address response alone
   does not identify the chip. Stop if the hardware differs from the vendor source.
2. Prepare a reviewed application-only candidate with real approved recordings,
   isolated storage and disabled public updates. Retain candidate/source hashes,
   baseline RTC bytes and the preservation/readback procedure before installation.
3. With internet available, verify `rtc_save=verified`. Confirm saved settings,
   consumption history, skip and shared audio remain intact.
4. Block internet time access while retaining local control connectivity and RTC
   power. Press RESET; verify `rtc_restore=restored`, correct UTC/local display and
   automatic readiness before any network time synchronization.
5. Observe one upcoming scheduled prayer, audible playback and exactly one
   consumption advance while offline. Reconnect internet and verify RTC refresh
   without replay. Exercise saved-skip persistence in an agreed isolated scenario.
6. Record memory/fragmentation/PSRAM during the focused diagnostic run. Restore the
   reviewed clean candidate, confirm preserved state and record clean-image limits.

Do not inject destructive RTC faults on the attended unit or restore historical
NVS to repeat an occurrence. Backup-power removal, battery retention, drift,
public release/update qualification and the intermittent HTTP defect are separate
work. No physical result or runtime-headroom claim is established by host tests.
