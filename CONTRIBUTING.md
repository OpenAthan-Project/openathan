# Contributing to OpenAthan

OpenAthan is intended to be reproducible by nontechnical users. Contributions should optimize for the public project, not only for a single prototype.

## Start here

- [Build and test instructions](.github/workflows/README.md): pinned prerequisites
  and commands for the host, Python, browser and firmware checks.
- [Architecture](docs/development/architecture.md): reusable core, firmware and
  hardware-adapter responsibilities.
- [Device UI development](web/device-ui/README.md): local interface and browser tests.
- [Supported hardware](hardware/README.md) and [roadmap](docs/development/roadmap.md).
- [Report a bug or request a feature](https://github.com/OpenAthan-Project/openathan/issues/new/choose).
  For vulnerabilities, use [private security reporting](SECURITY.md).

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

Before opening a PR, rebase onto the latest `main` and review the final diff.
Use the [PR template](.github/pull_request_template.md), separating automated
checks from physical hardware results and their limitations. Documentation-only
changes need link, whitespace and source-consistency checks; they do not require
firmware builds or device access.

## Release and hardware documentation checklist

- Verify publication before changing release availability. Refresh the README,
  roadmap, product description and affected guides; update published-release
  summaries without rewriting dated validation reports as new passes.
- Describe available features separately from future work and qualification limits.
- For a supported model, add a guide and hardware-index row with exact identity,
  required parts, supported features, setup, updates/recovery and limitations.
  Check manufacturer references, physical evidence and installer availability;
  keep unvalidated models out of the supported list.
- Treat audio as essential and screens/lights/other peripherals as optional.
  Document a setup path without QR scanning. Vendor peripherals are not supported
  OpenAthan features until their adapters are implemented and validated.
- Check setup/release links and screenshots. Label simulated previews and verify
  permission before adding third-party photographs or media. Keep private paths,
  credentials and diagnostic archives outside Git.

By contributing, you agree that your contributions may be distributed under the license applicable to the part of the repository you modify.
