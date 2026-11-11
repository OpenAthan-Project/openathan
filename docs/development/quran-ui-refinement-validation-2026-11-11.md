# Quran device UI refinement validation — 2026-11-11

This report records two source-bound checkpoints. The initial implementation used
`176ca3e`. The PR was subsequently rebased onto `91abee8c30200853b7953c06b001a8f6ab7ad4a7`.
The [rebased PR validation](#rebased-pr-validation) below records its fresh checks
and production image measurements. Earlier measurements and screenshots describe
the original implementation checkpoint.

This focused refinement addresses the four Quran-page critique findings while
retaining OpenAthan's ink/amber palette, native controls and standalone device UI.
It adds a playback area with observed recording identity, prominent Stop and the
shared speaker-volume control; loads reciters once on first entry; reveals dependent
choices when available; and removes completed-step instructions and unavailable
paging guidance. Catalog and playback feedback remain independent.

## Original source and review scope

The implementation is a local change based on `176ca3ebb55e5f91336bd35d8f815933d0bb0565`.
The asset hashes below identify the exact browser and firmware bytes assessed.
No package dependency, firmware playback API, partition, shared-audio payload or
update policy changed. The three volume controls use the existing settings domain,
revision checks, write scheduling, feedback and recovery.

Current recording names follow observed identifiers and known catalog metadata;
editing a replacement does not relabel current playback. The active panel stays
sticky only while sufficient space remains for recording controls. Resizing,
enlarged text and long names retain reachable fields and normal scrolling when
needed. Known reciter names are visually limited to two lines while their full text
remains in the accessibility tree.

## Original automated validation

- 542 full browser cases passed: 271 Chromium and 271 WebKit,
  with zero failures or skips. These use production UI assets, the production C++
  Digest verifier and simulated device/provider APIs.
- All 20 UBSan CTests passed.
- All 135 pinned Python checks passed, with zero skips. The successful run used the
  completed host build's validator, adapter and core-library paths, plus the resolved
  ArduinoJson headers.
- All 11 matched before/after build pairs passed their existing capacity checks.
  The exact three current gzip assets were verified against generated source and
  actual OTA bytes in all nine UI-bearing variants. Scheduler-only device and
  validation images do not embed this UI and have zero growth.

The browser cases cover first-entry loading without repeat navigation requests,
explicit retry after failure, progressive choices, catalog/playback failure separation,
current-recording identity during a new selection, reachable Stop at 320×640,
long names and 200% text at 320/390/1280px, shared volume saving, cross-view keyboard
save ordering, and conflict recovery. Existing dropped-response, stale-read,
Athan-priority and Play/Stop token cases also pass.

The final source diff was reviewed for scope, request ordering, accessible labels,
feedback, layout and shared-volume integration. Whitespace checks passed.

## Original matched firmware measurements

Both stages use Python 3.13.0, ESPHome 2026.9.0, the repository's macOS constraints,
ESP-IDF 5.5.5 and Xtensa GCC 14.2.0 (`esp-14.2.0_20260121`). Public compile-only
settings and synthetic non-decodable audio fixtures are identical between stages.
These images must not be installed or played.

| Variant | Before OTA bytes | After OTA bytes | Delta | Remaining 1,572,864-byte budget | Static RAM delta |
| --- | ---: | ---: | ---: | ---: | ---: |
| atom | 1,306,096 | 1,307,280 | +1,184 | 265,584 | 0 |
| waveshare | 1,310,032 | 1,311,216 | +1,184 | 261,648 | 0 |
| device | 1,124,496 | 1,124,496 | +0 | 448,368 | 0 |
| validation | 1,142,080 | 1,142,080 | +0 | 430,784 | 0 |
| provisioning-validation | 1,307,808 | 1,309,008 | +1,200 | 263,856 | 0 |
| upgrade-qualification | 1,312,576 | 1,313,760 | +1,184 | 259,104 | 0 |
| upgrade-startup-failure | 1,312,576 | 1,313,760 | +1,184 | 259,104 | 0 |
| waveshare-audio | 1,238,448 | 1,239,632 | +1,184 | 333,232 | 0 |
| waveshare-display | 1,288,256 | 1,289,440 | +1,184 | 283,424 | 0 |
| waveshare-diagnostics | 1,375,104 | 1,376,288 | +1,184 | 196,576 | 0 |
| quran-qualification | 1,377,568 | 1,378,768 | +1,200 | 194,096 | 0 |

All images preserve the existing two 2 MiB application slots and separate 3.5 MiB
shared-audio contract. The original scheduler-validation baseline attempt failed
before compilation because managed-component integrity metadata was incomplete;
a fresh isolated build passed. Original failed and interrupted local receipts are
retained outside the repository. Final measurements use only completed checked builds.

## Original embedded asset provenance

All images retain the firmware's existing gzip asset pipeline. Application-image
deltas include linker alignment and can differ from compressed asset deltas.

| Asset | Before gzip bytes | After gzip bytes | Delta | Original SHA-256 |
| --- | ---: | ---: | ---: | --- |
| `index.html` | 3,859 | 4,064 | +205 | `fa80c51c304bd3686d1ac175287d8ce27f48198dc7f381977e71c4f960eb94a3` |
| `app.js` | 19,413 | 20,009 | +596 | `c1b8a6980ec0f562724c3bb73bb4552b8bca68dd7f3c8d8394d2f25fcae448fe` |
| `style.css` | 3,070 | 3,460 | +390 | `d029484e8c0eebedb4b348a0c0b994e6adcc9991622ceb209d4bf3036f36e5c5` |

## Rendered evidence and limits

These screenshots show exact implemented assets with illustrative simulated API
values, including the fixture's date. They are browser captures, contain no generated
media, and provide no evidence of a physical speaker's clock or audible playback.

- [390×844 phone playback](quran-ui-refinement/phone-final.jpg)
- [1280×900 desktop playback](quran-ui-refinement/desktop-final.jpg)
- [320×640 compact phone playback](quran-ui-refinement/compact-final.jpg)

A native browser walkthrough confirmed selection, volume changing from 70% to 71%
with saved feedback, and explicit stopped feedback. Browser viewport overrides were
reset, preview tabs closed, and temporary local servers stopped.

No physical device, production settings, prayer history, stored audio or recovery
image was accessed. Real provider playback, audible output, runtime heap,
fragmentation/PSRAM and assistive-technology speech remain unmeasured by this work.
These automated and browser results do not establish release or hardware qualification.

## Rebased PR validation

The PR is based on main `91abee8c30200853b7953c06b001a8f6ab7ad4a7`. Conflict resolution preserves
speaker naming, the local finder, and `playback_busy` handling. The new Quran
heading and queued catalog request also respect both Athan and dua ownership.
Stop remains reachable during a silent dua interval.

Fresh affected browser checks passed on the rebased implementation: 73
Chromium cases and 73 WebKit cases, with zero failures. Cases outside the
selected pattern were intentionally omitted. The pagination regression pauses
the browser clock before input so its before-debounce click is deterministic.
The pattern was
`Quran|volume|speaker name|playback|Athan|dua`. The complete browser and host suites,
phone-setup cases, and all firmware variants remain PR CI checks.

Fresh matched builds cover all three production boards using the same pinned
toolchain and public compile-only settings as above. Each before and after image
passes the existing capacity checker. Static RAM is unchanged, and generated assets
were byte-checked against all three actual OTA application images.

| Production profile | Main OTA bytes | PR OTA bytes | Delta | Remaining application budget | Static RAM delta |
| --- | ---: | ---: | ---: | ---: | ---: |
| atom | 1,417,184 | 1,418,400 | +1,216 | 154,464 | 0 |
| waveshare | 1,420,976 | 1,422,192 | +1,216 | 150,672 | 0 |
| audio-board-production | 1,384,768 | 1,385,984 | +1,216 | 186,880 | 0 |

Rebased embedded source SHA-256 values:

- `index.html`: `36aef3d7247b69425ff26da2cabd17f9c0b1001ee66a2b99478cafa33f69a450`
- `app.js`: `bde054685c960defd5b851b41668ba37bfd79e78f82c944a6716f7b6dd083a2d`
- `style.css`: `366aa9b60529bd8e978026c3a3240d4b6882c6f556218916bd15d2b1441a0b06`

The Quran layout and controls retain the illustrated composition. The current
DOM, focus, long-name, enlarged-text, and responsive behavior were rechecked in
the browser cases. Physical audio, runtime heap, fragmentation, PSRAM, and
assistive-technology speech remain unmeasured.
