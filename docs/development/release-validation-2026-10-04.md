# Reference release and public installer — 2026-10-04

[v0.3.0](https://github.com/OpenAthan-Project/openathan/releases/tag/v0.3.0) is
the published stable GitHub release from source
`1915fe0a161640518f25d967ea3e58d5bc65b93d`. It adds the optional dim GC9107
clock, next-prayer and playback display. Text is upright from the side opposite
the Pyramid's ports; brightness defaults to 10%. Ordinary connected states are
quiet, and Wi-Fi loss shows an `Offline` footer while scheduling and stored audio
continue with a valid clock. Cold boots still require time synchronization.

This summary records final build, delivery and scoped reference-unit acceptance.
Earlier [display observations](display-validation-2026-10-03.md),
[orientation measurements](display-orientation-validation-2026-10-03.md) and
[v0.2.1 release evidence](release-validation-2026-10-03.md) retain their dated
source limits.

## Exact release build and public delivery

All eight jobs in the [release-source CI run](https://github.com/OpenAthan-Project/openathan/actions/runs/37162147058)
passed. The published [build report](https://github.com/OpenAthan-Project/openathan/releases/download/v0.3.0/build-report.json)
records Python 3.13.0, ESPHome 2026.9.0, ESP-IDF 5.5.5,
`esp-14.2.0_20260121` and tzdata 2026.4. Source/build identity, dependency pins,
image integrity, production-only profile, factory/OTA consistency, unchanged
partition layout and compatible rollback bootloader checks passed.

| Measurement | Bytes |
| --- | ---: |
| Actual OTA application | 1,249,440 |
| Remaining 1.5 MiB application budget | 323,424 |
| Free space in each 2 MiB application slot | 847,712 |
| Static RAM | 115,123 |
| Separate shared audio partition | 3,670,016 |

The exact production image is **37,728 bytes larger** than published v0.2.1
using the same pinned toolchain. It is 32 bytes larger than the
[v0.3.0 development measurement](v0.3.0-preparation-2026-10-03.md), which used
the compile-only source identity instead of the full release commit. These are
different build inputs; the earlier matched display/orientation/version reports
isolate their individual changes. Both application slots and shared audio are
retained. Static RAM does not establish runtime headroom.

The release was published as stable latest on **2026-10-04 at 14:18:18 Toronto**
(18:18:18 UTC). Anonymous HTTPS verification passed for all seven reviewed assets,
the signed descriptor, approved recordings and exact source/tag identity. The
public latest `upgrade.json` redirect matched the signed release descriptor.
Desktop delivery checks do not establish on-device OTA application transfer.

| Asset | SHA-256 |
| --- | --- |
| `manifest.json` | `287eff3cae08719205140e5cfbd62e5bb6f333d9481b5288aea3b0109430dbaf` |
| `firmware.factory.bin` | `6ef9f3367d43e00f2f3dc0b948307ad0ca94103e5ca84f6f73f56ae3431fe45f` |
| `firmware.ota.bin` | `2da39aac82c6800254054f0ff5e29cb1f4894eb4804d9653c7d805f4b2c0f090` |
| `athan-audio.bin` | `ca74b33e3e8a504b42c414ba8361438101d41d7ac458d6f4367dccaf1981f228` |

The approved normal/Fajr recordings and [attribution](../../AUDIO-LICENSES.md)
are unchanged from v0.2.1. Compatible application updates preserve installed
recordings and saved data. Fresh USB installation erases settings, credentials
and prayer history; existing speakers use the
[preserving update guidance](firmware-upgrades.md).

## Reference-device acceptance — 2026-10-03–04 Toronto

The exact production application was installed through a fresh guarded,
application-only USB procedure. Independent application readback passed.
Before/after comparisons confirmed preservation of settings, credentials, prayer
history, shared audio, bootloader, inactive application and control/OTA regions.
No historical recovery image was restored.

Startup on Pyramid bottom power confirmed exact production identity, active and
applied setup, synchronized clock, healthy automatic scheduling and no queued
installation. The owner confirmed upright, readable text from opposite the
ports on both the isolated image and the installed production application.
Validation is for AtomS3R C126 with GC9107 + Voice Pyramid A167; ST7735 revisions
remain unqualified.

The earlier attended isolated display run covered both complete recordings,
readability and comfortable dimness, button/browser controls, and scheduled
playback while offline. Those observations are reused within their recorded
source limits; they are not new exact-production listening/control tests.

## Focused runtime memory observations

An isolated instrumented derivative of the same release source completed three
normal/Fajr pairs with idle recovery. The 3,600.087-second device-uptime interval
contains 556 retained telemetry samples, 76 redraws and the same observed boot.
Production excludes the private telemetry and isolated test storage.

Every settled largest PSRAM block was 7,208,960 bytes, retaining a fixed
1,048,576-byte reduction from the initial 8,257,536. Final free PSRAM was
8,354,152 versus initial 8,354,312. Settled internal largest block reached
172,032 after the first Fajr and stayed there through later tracks and final
idle; cumulative minimum free internal heap was 208,276. No progressive
largest-block decline was observed in the retained samples. Complete
fragmentation recovery is not established.

The capture has a **13m50s observation gap** after an observer HTTP timeout and
later hostname lookup failures. The interrupted track was stopped once and its
idle outcome independently reconciled before distinct remaining plays completed
the six full tracks. A later authenticated hostname read passed; the underlying
first-timeout cause remains unresolved. This is a bounded hour-long device
interval, not an uninterrupted one-hour capture.

These instrumented measurements do not establish exact-production maximum-load
heap/PSRAM/stack headroom or long-term stability. This record does not include
physical fresh-install acceptance of the exact v0.3.0 factory bundle or public
OTA application transfer/slot switching on this image. Broader hardware, browser
and network coverage remain unqualified; touch nonresponse remains unresolved.

## Public website — 2026-10-04

The [website validation/deployment run](https://github.com/OpenAthan-Project/website/actions/runs/37225345989)
passed at website source `346a3574bc52af543f86c77e40c6a0e63ae570b9`.
Formatting, release import, type/unit checks, development and built-site browser
checks passed before the freshness check and deployment.

At **14:46:49 Toronto** (18:46:49 UTC), anonymous HTTPS checks confirmed that
the homepage, installer and `/release.json` selected v0.3.0 at that website
commit. Served manifest, factory and audio bytes matched the reviewed published
release, including the hashes above. This is public delivery verification, not
a physical browser installation or setup test.

The observed run used a manual workflow dispatch. The existing automatic
stable-release adoption policy and 30-minute polling schedule remain configured;
this run does not establish future scheduled-run timing. Website adoption does
not update an existing speaker on its owner's behalf.
