# OpenAthan repository guidance

## Architecture and project boundaries

- Preserve standalone operation: prayer calculations, scheduling, settings and
  device controls must work without Home Assistant or the public website.
- Keep reusable product logic independent of ESPHome and reference hardware.
  Audio playback is fundamental; displays, LEDs, touch, microphones and other
  peripherals remain optional capabilities implemented through adapters.
- This repository owns firmware, release artifacts and the device-local UI.
  The [public website](https://github.com/OpenAthan-Project/website) consumes
  published artifacts and does not compile firmware.
- Use the [architecture guide](docs/development/architecture.md) for responsibility
  changes and [contributor guidance](CONTRIBUTING.md) when proposing major features.

## Firmware and memory budgets

- For the reference firmware, keep each actual OTA application image at or below
  **1.5 MiB (1,572,864 bytes)** within its **2 MiB slot**, preserving at least
  **512 KiB** of headroom. Retain both application slots and the separate
  **3.5 MiB shared audio partition**.
  The [partition table](firmware/esphome/feasibility/partitions.csv) and
  [capacity checker](tools/check_feasibility.py) define the existing contract.
- For firmware, embedded UI, dependency or build-configuration changes, compare
  before/after `firmware.ota.bin` sizes using the same pinned toolchain and
  configuration. Report the byte delta and remaining application budget; run the
  existing capacity checks for affected variants.
- Prefer existing capabilities and proportionate dependencies. Keep embedded UI
  assets compressed; measure the cost of added frameworks, fonts, images or large
  datasets. Keep reference recordings in shared audio storage, outside application
  images.
- Investigate unexpected growth before proposing budget or partition changes.
  Preserve update compatibility, durable records and recovery capabilities when
  evaluating size reductions.
- Assess runtime heap, fragmentation and PSRAM separately when changes affect
  concurrent audio, networking or UI activity. Static RAM figures and a successful
  build do not establish runtime headroom.

## Hardware and evidence

- Verify hardware mappings against official documentation/source and physical
  hardware; keep board-specific behavior in the appropriate adapter.
- Before hardware work, use the relevant [scheduler validation runbook](firmware/esphome/scheduler/VALIDATION.md)
  or [provisioning hardware runbook](firmware/esphome/provisioning/HARDWARE_TEST.md)
  and recheck the live device state. Preserve settings, prayer-consumption history,
  shared audio and recovery capability; isolate test storage from production data.
- Prefer compatible application-only recovery. Historical full-flash backups can
  restore old prayer history and are not routine settings or credential recovery.
- Distinguish automated checks, observed physical results and untested behavior.
  Keep dated device state and changing binary measurements in linked validation
  or build reports, such as the [capacity and validation report](docs/development/feasibility-report.md),
  rather than this instruction file. Development builds do not establish release
  readiness.

## Validation and pull requests

- Use the [CI instructions](.github/workflows/README.md) for pinned prerequisites,
  CMake/CTest host tests, Python tests, configuration checks and firmware builds.
  Use the [device UI guide](web/device-ui/README.md) for browser-test prerequisites
  and commands. CI audio fixtures are synthetic and must not be installed or played.
- Match validation to the change. For documentation-only changes, check whitespace,
  links and consistency with source; firmware builds and hardware access are not
  required. Update relevant documentation when behavior changes.
- Keep changes focused. Before creating a PR, rebase onto the latest `main`, review
  the final diff again, resolve findings or ask about unresolved issues, and rerun
  affected checks when the rebase changes the result.
- Follow the [PR template](.github/pull_request_template.md). Describe the problem
  and resulting behavior for external contributors; summarize automated and
  hardware validation separately, including material limitations.

## Licensing and private material

- Preserve [Apache-2.0 software licensing](LICENSE), [CC BY 4.0 documentation licensing](docs/LICENSE.md)
  and the [CERN-OHL-P-2.0 policy for original hardware designs](hardware/LICENSE.md).
  Third-party dependencies, hardware and media retain their own terms; preserve
  attribution and verify compatible redistribution rights before bundling media.
- Keep credentials, private diagnostic archives and recovery images outside Git.
  Keep this guidance portable: personal filesystem paths and machine-local context
  belong in local instructions, not repository documentation.
