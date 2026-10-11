# Waveshare V2 RTC implementation and powered RESET validation — 2026-10-10

The isolated Waveshare profiles include startup restoration from a previously
initialized PCF85063A and verified UTC writes after SNTP synchronization.
The [profile](waveshare-box-v2.md#rtc-and-offline-restart) records the protocol,
source references and fallback behavior. Attended offline RESET restoration
passed on one V2 unit with RTC power retained. Battery-backed retention while
unplugged, drift and audible scheduled playback using RTC-only time remain
unqualified. The [v0.5.0 release report](release-validation-2026-10-10.md) records
reuse of this source-bound evidence in the published production profile.

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

## Attended powered RESET validation

Source `1378dc2f043adaae108059a000e7e54b120f6d3c` includes the network-synchronization
guard. The verified V2 board and pinned vendor source identify the PCF85063A;
registers at `0x51` on GPIO10/11 matched its layout and advanced. An address
response alone was not treated as chip identification.

Genuine internet synchronization initialized the RTC and verified readback.
Private test controls then blocked network time while retaining local Wi-Fi and
USB power. The owner briefly pressed physical RESET and confirmed the correct
local clock. A fresh boot reported `rtc_restore=restored`, with network time still
disabled and automatic readiness recovered without SNTP. Fresh clock samples
differed from the host by at most 1.508 seconds; cached responses were excluded.
The earlier USB-JTAG restart retained system time and was not counted as RTC
restoration.

Re-enabling genuine network time verified RTC refresh without replay or changes
to settings, history, skip, setup or display preferences. Fresh paired reads and
independent complete-flash readback preserved saved records and shared audio
through the application-only test installations. The final clean development
application was 1,262,784 bytes, SHA-256
`5fb0e90263f6dcaf380da6809d86b3ce59e3531bc87f80b2811b0665333571a3`;
normal SNTP was enabled and the temporary diagnostic controls were excluded.
These are dated development results, not the later production application's identity.

## Remaining limits

No scheduled prayer was due during the RTC test, so audible playback using
RTC-only time and restart with a nonempty saved skip remain untested. The test
did not remove RTC power, inspect a backup battery, measure long-term drift or
inject destructive hardware faults. Idle diagnostic measurements do not establish
clean-production concurrent audio/network headroom or a new long soak. The
earlier intermittent HTTP issue remains unresolved.

Further hardware work follows the
[preservation runbook](../../firmware/esphome/provisioning/HARDWARE_TEST.md).
Historical NVS restoration is not a way to repeat a consumed occurrence.
