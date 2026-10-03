# Voice Pyramid Reference Build — Bill of Materials

## Required hardware

| Component | Model / SKU | Purpose |
|---|---|---|
| M5Stack AtomS3R | **C126** | ESP32-S3 controller; 8 MB flash, 8 MB PSRAM, display |
| M5Stack Voice Pyramid | **A167** | Speaker/audio hardware, microphone, RGB LEDs, touch controls, enclosure |
| USB-C data cable and computer | Desktop Chrome or Edge for browser setup | USB installation and credential recovery |
| USB power source | Suitable for the Pyramid's documented bottom USB-C power input | Normal operation; the data cable may be reused for power |

The reference build uses pre-assembled modules and requires no soldering. See the
[assembly and single-cable power instructions](assembly.md).

> **Product naming:** A167 is Voice Pyramid and has also been called Echo Pyramid. Verify the SKU when purchasing.

Official references:

- [M5Stack AtomS3R C126 documentation](https://docs.m5stack.com/en/core/AtomS3R)
- [M5Stack A167 documentation](https://docs.m5stack.com/en/atom/Voice_Pyramid)
- [M5Stack A167 reference source](https://github.com/m5stack/M5Echo-Pyramid)

## Optional expansions

Not required by the current reference build:

- battery-backed RTC;
- microSD or USB storage;
- external powered speaker/audio output;
- additional controls;
- Home Assistant.

These should only become part of the required BOM if testing proves they are necessary for reliable core operation.

## Development-only hardware

A second Atom/audio base may be useful to contributors for firmware experimentation, but it is **not** part of the public end-user BOM.

## Procurement notes

Prices, vendors, and availability change frequently. Project documentation should distinguish reference SKUs from vendor-specific purchase links so users can buy from appropriate distributors in their region.
