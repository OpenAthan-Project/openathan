# AtomS3R display backlight

This optional C126 adapter controls the LP5562 white output on the Atom's
internal I2C bus (SDA GPIO45, SCL GPIO0, address `0x30`). It does not access
the Pyramid bus, amplifier, LEDs or touch controller. Failed initialization
stops this component without changing scheduler or upgrade health.

The reference configuration initializes at 10% linear PWM duty and retains
17.5 mA white-channel current. `display_brightness_percent` remains a build-time
initialization value (1–100). When wired through `display_output_id`, the saved
screen preference takes precedence after OpenAthan starts; a missing or corrupt
record uses 10%. Runtime changes write only the PWM register, preserve the current
and other channels, and skip unchanged values. Failed runtime writes can be
retried without repeating initialization. The local page saves through
`/api/display`; see the [API contract](../../provisioning/README.md#screen-brightness-unreleased).
Always-on operation does not automatically raise brightness for faults/playback.

Initialization is adapted from [M5Stack's LP5562 component](https://github.com/m5stack/esphome-yaml/blob/cc708ddcc9dea7cfc746b408d2495ce281bbf2d2/components/lp5562/lp5562.cpp).
The upstream MIT license is retained in [LICENSE](LICENSE). The reduced adapter
uses checked writes and explicit configuration values; it includes no engines,
general-purpose light entities or network dependency. The original helper's
uninitialized configuration variable is not retained.

Pin ownership and screen revisions follow [M5Stack's C126 documentation](https://docs.m5stack.com/en/core/AtomS3R).
The accompanying profile targets GC9107. The integrated dim display passed
[attended isolated hardware acceptance](../../../../docs/development/display-validation-2026-10-03.md#attended-isolated-hardware-acceptance)
on 2026-10-03, including upright text, readability and comfortable dimness at
the 10% default. This bounded, instrumented maintainer test does not establish
release readiness or long-soak behavior. That run used rotation 180; physical
confirmation of the corrected ports-at-back rotation 0 remains pending.
Newer ST7735 revisions are not qualified by this profile.
