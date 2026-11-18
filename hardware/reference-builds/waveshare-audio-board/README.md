# Waveshare AUDIO-Board — Parts and setup

[Compare published builds](../../README.md)

## Identify the model

Use **Waveshare ESP32-S3-AUDIO-Board**, the screenless speaker with seven RGB
LEDs. This is a separate board from the Touch-LCD Box. Wiring follows the
manufacturer's V1.1 schematic; identify and verify the physical revision before
installation. A model sticker alone does not establish the PCB revision.

Supported since [v0.9.0](../../../docs/releases/v0.9.0.md). The public installer
offers this model when the selected approved release supplies its matching
artifacts. Review the [latest release notes](https://github.com/OpenAthan-Project/openathan/releases/latest)
for current validation limits; a successful build does not establish physical
qualification of every release image or PCB revision.

## Required parts

The AUDIO-Board with its matching speaker, a USB data cable, suitable USB power,
a computer with desktop Chrome or Edge, and 2.4 GHz WPA2/WPA3 personal Wi-Fi.
No display, SD card, microphone accessory or battery is required.
See the [manufacturer's hardware documentation](https://docs.waveshare.com/ESP32-S3-AUDIO-Board).

An optional [3D-printable lid on MakerWorld](https://makerworld.com/models/3509161)
makes the K1, K2 and K3 buttons easier to press.

## Implemented features

Scheduled normal/Fajr Athan, Quran streaming, optional dua, local phone settings,
phone/USB enrollment and signed application updates reuse OpenAthan's product layer.
Like Box V2, the board has an onboard PCF85063 real-time clock. First use needs
internet time. OpenAthan saves verified synchronized time to the clock and can
restore it after a powered restart. A [physical RESET test](../../../docs/development/waveshare-audio-board-hardware-2026-10-21.md#observed-operation)
confirmed restoration on one unit. Time retention while unplugged,
backup-battery operation and long-term drift remain unqualified. If the clock
is invalid, scheduled playback waits for internet time synchronization.

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

Choose **Waveshare AUDIO-Board (no screen)** in the
[installer](https://openathan.com/install/) and explicitly confirm a fresh
installation. This replaces existing firmware, credentials, settings and
history. ESP32-S3 detection alone cannot identify this board.

For phone setup, power through USB-C and join `OpenAthan-Setup-…`. Keep other
unconfigured OpenAthan speakers unplugged to identify the correct network.
Open `http://192.168.4.1/setup/` if the automatic page does not appear. Choose a
speaker password and enter home Wi-Fi, copying the permanent speaker address
shown before connecting. Rejoin home Wi-Fi, open that address, sign in as
**admin**, review prayer settings and choose **Finish setup**.

Bookmark the speaker address. On firmware with the speaker finder, open
`http://openathan.local/` on home Wi-Fi to choose a speaker. This model has no
display or QR code. If the finder or saved address is unavailable, USB setup
can read its hostname and current IP address again without reinstalling.

## Updates and recovery

Use the speaker page or compatible preserving USB update for an existing
OpenAthan installation. Updates replace only an application slot and preserve
settings, prayer history and shared audio. Keep USB-C connected throughout a
USB transfer; press RESET only when the updater asks, after verification.
Use command-only USB recovery for Wi-Fi/password repair.

## Limitations

Microphones, speech recognition, camera, SD storage, external displays and
battery management are not implemented. Recorded hardware checks used USB
power. The [dated playback report](../../../docs/development/waveshare-audio-board-hardware-2026-10-21.md)
includes owner-confirmed scheduled Fajr and restart without replay. The
[private upgrade report](../../../docs/development/waveshare-audio-board-upgrades-2026-10-26.md)
records signed-update, interruption and rollback results on one unit. These
checks do not establish every release image's physical acceptance. The tested
unit's exact PCB revision remains unknown. Complete power-loss clock retention,
backup-battery operation and long-term drift remain unqualified. See the
[hardware results and limits](../../../docs/development/waveshare-audio-board-hardware-2026-10-21.md)
and [implementation guide](../../../docs/development/waveshare-audio-board.md).
