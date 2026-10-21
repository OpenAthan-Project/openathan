# Waveshare Box V2 — Parts and setup

[Compare supported speakers](../../README.md)

## Identify the model

Use **Waveshare ESP32-S3-Touch-LCD-1.85C-BOX V2**, the pre-assembled speaker
with a round display. **V1 is not supported** because its audio hardware differs.
Verify the exact model and revision with the seller before buying; the ESP32-S3
chip name alone does not identify a compatible speaker.

See the [manufacturer's model documentation](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.85C)
and OpenAthan's [verified profile and hardware notes](../../../docs/development/waveshare-box-v2.md).
Vendor descriptions also cover capabilities that OpenAthan does not use.

## Required parts

| Item | What to check |
| --- | --- |
| Waveshare Box V2 | Exact model and revision above; no module assembly required |
| USB data cable | Fits the rear USB-C port; charging-only cables cannot install firmware |
| USB power source | Suitable for the manufacturer's USB-C power requirements; check what is included |
| Computer | Desktop Chrome or Edge for installation and USB recovery |
| Home Wi-Fi | 2.4 GHz WPA2/WPA3 personal network; no enterprise authentication or web sign-in |

## Supported features

The speaker supports scheduled normal/Fajr Athan, optional dua after Athan,
internet Quran streaming and local phone settings. Its round screen shows the
clock, next prayer, prayer ring, playback and connection information. Brightness
and 12/24-hour formatting are saved.

The **BOOT** button stops audio; a short press while idle shows the webpage QR
and addresses. **RESET** restarts the device. Touch input, microphones and SD
storage are not supported by OpenAthan.

## Setup

1. Connect the rear USB-C port to your computer using a USB data cable. This same
   port supplies power during normal use.
2. Open [USB setup](https://openathan.com/install/) in desktop Chrome or Edge and
   select the connected device. For a new installation, choose **Waveshare Box
   V2** and confirm the model and full erasure. Installation replaces existing
   firmware, credentials, settings and prayer history.
3. Choose phone setup now or later, or continue on the computer. For phone setup,
   power the speaker through its rear USB-C port and join the displayed
   `OpenAthan-Setup-…` Wi-Fi network manually or using its QR code. Stay connected
   despite the no-internet warning. If the setup page does not open, visit
   `http://192.168.4.1/setup/` while on that network.
4. Choose a speaker password and connect the speaker to home Wi-Fi. Rejoin that
   home Wi-Fi on your phone. Open the speaker's permanent address and sign in as
   **admin** with the speaker password. Initial phone setup uses an open network;
   perform it in a trusted place.
5. Choose location, timezone and prayer settings, preview the timetable and select
   **Finish setup**. Automatic Athan waits for a valid clock. Start at low volume
   and adjust gradually.

The temporary setup address is not the everyday settings address. Press BOOT
while idle to find the speaker's current webpage address. If `.local` does not
open, use the displayed IP address. Keep your phone and speaker on the same home
Wi-Fi. See the [complete setup guide](https://openathan.com/docs/getting-started/)
for the computer path and location helper.

## Updates and recovery

Use **Firmware → Check for updates** on the speaker's settings page, then read
the notes and choose **Install update**. Keep power connected. Compatible updates
preserve settings, prayer history and stored recordings. To change Wi-Fi or reset
a forgotten speaker password, reconnect through USB setup and choose recovery.
Do not choose a fresh installation for a preserving update or credential repair.

See the [preserving update guide](../../../docs/development/firmware-upgrades.md)
for older firmware and bootloader compatibility, and [troubleshooting](https://openathan.com/docs/troubleshooting/)
for connection problems.

## Limitations and validation

First use needs internet time. The onboard clock can restore previously
synchronized time after a powered restart. Time retention while unplugged,
backup-battery behavior and long-term clock drift remain unqualified. Quran
streaming requires internet; stored Athan and installed dua work once the clock
is valid.

Review the [current release notes](../../../docs/releases/v0.8.0.md) and
[model-specific validation](../../../docs/development/waveshare-box-v2.md#attended-validation).
Publication does not imply testing of every exact release image, panel lot,
network condition or long-running workload.
