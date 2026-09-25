# OpenAthan ESPHome Component

Hardware-independent adapter between ESPHome and `openathan-core`. The
[scheduler development guide](../../scheduler/README.md) documents settings,
interfaces, state persistence, actions and verification.

This component owns OpenAthan-facing state and scheduling integration, and
communicates with audio through the portable `Playback` interface. Audio playback
is fundamental. Status lights, displays, controls, microphone input, RTC,
removable storage, and Home Assistant integration are optional.

The bridge loads separate settings and consumption records, uses saved timezone
rules for OpenAthan dates, preflights scheduling edits, and verifies requested
volume through the playback adapter. Reads, full-snapshot writes, revisions and
fault/application status are documented in the [settings guide](../../scheduler/SETTINGS.md).
Networking is optional; product setup and authentication must remain outside
the reusable scheduler and preserve its persistence contract.

Do not put prayer calculation policy exclusively in ESPHome YAML. Core behavior should remain testable outside ESPHome. This component must not contain Voice Pyramid GPIOs, call A167-specific behavior directly, or require Home Assistant.
