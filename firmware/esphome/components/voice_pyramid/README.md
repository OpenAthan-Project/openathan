# Voice Pyramid ESPHome Component

Audio clock and amplifier adapter for the M5Stack Voice Pyramid A167.

Implements the 44.1 kHz Si5351 CLK1 configuration (11.2896 MHz from a 27 MHz
crystal) with register readback, and the AW87559 reset/SYSCTRL initialization
used in the private 2026-09-24 audio diagnostics. The reference hardware
package supplies the ESPHome ES8311 DAC, I2S speaker, and GPIO expander enable.
The extracted implementation passed full-recording playback and scheduler
validation on the reference unit; see the
[device results](../../../../docs/development/feasibility-report.md#integrated-device-validation--2026-09-24).

No third-party M5Stack component checkout is required. The register settings
come from the verified diagnostic; the reusable implementation is OpenAthan
code. Optional LEDs use the STM32 controller through checked I2C writes; see the
[light contract](../../../../docs/development/lights.md). Touch, microphone and
display are not implemented here.

M5Stack documents SKU A167 as [Voice Pyramid](https://docs.m5stack.com/en/atom/Voice_Pyramid); it has also been called Echo Pyramid. Verify the SKU rather than relying only on a product name.

Responsibilities are limited to verified A167-specific audio hardware, microphone, RGB LEDs, capacitive-touch controls, power, and initialization behavior as required. AtomS3R C126 platform and display configuration belong in the reference board layer, not this component.

This component exposes verified A167 functions so the reference-hardware package can bind them to generic capability interfaces. It must not contain prayer calculation, scheduling, or product policy, and it must not become a dependency of the hardware-independent `openathan` component.
