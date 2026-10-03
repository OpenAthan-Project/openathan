# Reference release and public installer — 2026-10-03

[v0.2.1](https://github.com/OpenAthan-Project/openathan/releases/tag/v0.2.1) is a
published stable GitHub release from source
`caa81a465ded47f5c4b4ab97a165e1a3432a50e4`. This summary reconciles release,
production-device and public-website evidence. It does not replace the dated
reports for earlier tested images or claim new hardware tests.

## Exact release build and public delivery

The [exact main CI run](https://github.com/OpenAthan-Project/openathan/actions/runs/37083770562)
passed all eight jobs. The published [build report](https://github.com/OpenAthan-Project/openathan/releases/download/v0.2.1/build-report.json)
records Python 3.13.0, ESPHome 2026.9.0, ESP-IDF 5.5.5 and compiler
`esp-14.2.0_20260121`. Dependency, partition, image integrity, production-only
profile, embedded identity and rollback-bootloader checks passed.

| Measurement | Bytes |
| --- | ---: |
| Actual OTA application | 1,211,712 |
| Remaining 1.5 MiB application budget | 361,152 |
| Free space in each 2 MiB application slot | 885,440 |
| Static RAM | 113,351 |
| Separate shared audio partition | 3,670,016 |

The two application slots and shared-audio layout are unchanged. Static RAM is
not a runtime heap/fragmentation or PSRAM measurement. The matched TX-buffer
change is **+16 application bytes**; its [regression report](upgrade-download-validation-2026-10-02.md)
uses a fixed build identity to isolate that change and is distinct from the
exact release image above.

All seven published assets matched their reviewed bytes through anonymous HTTPS
downloads. The signed descriptor, source/tag identity, committed recording
approval and production audio-format validation passed. The public latest
`upgrade.json` redirect also matched the release descriptor. These desktop checks
do not establish on-device OTA application transfer.

| Asset | SHA-256 |
| --- | --- |
| `manifest.json` | `b6ef1cd38e8feb75ed90d0aeb454ea7ae033d49ded439ab293b29ec510eeb437` |
| `firmware.ota.bin` | `fed9bc05bee935d9b512d9bc941743962378b8561e35ebc882a05ec8568a761e` |
| `athan-audio.bin` | `ca74b33e3e8a504b42c414ba8361438101d41d7ac458d6f4367dccaf1981f228` |

Both selected recordings have [documented approval and attribution](../../AUDIO-LICENSES.md).
The shared audio is identical to the v0.2.0 bundle; application OTA preserves a
speaker's existing recordings rather than replacing them.

## Production reference-device observations — 2026-10-02 Toronto

The exact v0.2.1 application was installed through a coordinated application-only
USB procedure. Independent application readback and before/after comparisons
passed for the bootloader, control/OTA regions, settings, credentials, prayer
history, light preferences, inactive application and shared audio. No historical
NVS restore, factory erase or production test instrumentation was used.

Bottom-only startup observations confirmed the exact production identity, active
setup, applied settings, synchronized clock, automatic readiness, healthy
scheduler and no queued installation. A real owner-requested public update check
while idle progressed through checking to current with no error. GitHub latest
was still v0.2.0 at that time, so no downgrade was offered or queued. This proves
public descriptor discovery/redirect/signature verification on this image; no
OTA application transfer or slot switch was requested during that check.

Earlier production v0.2.0 observations covered cold-restart preservation and
real scheduled normal/Fajr playback. The listener reported Fajr as clear and
accepted the complete replacement normal recording as clear and comfortable.
These are earlier source-bound observations, not repeated v0.2.1 listening tests.

## Retained qualification and limits

The [maintainer qualification checkpoint](upgrade-qualification-2026-10-01.md)
records interrupted attempts, power cuts, pending-boot rollback, corrected
shutdown handoffs, both retained recordings and a reconciled 3,601.579-second
run with 720 status samples and 48 completed private-feed descriptor checks.
Its heap/PSRAM/stack results belong to that isolated source and trust profile.
The [earlier capacity/device report](feasibility-report.md) retains provisioning,
local controls, persistence, audio and light observations with their dates.

Remaining limits include:

- No new physical fresh installation of the exact v0.2.1 factory bundle.
- No exact-v0.2.1 public application-transfer/slot-switch observation or public
  descriptor check during scheduled playback.
- No exact-v0.2.1 maximum-URL TX-buffer concurrent-playback memory measurement;
  earlier isolated measurements do not establish that headroom.
- Broader hardware/browser/network coverage and overnight/long-term stability
  are not established by bounded reference-unit tests.
- Lower-right touch nonresponse remains unresolved; touch, display, microphone,
  Quran, RTC and expanded-storage product features are not implemented.

## Public website — 2026-10-03

[Website PR #8](https://github.com/OpenAthan-Project/website/pull/8) merged as
`e5e44af52233edd3a037a47cfbe1c7d174459eb4` and its
[validation/deployment run](https://github.com/OpenAthan-Project/website/actions/runs/37120020739)
passed. Checks included 111 unit tests, 195 development-browser passes with three
expected skips, and 159 built-site browser passes. Live HTTPS pages, installer
selection and `/release.json` matched that website commit and v0.2.1; served
manifest, factory and audio hashes matched the published release. Location
help and owner-update guidance were available, simulator/missing routes returned
404, and the `www` HTTPS redirect passed.

The website's 30-minute approved-release polling workflow is active. A read-only
selection check against the deployed metadata reported unchanged and fresh,
which exercises no-op comparison but does not establish actual scheduled-run
timing. GitHub schedules may be delayed/dropped or disabled after inactivity;
see [deployment and rollback guidance](https://github.com/OpenAthan-Project/website/blob/e5e44af52233edd3a037a47cfbe1c7d174459eb4/deployment/README.md).
Website adoption never updates an existing speaker on its owner's behalf.
