# openathan-core

This directory is reserved for framework-independent OpenAthan logic.

The core should contain prayer calculation, scheduling, settings models, and playback policy without depending directly on ESPHome, Home Assistant, or a particular hardware board.

Keeping this layer independent is intentional: it should be reusable if OpenAthan later migrates from ESPHome to native ESP-IDF or another embedded framework.
