# Contributing to OpenAthan

OpenAthan is intended to be reproducible by nontechnical users. Contributions should optimize for the public project, not only for a single prototype.

## Before implementing a major feature

Please open an issue describing:

- the user problem being solved;
- whether new hardware is required;
- impact on total finished-project cost;
- impact on standalone/offline operation;
- impact on installation and setup complexity;
- whether the feature can remain optional.

## Engineering principles

- Keep the required hardware list short.
- Prefer pre-assembled modules over soldering or custom PCBs for the reference build.
- Do not require Home Assistant for core functionality.
- Keep prayer calculations and scheduling local when practical.
- Keep OpenAthan core logic modular and minimally coupled to ESPHome.
- Prefer configuration through the local OpenAthan UI rather than giant user-edited YAML files.
- Preserve an upgrade path to native ESP-IDF if ESPHome becomes restrictive.
- Do not add bundled audio without verifying redistribution rights.

## Pull requests

Keep pull requests focused. Document hardware assumptions and include tests where practical. User-visible changes should update relevant documentation.

By contributing, you agree that your contributions may be distributed under the license applicable to the part of the repository you modify.
