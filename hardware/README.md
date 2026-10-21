# Choose your OpenAthan hardware

OpenAthan releases currently support the two builds below. Both play scheduled
Athan, stream Quran, offer optional dua after Athan, and use phone or USB setup.
You do not need to solder, write code or run Home Assistant.

## Supported builds

| Model | Assembly | Screen and controls | Guide |
| --- | --- | --- | --- |
| M5Stack AtomS3R **C126** + Voice Pyramid **A167** | Connect two modules | Small display, front button, RGB prayer lights | [Parts](reference-builds/voice-pyramid/bom.md) · [Setup](reference-builds/voice-pyramid/assembly.md) |
| Waveshare **ESP32-S3-Touch-LCD-1.85C-BOX V2** | Pre-assembled | Round display with prayer ring, BOOT button | [Parts and setup](reference-builds/waveshare-box-v2/README.md) |

**Compatibility:** the Atom profile supports the GC9107 display; newer ST7735
revisions remain unqualified. Waveshare **V1 is unsupported**. A167 is also
called Echo Pyramid. Check the exact revision before buying.

Choose the pre-assembled Box V2 if you prefer not to connect modules. Choose the
Atom/Pyramid combination if you want the separate prayer-status lights and can
verify the supported Atom display revision. These are compatibility choices;
this documentation does not establish comparative speaker loudness or quality.

## What you need

In addition to the speaker parts, allow for a USB data cable, suitable USB power,
a computer with desktop Chrome or Edge for installation/recovery, and a 2.4 GHz
WPA2/WPA3 personal Wi-Fi network. Check what your seller includes. Prices and
availability vary by region; the model guides link to manufacturer references
for identification rather than promising a particular price or seller stock.

The [public installer](https://openathan.com/install/) supplies ready-to-install
firmware. A new installation erases existing firmware and saved data. Existing
speakers should use [preserving updates or credential recovery](../docs/development/firmware-upgrades.md).

## Features and hardware limits

- **Clock:** Atom needs internet time after a cold boot. Box V2 has an onboard
  clock that can restore initialized time across powered restarts. Neither guide
  promises operation after unplugging without resynchronization; Box V2 battery
  retention and drift remain unqualified. Stored Athan and installed dua work
  locally once time is valid; Quran streaming needs internet.
- **Screen:** both current builds show time, prayer/playback status and connection
  information, with saved brightness and 12/24-hour formatting.
- **Physical controls:** the Atom front button or Box V2 BOOT button stops audio.
  While idle, a short press shows connection information. Phone controls provide
  volume and skip/restore actions. Box V2 also has a RESET button to restart it.
- **Lights:** A167 has supported RGB prayer-status lights. Box V2 uses its screen
  for the prayer ring; it is not a separate RGB light output.
- **Unused peripherals:** touch input, microphones and SD/removable storage are
  not supported by OpenAthan. Their presence in vendor specifications does not
  make them available in OpenAthan firmware.

## Other hardware and expansions

Audio playback is essential. A screen, lights, buttons, microphone, hardware
clock and removable storage are optional capabilities of the reusable design;
their actual support depends on an implemented and validated adapter. Setup
instructions must always offer a way to continue without scanning a screen.

Only the models in the supported-build table have public installation guidance.
For custom integrations, start with the [ESPHome guide](../firmware/esphome/README.md)
and [architecture](../docs/development/architecture.md). Possible future accessories
are listed separately under [expansions](expansions/README.md); they are not a
shopping list of supported add-ons.

Contributors adding a model should provide its own guide with the same headings
as the existing guides: identification, required parts, supported features,
setup, updates/recovery, and limitations. Add a comparison row only when the
model, release/installer availability and validation evidence have been checked.
Keep hardware mappings and detailed measurements in linked development reports.
