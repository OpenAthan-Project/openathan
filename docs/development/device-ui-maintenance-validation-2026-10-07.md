# Device UI maintenance validation — 2026-10-07

This records the conservative cleanup after PR head
`bf541b209b0db1f85fa9fc64e92e322f0e91383c`. The validated controller is
`c8edd6733a77fdf7cb001f12ad8997e69cf2b4fa`; subsequent documentation changes do not change its assets.
The [device UI README](../../web/device-ui/README.md) owns the current behavior
and browser-testing contract. Earlier [integration](device-ui-redesign-validation-2026-10-05.md)
and [holistic review](device-ui-holistic-review-2026-10-06.md) reports retain their
dated evidence and source boundaries.

## Cleanup

- Scenario-specific positive UI waits replace the global pending-message regex.
  Startup drains its observed requests and waits for rendered settings/setup;
  request completion alone is not treated as an applied response.
- `pump()` schedules writes/actions; `canSaveDomain()` selects an eligible domain
  and `saveDomain()` owns the existing preference-write execution. Request and
  response acceptance, rendering, drafts, playback, controls, the optional helper
  and updates have explicit sections within the single embedded script.
- Save execution and owner-selected recovery retain their original bodies and
  mutation/cleanup order. Recovery intents remain distinct. No state owner,
  request, retry path, asynchronous delay, dependency or build step was added.
- The design system retains visual tokens, the surface brief retains approved
  composition, and both point to the README for runtime behavior. Metadata points
  to that contract and this record. Prior reports, screenshots and provenance
  are preserved. APIs, payloads, persistence, markup, styling and control copy
  are unchanged by this cleanup.

## Automated validation

- The revised harness passed all **464 Chromium/WebKit scenarios** against the
  unchanged `bf541b2` controller, then all **464** against the refactored controller.
  Both runs had zero failures, cancellations or skips, using Node **24.19.0** and
  Playwright **1.62.1**, production assets and the production C++ Digest verifier
  with simulated endpoints. All existing scenarios remain.
- The eight revision-aligned timetable cases also ran against known broken
  controller `352fefc`. All eight rejected its obsolete `15:45` time instead of
  the confirmed `16:45`, retaining the regression check after the wait changes.
- **16/16 UBSan host suites**, **103 Python tests** with zero skips, and all
  **nine ESPHome configuration checks** passed. Out-of-tree host binaries were
  supplied through the existing validator/library environment variables; bridge
  tests used the exact resolved ArduinoJson headers.
- JavaScript syntax, modified documentation links, metadata JSON and Git
  whitespace passed. Follow the [CI instructions](../../.github/workflows/README.md)
  and [browser prerequisites](../../web/device-ui/README.md#browser-validation)
  to reproduce the suites.

## Matched actual OTA measurements

Both phases use the same disposable source/build paths, public compile-only
settings, synthetic audio/trust fixtures, Python **3.13.0**, ESPHome **2026.9.0**,
ESP-IDF **5.5.5**, and compiler **esp-14.2.0_20260121**. A temporary wrapper fixed
ESPHome build metadata at `2026-10-07 00:00:00 +0000`; installed dependencies and
the repository build pipeline were unchanged. Only `app.js` changed between
phases. Measurements use actual `firmware.ota.bin` files.

| Variant | Before OTA bytes | After OTA bytes | Cleanup delta | Application budget remaining |
| --- | ---: | ---: | ---: | ---: |
| reference | 1,265,600 | 1,265,760 | +160 | 307,104 |
| isolated | 1,267,376 | 1,267,552 | +176 | 305,312 |
| qualification | 1,272,768 | 1,272,928 | +160 | 299,936 |
| forced rollback | 1,272,768 | 1,272,928 | +160 | 299,936 |

All eight before/after capacity checks passed and verified exact gzip embedding,
factory/OTA payload equality, pinned dependencies, storage/qualification isolation
and audio exclusion. Resolved dependency hashes and static RAM are unchanged.
Both **2,097,152-byte** OTA slots and the separate **3.5 MiB** shared audio
partition remain. The **1,572,864-byte** application budget is unchanged; the
largest candidate has **824,224 bytes** free in its slot.

Compressed JavaScript grew **149 bytes**, from **15,173** to **15,322**; the
unchanged HTML/CSS bring total compressed UI to **21,677 bytes**. This accounts
for the small OTA growth after image alignment. Compression without the added
section labels measured **15,191 bytes**, confirming that the labels account for
most of the growth and helper extraction/reordering for the remainder.
The validated JavaScript SHA-256 is
`ee55dc065b91180b6fea4d12c5dca38f86fed1f8281f8d85fb14c227fc1f58bb`.

## Limits

Browser endpoints and CI audio are synthetic; these builds must not be installed
or played. Hardware behavior, device latency, runtime heap/fragmentation/PSRAM,
concurrent physical playback/network headroom and actual screen-reader speech
were not measured. These checks do not establish physical-device or release
readiness.
