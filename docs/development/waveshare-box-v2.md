# Waveshare Box V2 development profile

The ESP32-S3-Touch-LCD-1.85C-BOX **V2** development profile reuses standalone
prayer calculations, scheduling, stored recordings, USB provisioning and the
authenticated device-local phone controls. V1 uses different audio hardware and
is not supported by this profile. Touch, microphones and SD storage are
deferred. Physical acceptance is recorded in the
[dated validation report](waveshare-validation-2026-10-09.md).

## Build and storage

Use the [pinned CI prerequisites](../../.github/workflows/README.md) and
ESPHome 2026.9.0 / ESP-IDF 5.5.5:

```sh
python -m esphome compile firmware/esphome/waveshare/development.yaml
# Comparison without the display:
python -m esphome compile firmware/esphome/waveshare/audio.yaml
```

The chip is configured for 16 MB flash and 8 MB octal PSRAM. The existing
[partition table](../../firmware/esphome/feasibility/partitions.csv) remains in
the lower 8 MiB: two 2 MiB application slots and separate 3.5 MiB shared audio.
Additional flash is unused. Each OTA image must remain at or below 1,572,864
bytes. `check_feasibility.py` recognizes the explicit Waveshare hardware define
and requires 16 MiB image headers, isolated storage and disabled updates.
The public release validator still defaults to 8 MiB reference images and
rejects isolated development firmware.

