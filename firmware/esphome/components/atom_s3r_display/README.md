# AtomS3R display backlight

This optional C126 adapter controls the LP5562 white output on the Atom's
internal I2C bus (SDA GPIO45, SCL GPIO0, address `0x30`). It does not access
the Pyramid bus, amplifier, LEDs or touch controller. Failed initialization
stops this component without changing scheduler or upgrade health.

The reference configuration uses a fixed 10% linear PWM duty and 17.5 mA
white-channel current. `display_brightness_percent` is a build substitution
(1–100); there is no saved brightness setting or new device HTTP endpoint.
Always-on operation does not automatically raise brightness for faults/playback.

Initialization is adapted from [M5Stack's LP5562 component](https://github.com/m5stack/esphome-yaml/blob/cc708ddcc9dea7cfc746b408d2495ce281bbf2d2/components/lp5562/lp5562.cpp).
The upstream MIT license is retained in [LICENSE](LICENSE). The reduced adapter
uses checked writes and explicit configuration values; it includes no engines,
general-purpose light entities or network dependency. The original helper's
uninitialized configuration variable is not retained.

Pin ownership and screen revisions follow [M5Stack's C126 documentation](https://docs.m5stack.com/en/core/AtomS3R).
The accompanying profile targets GC9107, matching earlier source-bound physical
display evidence. The new integrated dim display has not received physical
acceptance. Newer ST7735 revisions are not qualified by this profile.
