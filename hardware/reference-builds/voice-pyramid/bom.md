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

Official-store purchase links (both modules are required):

- [AtomS3R C126](https://shop.m5stack.com/products/atoms3r-dev-kit) — confirm the
  **GC9107 display revision** with the seller; C126 alone does not guarantee it.
- [Voice Pyramid A167](https://shop.m5stack.com/products/echo-pyramid-smart-speaker-base-without-m5atom) —
  the base is sold without the Atom controller.

These purchase links are separate from the technical references above. You may
also use a regional distributor supplying the exact supported model and revision.
Prices, shipping and availability depend on the seller.

The following retailers also list both modules. Confirm the **GC9107 display**
before ordering from any seller. The links identify product listings, not a
guarantee about the hardware revision or current stock.

| Retailer | Store region | AtomS3R C126 | Voice Pyramid A167 |
| --- | --- | --- | --- |
| DigiKey | Global, with regional sites | [Atom](https://www.digikey.com/en/products/detail/m5stack-technology-co-ltd/C126/25600008) | [Pyramid](https://www.digikey.com/en/products/detail/m5stack-technology-co-ltd/A167/29291634) |
| OpenELAB | US store, with global and German stores | [Atom](https://openelab.com/products/m5stack-atoms3r-development-kit) | [Pyramid](https://openelab.com/products/m5stack-echo-pyramid-smart-speaker) |
| The Pi Hut | UK | [Atom](https://thepihut.com/products/atoms3r-development-kit-with-0-85-display-8mb-psram) | [Pyramid](https://thepihut.com/products/echo-pyramid-smart-speaker-base-without-m5atom) |
| TinyTronics | Netherlands | [Atom](https://www.tinytronics.nl/en/development-boards/microcontroller-boards/with-wi-fi/m5stack-atoms3r-esp32-s3-pico-development-board-0.85-inch-display) | [Pyramid](https://www.tinytronics.nl/en/platforms-and-systems/m5stack/atom/m5stack-atomic-voice-pyramid-base) |

More sellers appear in [M5Stack's distributor directory](https://m5stack.com/distributor).
All purchase URLs above are ordinary links. OpenAthan does not currently earn
commissions through them. Maintainers can find the dated
[vendor and referral-program review](../../../docs/development/hardware-vendors.md).
