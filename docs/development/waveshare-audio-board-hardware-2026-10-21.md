# AUDIO-Board attended hardware checks — October 21, 2026

Later signed-update and recovery evidence is recorded in the
[October 26 report](waveshare-audio-board-upgrades-2026-10-26.md). The observations
below retain their original source and date.

These results apply to one USB-powered ESP32-S3-AUDIO-Board using source
`341c05c5c69dbafdcf8535a0b063ec4f21404563`. Its model, 16 MiB flash, 8 MiB
PSRAM and directly observed I/O match the AUDIO-Board profile. The exact PCB
revision marking is not visible. Chip revision and installed vendor firmware
did not identify that PCB revision. Other units and revisions need their own
hardware preflight.

The original vendor application and complete flash were preserved privately in
matching reads before the explicitly approved first installation. Approved real
normal/Fajr/dua assets came from the published v0.8.0 shared image and matched
the committed registry and signature checks. No synthetic CI audio was installed.
The installed candidate reports v0.8.0 but includes unpublished AUDIO-Board support.
It is not a public release of that support.

## Observed operation

| Check | Observed result |
| --- | --- |
| Peripheral mapping | ES8311 identity, TCA9555 inputs and PCF85063 access matched the documented bus; KEY1/2/3 and BOOT press/release inputs were observed. |
| Seven-pixel wiring | The owner confirmed all seven LEDs red, green and blue during a RAM-only GPIO diagnostic. |
| Phone setup | Temporary setup Wi-Fi, password/home-Wi-Fi wizard and the device's `.local` page worked without a display. |
| Volume keys | KEY3/KEY1 produced 70 → 75 → 70%; a three-second KEY3 hold produced one 20 → 25% step; KEY1 returned it to 20%. |
| Volume limits and persistence | The owner confirmed clamping at 0/100%. Final key-driven 20% survived the application transition; saved settings and phone access remained available after RESET. |
| Production light controls | All seven LEDs responded together to off/on and brightness changes. Software subsequently reported enabled lights at 24% brightness. |
| Production Quran and KEY2 | The owner heard clear Quran at 20%; KEY2 stopped it. |
| Stored normal Athan | One complete attended recording was clear at 20% and finished normally. |
| Stored Fajr and dua | One complete attended Fajr, including the morning phrase, and the automatic dua afterward were clear at 20%. The signed dua bank was already bundled; enabling it did not exercise a new download. |
| Playback lighting | All seven LEDs pulsed blue together during Fajr. |
| BOOT | The clean Quran repeat stopped on a short BOOT press while uptime continued. Three-second skip and seven-second cancel holds set and cleared the next-prayer skip. |
| RTC restart | After a physical RESET, the adapter reported `restored`, a valid clock and a subsequent verified SNTP save. |
| Prayer history | Consumption values remained unchanged through manual playback, RESET and skip/cancel. Real scheduled playback remains a separate pending check. |

The first diagnostic Quran Stop attempt used RESET instead of BOOT, as confirmed
by the owner. Its connection loss and power-on reset are retained as a physical
RESET observation. The subsequent BOOT repeat had continuous uptime and no
reported scheduler fault. Initial diagnostic hostname/IP connection attempts also
timed out; follow-up USB status and HTTP/encrypted API access observed reconnection
without a firmware or credential change. Their exact initial cause is unresolved.

## Images and preservation

The production-profile candidate is **1,373,488 bytes**, leaving **199,376 bytes**
under the application limit and **723,664 bytes** inside its 2 MiB slot. Its
4,320-byte difference from the earlier development-identity measurement comes
from embedding the real commit, which enables the existing production dua
download path. SDK configuration and managed dependency hashes matched.
The [matched before/after measurements](waveshare-audio-board-validation-2026-10-21.md)
remain the comparison for existing-board growth.

A private qualification overlay added encrypted local manual playback and
read-only memory/status collection. Every original source file was unchanged;
the overlay used the production namespaces and disabled public updates. It was
**1,418,912 bytes**, with **153,952 bytes** of application budget remaining:
45,424 additional application bytes and 368 additional static RAM bytes.

The transition to that overlay used fresh matching complete 16 MiB reads and
an application-only write. Independent full-flash readback matched every expected
byte before boot, including unchanged settings, credentials, history, audio,
bootloader, partition table, OTA metadata, inactive slot and unused flash.
Restoration of the regular candidate used another fresh matching current-state
pair and application-only write. All 16 MiB again matched expected changes before
boot. The regular source/hardware identity, active setup, Wi-Fi reconnection and
boot health were confirmed; HTTP returned its expected authentication challenge
after USB closed. The owner then confirmed the phone page reconnected at 20%
with its normal timetable ready. Historical vendor full-flash backups are
emergency restoration only; ordinary OpenAthan recovery must retain newer
settings and history.

## Runtime measurements

Encrypted status was sampled approximately every five seconds while the phone UI,
audio and light output remained active. These are diagnostic-overlay measurements.
The watermark includes shorter allocation peaks between samples.

| Activity | Sampled internal free minimum | Sampled largest internal block minimum | Internal free watermark | Sampled PSRAM free minimum | Maximum fragmentation indicator |
| --- | ---: | ---: | ---: | ---: | ---: |
| Full normal Athan | 219,500 | 176,128 | 213,384 | 7,233,628 | 22.78% |
| Full Fajr and dua | 217,820 | 176,128 | 206,372 | 7,233,000 | 27.08% |
| Quran before owner RESET | 197,712 | 143,360 | 185,704 | 7,232,604 | 34.25% |
| Clean Quran/BOOT repeat and controls | 199,204 | 143,360 | 175,832 | 7,232,812 | 35.46% |

Byte counts are bytes, not rounded KiB. The fragmentation indicator is
`100 × (1 − largest_free_block / total_free_internal_bytes)` at each sample.
It measures the share of free bytes outside the largest available block.
Initial idle internal free/largest were 243,264/200,704; after the clean controls
they were 240,952/159,744. Post-playback PSRAM returned above 8.36 million free
bytes. No unrequested reset or reported scheduler fault was observed in the
completed playback traces. The owner-confirmed RESET interrupted the first
Quran trace; the clean repeat and both full Athan traces have no collection error.

These bounded runs establish measured activity on this unit. Exact production
image runtime memory, repeated long streaming/soak behavior and larger output
levels remain unmeasured.

## Release qualification still required

- Independently inspect scheduled-consumption records; the October 22
  owner observation below confirms automatic playback and restart without replay.
- Qualify hardware-specific signed Wi-Fi/USB updates, settings/history/audio
  preservation and automatic rollback/recovery with the intended release images.

Powered RESET/RTC restoration is distinct from complete power-loss clock
retention, which was not tested. Battery features remain outside this milestone.

Public availability requires the remaining hardware qualification and explicit
release publication. Host/browser tests and the results above do not constitute
blanket release acceptance. Follow the [board qualification procedure](waveshare-audio-board.md#hardware-qualification).

## October 22 owner follow-up

On the regular unpublished candidate, the owner reported hearing Fajr start by
itself followed by dua. After the requested RESET and phone-page refresh, they
confirmed Dhuhr was shown next and the speaker stayed quiet without Fajr or dua
replaying. These are owner-confirmed physical observations; no device telemetry
or independent stored-consumption read was collected in that follow-up.