Wi-Fi power saving is disabled in this board profile following the attended
LIGHT/NONE comparison. HTTP timeouts remain unresolved; see the dated report for
the comparison and its limits. ESPHome documents this reliability tradeoff in
[Wi-Fi power-save mode](https://esphome.io/components/wifi/#power-save-mode).

The [network follow-up](waveshare-network-development-2026-10-09.md) records
the unresolved findings and validated clean development installation.

The hardware identity is `waveshare-esp32-s3-touch-lcd-1_85c-box-v2`. Development
uses `openathan-test` with a MAC suffix and the existing `oa_test`,
`oa_setup_test`, `oa_network_test` and `oa_upgrade_test` namespaces. It does not
load production settings or consumption history.

`firmware.hardware` and `firmware.updates_enabled` are additive status fields.
Existing clients can ignore them; the local UI treats a missing update flag as
enabled for older firmware. Waveshare sets it to false: automatic discovery,
authenticated check/install/cancel requests and USB update writes are rejected.
USB provisioning and healthy application startup confirmation remain enabled.
These builds cannot be installed through the public updater or published as
reference releases. Maintainer installation/recovery uses esptool in an attended
session.

## Hardware adapters

| Function | Mapping |
| --- | --- |
| ES8311 codec | I²C `0x18`; 44.1 kHz, 16-bit mono |
| Audio clocks/output | MCLK 2, BCLK 48, LRCLK 38, DOUT 47 |
| Amplifier power | GPIO15; held low until codec and stored-audio/player initialization succeed |
| ST77916 QSPI | CLK 40, CS 21, data 46/45/42/41 |
| Backlight | GPIO5, 5 kHz PWM, saved 1–100%, default 50% |
| Panel reset | TCA9554 `0x20`, zero-based P1, vendor's one-based EXIO2 |
| Shared I²C | SCL 10, SDA 11; confirmed on the attended unit |
| PCF85063A RTC | I²C `0x51` on the shared bus; RTC physical acceptance pending |
| BOOT button | GPIO0, active-low input with pull-up; stop/skip/cancel observed on the attended V2 unit |

The mappings follow the [manufacturer's V2 audio example](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85C/blob/8ead4a96bf3a278fc4ebd8ef4768657e17fa2880/Arduino/examples/03_audio_out_no_tf/03_audio_out_no_tf.ino),
[display header](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85C/blob/8ead4a96bf3a278fc4ebd8ef4768657e17fa2880/ESP-IDF/ESP32-S3-Touch-LCD-1.85C-Test/main/LCD_Driver/ST77916.h)
and [expander source](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85C/blob/8ead4a96bf3a278fc4ebd8ef4768657e17fa2880/ESP-IDF/ESP32-S3-Touch-LCD-1.85C-Test/main/EXIO/TCA9554PWR.c).
The shared bus follows the current vendor board initialization. The
[linked V2 schematic](https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.85C/ESP32-S3-Touch-LCD-1.85C_V2.pdf)
retains GPIO12/13 expander labels; the diagnostic overlay probes both pairs.
The attended unit confirmed both codec and expander on GPIO10/11 and neither on
GPIO12/13; this result is specific to that unit.

The QSPI driver uses Waveshare's `vendor_specific_init_new` register sequence
from [ST77916.c](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85C/blob/8ead4a96bf3a278fc4ebd8ef4768657e17fa2880/ESP-IDF/ESP32-S3-Touch-LCD-1.85C-Test/main/LCD_Driver/ST77916.c).
The vendor selects this sequence for one panel ID; other panel lots require
physical validation. ESPHome adds pixel format, orientation, inversion,
sleep-out and display-on commands. Its QSPI path requires a 16-bit framebuffer
(259,200 bytes); allocator placement and recovery must be measured on-device.

### RTC and offline restart

The Waveshare profiles restore UTC once at startup from the onboard PCF85063A
when system time is invalid. After successful SNTP synchronization, the adapter
saves UTC to RTC and verifies readback. It does not periodically resynchronize
system time from RTC. Saved timezone/DST rules remain responsible for local time.
Only completed ESP-IDF network synchronizations permit RTC writes. ESPHome's
inferred startup notification after RTC restoration leaves retained time untouched;
duplicate notifications cannot repeat a completed write.

The register layout and bus follow the
[vendor RTC header](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85C/blob/8ead4a96bf3a278fc4ebd8ef4768657e17fa2880/ESP-IDF/ESP32-S3-Touch-LCD-1.85C-Test/main/PCF85063/PCF85063.h)
and [I²C example](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85C/blob/8ead4a96bf3a278fc4ebd8ef4768657e17fa2880/Arduino/examples/02_RTC_PCF85063/I2C_Driver.h);
validation and STOP sequencing follow the
[PCF85063A datasheet](https://www.nxp.com/docs/en/data-sheet/PCF85063A.pdf).

Register `0x03` holds an OpenAthan initialization marker (`0xA7`), committed only
after verified writes. Factory/vendor values are ignored because their year/time
conventions may differ. The first boot therefore still needs internet time.
RTC dates use years 2000–2099, with the existing valid-clock floor of 2019;
weekday is Sunday=0. Missing markers, oscillator loss, stopped/test/12-hour modes,
invalid BCD/calendar fields and bus errors leave automatic Athan waiting for SNTP.
Writes invalidate the marker first and preserve offset, alarms and timers.
RTC failures do not invalidate synchronized system time or block audio.

Offline restart support requires the RTC to remain powered. Retention after
unplugging, battery behavior and drift are not qualified. Hardware acceptance and
runtime memory checks remain pending; see the
[RTC implementation report and procedure](waveshare-rtc-validation-2026-10-10.md).
Existing activation, skip and consumed-prayer records remain authoritative:
startup and clock corrections do not play missed or consumed occurrences.

### Physical controls

The BOOT button uses the same release-triggered controls as the reference Atom:

| Press duration | Action on release |
| --- | --- |
| 50 ms–1 second | Stop current playback |
| 2–5 seconds | Skip the next scheduled Athan |
| 6–10 seconds | Cancel the skip |
| Other durations | No application action |

These actions use existing scheduler safeguards and do not start manual playback.
BOOT's GPIO0 connection follows the linked V2 schematic; holding it during reset
or power-on still enters ROM download mode. RESET is wired to the chip reset
input and retains its hardware restart function. Attended stop, skip, cancel-skip
and RESET skip-persistence checks passed on one V2 unit, preserving settings,
history and shared audio. Exact timing boundaries, unassigned gaps and the
BOOT-held physical download-mode sequence remain untested. See the
[automated and attended validation](waveshare-buttons-validation-2026-10-09.md)
for source/image identities, capacity and remaining limits.

### Display

The approved [Ring + countdown layout](../../.impeccable/surfaces/firmware-esphome-components-openathan-display-render-h.md)
keeps the existing bitmap text sizes in the 360×360 circle, with a steady 8px
outer ring, a small inline AM/PM suffix beside the prayer time, an hours/minutes
countdown below the time and a lower Offline footer.
It follows every calculated prayer, including muted and skipped announcements,
using the portable LED timetable independently of LED hardware/preferences.
Green means above 30 minutes, orange above 10 through 30, red 10 or less.
Countdown duration uses UTC; displayed times use the saved local timezone.
Setup, clock-waiting, playback and faults keep their existing centered 2× layout
at offset (52,52), without a countdown or proximity ring. The panel remains
rotated 180° for viewing opposite the rear USB cable.
Host tests check every rendered pixel against circular
bounds. Playback guidance says “Button to stop” through the explicit
`stop_button` capability. Saved timezone, time format and
brightness continue to use existing preferences. The presenter refreshes only
when visible content changes. Screen or backlight failure does not gate audio,
scheduling or startup health.
See the [display guide](../../firmware/esphome/components/openathan_display/README.md)
for formatting and state rules, and the
[validation report](waveshare-countdown-validation-2026-10-09.md) for paired
capacity measurements and the attended application installations. Owner
readability confirmation passed for both the first separate-AM/PM layout and
the approved inline-suffix refinement; concurrent
audio/network/display runtime memory remains unmeasured for the clean candidate.

## Attended validation

Follow the preservation principles in the
[provisioning runbook](../../firmware/esphome/provisioning/HARDWARE_TEST.md),
adapting the flash read to **0x1000000** bytes and the actual board power/USB
arrangement. Reidentify the unit, inspect security and installed partitions,
retain two matching private full-flash backups and record the exact restore
command before any first installation. Keep firmware, logs, credentials and
backups outside Git. Never install or play synthetic CI fixtures.

`waveshare/diagnostics.yaml` adds encrypted local telemetry and probes GPIO12/13
alongside the selected GPIO10/11 pair. Set a private `diagnostic_api_key` outside
Git. USB serial remains exclusively owned by provisioning; diagnostic status
is published through the encrypted API every 15 seconds. It includes detected
flash/PSRAM, reset reason, free/minimum internal heap, largest internal block,
free/largest PSRAM block, playback, codec/display failure, application state and
uptime. Explicit diagnostic buttons play normal/Fajr, stop audio, restart, or
show red/green/blue bars for ten seconds. Automatic scheduled playback remains
inactive until setup is explicitly activated.

Confirm bus responders, panel reset, colors/orientation, brightness and PSRAM.
Explicitly play both approved recordings; observe volume/stop, setup persistence,
scheduled playback and replay protection. Bound the audio/network/display run
to 60 minutes and retain memory/error observations. Distinguish physical power
cycling from software restart, and queued playback from audible acceptance.
Prefer compatible application-only recovery after initial partition migration;
a full-flash restore also restores historical settings and prayer consumption.
