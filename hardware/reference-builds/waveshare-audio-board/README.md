# Waveshare AUDIO-Board — Parts and setup

[Compare published builds](../../README.md)

## Identify the model

Use **Waveshare ESP32-S3-AUDIO-Board**, the screenless speaker with seven RGB
LEDs. This is a separate board from the Touch-LCD Box. Wiring follows the
manufacturer's V1.1 schematic; identify and verify the physical revision before
installation. A model sticker alone does not establish the PCB revision.

**Availability:** included in the [published v0.9.0 release](https://github.com/OpenAthan-Project/openathan/releases/tag/v0.9.0).
[Attended checks on one USB-powered unit](../../../docs/development/waveshare-audio-board-hardware-2026-10-21.md)
are recorded. The release notes retain physical fresh-installation, retry and
recovery limits for the public installer. Check the [installer](https://openathan.com/install/)
for current availability; compiling this profile does not qualify another unit.

## Required parts

The AUDIO-Board with its matching speaker, a USB data cable, suitable USB power,
a computer with desktop Chrome or Edge, and 2.4 GHz WPA2/WPA3 personal Wi-Fi.
No display, SD card, microphone accessory or battery is required.
See the [manufacturer's hardware documentation](https://docs.waveshare.com/ESP32-S3-AUDIO-Board).

The [official Waveshare store listing](https://www.waveshare.com/product/arduino/boards-kits/esp32-s3/esp32-s3-audio-board.htm)
offers battery and no-battery options. USB-powered use does not require
the battery. Confirm the physical board revision and included speaker with the
seller. This purchase link does not establish public release readiness.
It is an ordinary link; OpenAthan does not currently earn a commission through it.
Regional sellers appear in [Waveshare's distributor directory](https://www.waveshare.com/distributors).

An optional [3D-printable lid on MakerWorld](https://makerworld.com/models/3509161)
makes the K1, K2 and K3 buttons easier to press.

## Implemented features

Scheduled normal/Fajr Athan, Quran streaming, optional dua, local phone settings,
phone/USB enrollment and signed application updates reuse OpenAthan's product layer.
The onboard RTC uses the existing verified-initialization rules. Offline
cold-start retention requires a working time source and separate physical validation.

All seven LEDs show the existing prayer/status colours together, with saved
brightness and an off switch. KEY1 lowers volume, KEY2 stops audio and KEY3 raises
volume. Each volume press changes the saved setting by five percentage points.
BOOT stops audio on a short press. Hold it for about three seconds (2–5 seconds),
then release to skip the next Athan; repeat the same hold to restore it.
RESET restarts the device.

With the USB-C socket at the top, the five buttons down the right edge are
**RESET → BOOT → KEY3 → KEY2 → KEY1**. KEY2 is the fourth button down; BOOT is
immediately below RESET. Use KEY2 or a short BOOT press to stop playback.

## Setup

When the installer offers a compatible release, choose **Waveshare AUDIO-Board (no screen)**
in the installer and explicitly confirm a fresh installation. This replaces
existing firmware, credentials, settings and history. ESP32-S3 detection alone
cannot identify this board.

For phone setup, power through USB-C and join `OpenAthan-Setup-…`. Keep other
unconfigured OpenAthan speakers unplugged to identify the correct network.
Open `http://192.168.4.1/setup/` if the automatic page does not appear. Choose a
speaker password and enter home Wi-Fi, copying the permanent speaker address
shown before connecting. Rejoin home Wi-Fi, open that address, sign in as
**admin**, review prayer settings and choose **Finish setup**.

Bookmark the speaker address. This model has no display or QR code. USB setup
can read its hostname and current IP address again without reinstalling.

## Updates and recovery

Use the speaker page or compatible preserving USB update for an existing
OpenAthan installation. Updates replace only an application slot and preserve
settings, prayer history and shared audio. Keep USB-C connected throughout a
USB transfer; press RESET after verification to complete startup checks.
Use command-only USB recovery for Wi-Fi/password repair.

## Limitations

Microphones, speech recognition, camera, SD storage, external displays and
battery management are not implemented. Hardware qualification uses USB power.
The tested unit's PCB revision marking remains unknown. The published v0.9.0
notes record later scheduled-playback, signed-update and rollback evidence,
including a preserving USB update of the final signed application on that unit.
Physical fresh installation, retry and recovery through the public installer
remain untested. Battery-backed clock retention, maximum output and an overnight
soak were not established. See the
[hardware results and limits](../../../docs/development/waveshare-audio-board-hardware-2026-10-21.md)
and [implementation guide](../../../docs/development/waveshare-audio-board.md).
