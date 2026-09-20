# Voice Pyramid Reference Build — Bill of Materials

## Required hardware

| Component | Model / SKU | Purpose |
|---|---|---|
| M5Stack AtomS3R | **C126** | ESP32-S3 controller; 8 MB flash, 8 MB PSRAM, display |
| M5Stack Voice Pyramid | **A167** | Speaker/audio hardware, microphone, RGB LEDs, touch controls, enclosure |
| USB power source and cable | Generic, suitable for the documented power input | Power |

The reference build is intentionally based on pre-assembled modules and should require no soldering.

> **Product naming:** M5Stack's current documentation calls SKU A167 **Echo Pyramid**. OpenAthan uses **Voice Pyramid** as its project-facing name for that same product. Verify the SKU when purchasing.

Official references:

- [M5Stack AtomS3R C126 documentation](https://docs.m5stack.com/en/core/AtomS3R)
- [M5Stack A167 documentation](https://docs.m5stack.com/en/atom/Echo_Pyramid)
- [M5Stack A167 reference source](https://github.com/m5stack/M5Echo-Pyramid)

## Optional expansions

Not required for the first release:

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
