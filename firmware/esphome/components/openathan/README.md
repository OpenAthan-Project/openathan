# OpenAthan ESPHome Component

Planned hardware-independent adapter between ESPHome and `openathan-core`.

This component will own OpenAthan-facing state and scheduling integration, and will communicate with hardware only through capability adapters. Audio playback is fundamental. Status lights, displays, controls, microphone input, RTC, removable storage, and Home Assistant integration are optional.

Do not put prayer calculation policy exclusively in ESPHome YAML. Core behavior should remain testable outside ESPHome. This component must not contain Voice Pyramid GPIOs, call A167-specific behavior directly, or require Home Assistant.
