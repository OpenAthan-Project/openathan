# AtomS3R + Voice Pyramid — Parts and setup

[Compare supported speakers](../../README.md)

## Identify the model

Use an **M5Stack AtomS3R C126** with a **Voice Pyramid A167** speaker base.
A167 is also called **Echo Pyramid**; check the SKU. The supported Atom display
profile uses **GC9107**. Newer **ST7735** revisions are not qualified, so confirm
the display revision before buying; the C126 SKU alone is not sufficient.

See the manufacturer's [AtomS3R documentation](https://docs.m5stack.com/en/core/AtomS3R)
and [Voice Pyramid documentation](https://docs.m5stack.com/en/atom/Voice_Pyramid).
The Atom and Pyramid are separate modules; no soldering or custom PCB is needed.

## Required parts

Use the [parts list](bom.md) to check the two modules, USB data cable, USB power
source, computer and Wi-Fi requirements. A charging-only cable cannot install
firmware. Installation and USB recovery require desktop Chrome or Edge.

## Supported features

This build supports scheduled normal/Fajr Athan, optional dua after Athan,
internet Quran streaming and local phone settings. The small display shows the
clock, next prayer, playback and connection information. Brightness and
12/24-hour formatting are saved. A167's RGB lights show prayer/status information
and have separate brightness controls.

The **front button** stops audio. While idle, a short press cycles through the
webpage QR, addresses and normal screen. The Pyramid's capacitive touch controls
and microphones are not supported by OpenAthan.

## Setup

1. Attach the AtomS3R to the Pyramid using the manufacturer's intended connector.
   The front faces away from the Pyramid's power and expansion ports; place those
   ports and the power cord at the back.
2. Leave Pyramid bottom power unplugged. Connect your computer to the **Atom's
   USB-C port** with a data cable. Use only this one cable for installation or
   USB recovery.
3. Open [USB setup](https://openathan.com/install/) in desktop Chrome or Edge.
   Select the connected device and confirm the correct hardware before a new
   installation. **New installation erases existing firmware and saved data.**
4. Choose phone setup now or later, or continue on the computer. When moving to
   phone setup or normal operation, unplug Atom USB and connect **only the
   Pyramid's bottom USB-C power**.
5. For phone setup, join the displayed `OpenAthan-Setup-…` Wi-Fi network manually
   or press the front button to show its Wi-Fi QR. Stay connected despite the
   no-internet warning. If the setup page does not open, visit
   `http://192.168.4.1/setup/` while on that network. Choose a speaker password
   and connect to home Wi-Fi. Perform this open-network setup in a trusted place.
6. Rejoin home Wi-Fi on your phone, open the speaker's permanent address and sign
   in as **admin** with the speaker password. Choose location, timezone and prayer
   settings, preview the timetable and select **Finish setup**. Automatic Athan
   waits for a valid clock. Start at low volume and adjust gradually.

The temporary setup address is not the everyday settings address. Press the front
button while idle to find the speaker page again. If `.local` does not open, use
the displayed IP address. Keep your phone and speaker on the same home Wi-Fi.
Follow the [complete setup guide](https://openathan.com/docs/getting-started/)
for the computer path and location helper.

## Updates and recovery

Use **Firmware → Check for updates** on the speaker's settings page, then read
the notes and choose **Install update**. Wi-Fi updates can run on Pyramid bottom
power. Compatible updates preserve settings, prayer history and recordings.

For Wi-Fi or forgotten-password recovery, unplug bottom power and reconnect only
the Atom's USB-C data port to the computer. Choose recovery through USB setup,
then return to bottom-only power for playback. Do not choose a fresh installation
to repair credentials or update an existing speaker. See the
[preserving update guide](../../../docs/development/firmware-upgrades.md) for
older firmware and bootloader compatibility, and [troubleshooting](https://openathan.com/docs/troubleshooting/)
for connection problems.

## Limitations and validation

Atom needs internet time after every cold boot before automatic Athan resumes.
Once time is valid, stored Athan and installed dua work locally. Quran streaming
needs internet. Added clocks, external speakers and storage are not supported
accessories simply because the hardware has expansion ports.

Review the [current release notes](../../../docs/releases/v0.8.0.md) and
[display revision and validation limits](../../../firmware/esphome/components/atom_s3r_display/README.md).
The calibrated maximum volume has not been qualified for distortion; increase
volume gradually. Hardware evidence applies to the recorded units and source
versions, not every retail revision or exact release image.
