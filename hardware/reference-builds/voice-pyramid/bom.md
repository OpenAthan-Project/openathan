# Voice Pyramid Reference Build — Bill of Materials

[Compare supported speakers](../../README.md) · [Assembly and setup](assembly.md)

## Required hardware

| Component | Model / SKU | Purpose |
|---|---|---|
| M5Stack AtomS3R | **C126, GC9107 display revision** | Controller, display and front button; newer ST7735 revisions unqualified |
| M5Stack Voice Pyramid | **A167** | Speaker, RGB lights and enclosure; microphones and touch controls unused by OpenAthan |
| USB-C data cable and computer | Desktop Chrome or Edge for browser setup | USB installation and credential recovery |
| USB power source | Suitable for the Pyramid's documented bottom USB-C power input | Normal operation; the data cable may be reused for power |
| Home Wi-Fi | 2.4 GHz WPA2/WPA3 personal network | Initial setup and internet time; no enterprise or web sign-in networks |

The reference build uses pre-assembled modules and requires no soldering. See the
[assembly and single-cable power instructions](assembly.md).

> **Product naming:** A167 is Voice Pyramid and has also been called Echo Pyramid. Verify the SKU when purchasing.

Official references:

- [M5Stack AtomS3R C126 documentation](https://docs.m5stack.com/en/core/AtomS3R)
- [M5Stack A167 documentation](https://docs.m5stack.com/en/atom/Voice_Pyramid)
- [M5Stack A167 reference source](https://github.com/m5stack/M5Echo-Pyramid)

## Optional expansions

No additional modules are required. Clocks, removable storage and external audio
outputs are [possible future expansions](../../expansions/README.md), not currently
supported accessories. Home Assistant integration also remains future work.

## Development-only hardware

A second Atom/audio base may be useful to contributors for firmware experimentation, but it is **not** part of the public end-user BOM.

## Procurement notes

Prices, vendors, and availability change frequently. Project documentation should distinguish reference SKUs from vendor-specific purchase links so users can buy from appropriate distributors in their region.
