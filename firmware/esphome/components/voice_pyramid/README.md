# Voice Pyramid ESPHome Component

Planned hardware adapter for the M5Stack Voice Pyramid A167.

M5Stack's current official documentation calls SKU A167 Echo Pyramid. OpenAthan retains Voice Pyramid as its project-facing name for the same product.

Responsibilities are limited to verified A167-specific audio hardware, microphone, RGB LEDs, capacitive-touch controls, power, and initialization behavior as required. AtomS3R C126 platform and display configuration belong in the reference board layer, not this component.

This component exposes verified A167 functions so the reference-hardware package can bind them to generic capability interfaces. It must not contain prayer calculation, scheduling, or product policy, and it must not become a dependency of the hardware-independent `openathan` component.
