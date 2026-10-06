# Device UI holistic review — 2026-10-06

Unreleased development work for PR #29. This review preserves the approved
composition, native controls, API payloads and four persistence domains. It
replaces overlapping browser state ownership with versioned edits and common
response acceptance. The [earlier report](device-ui-redesign-validation-2026-10-05.md)
retains the integration and individual review history through `d8b0042`.

No hardware was accessed, flashed or played. Public website changes, merge,
release and deployment are outside this work. All browser device data is
**simulated**; actual API adapters, Digest verification and firmware compilation
are checked separately.

## Regression baseline

Before editing the controller, 16 new Chromium/WebKit checks failed against
`d8b0042bcc882cd662643b46c20840cc0a914b5e`. They reproduce an older volume timer
overwriting newer input in the other view, an older firmware poll hiding a newer
available update, and a late setup preview navigating after Back. Both volume
directions and native pointer/keyboard input are covered, along with late preview
errors and duplicate submissions. Controlled clocks and response gates determine
the ordering; these checks do not depend on controller globals.

The first complete refactor run retained all original scenarios but exposed two
failures in stale time-format recovery choices. That behavior was restored.
Eight additional checks then exercised older failed refreshes after newer contact,
a queued prayer review whose calculation baseline changed, and acknowledgment
during an unfinished newer drag. Each failed before its correction. The resulting
suite retains the original 316 scenarios and adds 30.

The next full run exposed a compatibility gap in two existing Stop/Skip cases:
a successful Stop reply arriving after a failed newer Skip readback could no
longer confirm stopped playback. The common policy now orders playback by
successful observations, since a failed read contains no new playback state.
Successful contact permits Stop confirmation while retaining uncertain Skip;
older failures still cannot revoke newer successful contact.

The final full-PR review also found a recovery helper hiding prayer-draft recovery
buttons when an optional domain recovered. Display reproductions failed in both
engines. Recovery now closes only its captured groups; six passing targeted cases
cover display, lights and time format while a separate prayer draft remains
conflicted, then complete that draft without repeating its write.

## State ownership and complexity removed

Each persistence domain owns confirmed revision/value, field edits, one captured
operation, and its recovery condition. A field edit has a value, version, release
eligibility and optional keyboard timer. Both volume controls use the same field.
New input cancels its previous timer; acknowledgment removes only the captured
versions. Recovery choices likewise resolve the edits present when recovery began,
preserving subsequent edits even when they return to the same value. Unreleased
drags are displayed immediately and cannot be sent or labeled saved.

Calculation drafts and explicitly reviewed calculation changes remain separate
from automatic preferences. Saves build from confirmed values, preserving exact
coordinates and excluding unconfirmed drafts. The existing write scheduler still
serializes writes. Blocked domains do not prevent eligible independent domains
from saving; Stop remains outside the scheduler. Uncertain outcomes require
fresh readback before another write in the affected domain.

Requests return internal `{data, order}` metadata. One acceptance policy handles
status, saves, recovery/readback, Stop, Skip and firmware responses. Durable values
follow their own revisions; operational, application and update observations also
follow request order. Older responses cannot clear newer faults or acknowledge
unconfirmed edits. The rendered snapshot projects confirmed domain values onto
operational observations; it is not another mutable source of saved preferences.

Previews capture a unique operation, draft version and confirmed calculation
baseline. Back, Discard, edited drafts, changed saved calculations and storage faults
invalidate it. Both success and failure check that lifecycle before changing
feedback, navigation or confirmation. Current pending previews disable duplicate
submission while leaving Back available.

This removes desired/pending/recovery-later patch merging, value-based edit
acknowledgment, per-element volume timers, response WeakMap metadata, overlapping
generation/playback guards and the duplicate time-format snapshot. Source is
formatted for review; a lower line count is not the simplification metric.
The existing deterministic gzip embedding is unchanged. No framework, dependency,
remote asset or build pipeline was added.

## Finding-to-test matrix

The recurring review findings map to four shared invariants: latest edits survive,
stale responses cannot regress state, uncertain writes require readback, and
independent controls remain usable. Test names below identify scenarios in
[`tests/device_ui.test.cjs`](../../tests/device_ui.test.cjs); parameterized cases
run in both engines and across applicable persistence domains.

| Finding or interaction | Invariant and representative scenario |
| --- | --- |
| Older Today/Settings volume timer wins | Latest edits survive: `shared volume owner keeps newer … input after switching from …`; both directions, native pointer and keyboard |
| Unreleased input or same-value edits are consumed by acknowledgment/recovery | Latest edits survive: `versioned acknowledgment never labels an unfinished newer drag saved`; existing edits-during-recovery cases across all four domains |
| Nested enabled-prayer patches repeat old values | Captured versions only: `successive enabled-prayer edits clear acknowledged nested patches` |
| Preference recovery discards a separate calculation draft or coordinate precision | Latest edits survive: existing recovery, draft, precision and helper-handoff cases; `preference conflict recovery … invalidates a stale prayer preview` |
| Late preview success/error jumps forward after Back, or Discard restores confirmation | Lifecycle acceptance: `preview lifecycle ignores a … setup preview after Back`, `preview lifecycle permits only one pending setup preview`, `Discard invalidates an in-flight timetable preview` |
| Queued reviewed calculations use a changed baseline | Fresh review required: `versioned review waits for fresh preview when its queued calculation baseline changes`; sent/unconfirmed Discard cases |
| Older available/current firmware poll or Stop result reverses update observations | Freshness per resource: `shared acceptance keeps newer update status after an older firmware poll`, `delayed Stop retains newer firmware check results` |
| Older healthy save/readback clears a newer revision, application failure or storage fault | Revision and fault acceptance: `a stale healthy … acknowledgment retains newer preferences and user edits`, same-revision application cases, older save/storage-fault cases |
| Older Skip/Restore/Stop regresses occurrence, readiness, timetable or playback | Operational freshness: delayed Skip/Restore response/readback cases, `delayed Stop cannot hide a newer authoritative playback observation`, newer timetable/date cases |
| Older failed refresh/readback marks newer successful contact disconnected | Contact freshness: `shared acceptance ignores an older … refresh failure after newer contact`, `stale … readback preserves connection and active Stop` |
| Lost reply retries an already committed write, or allows another write without confirmation | Readback before writing: `lost committed response is verified once without repeating a save`, uncertain and committed-write recovery cases in all domains |
| A faulted/conflicted prayer domain prevents healthy controls or Stop | Independent controls remain usable: `a … queued Skip allows independent saves and waits for recovery`, unreadable-store cases, Stop-during-save checks |
| Recovery in another domain hides prayer recovery controls | Independent group ownership: `independent … recovery leaves prayer-draft recovery reachable`; display, lights and time format |
| Storage recovery hides first-run setup, replaces drafts or moves focus on every poll | Recovery retains interaction: setup recovery cases exercise the actual five-second timer, manual waiting-clock completion and helper ready-preview gating |
| Narrow/enlarged layouts reorder controls or repeat live feedback | Discoverability and accessibility: Settings keyboard-order matrix, layout/contrast/target tests and unchanged-poll live-text checks |

## Automated validation

- **346 Chromium/WebKit scenarios passed**, with zero failures or skips: all 316
  original scenarios and 30 added regressions. The 130-case response-ordering
  batch, 24-case affected recovery batch and six independent-group cases also
  passed. A separate optional visual-capture case passed in both engines.

- **103 Python tests passed**, with zero skips. Production and isolated adapters
  exercise the actual API, persistence, activation, timezone conversion and Digest
  behavior; additive device-local date coverage remains intact.
- **16/16 host tests passed** with UndefinedBehaviorSanitizer and
  `-fno-sanitize-recover=all`.
- **Nine ESPHome configurations validated.** Reference, isolated, qualification
  and forced-rollback OTA builds completed; all four capacity checks passed.
- JavaScript syntax, local documentation links and Git whitespace passed.

The browser suite uses production HTML/CSS/JavaScript and the C++ Digest verifier
with simulated settings endpoints. Thirteen waits coupled to superseded private
flags were replaced with completed request chains and observable group feedback.
The new overlap checks use controlled clocks, response gates and native input;
no fixed wait is used to decide their ordering.

Layouts retain Today/Settings and first-run composition at 320px, 390px and 1280px,
ordinary/200% text, native/wider system fonts, both time formats and optional
hardware present/absent. Coverage checks keyboard order, visible Stop, at least
44px control targets, 4.5:1 text and 3:1 control contrast, skipped/highlight behavior,
stale connection information and quiet unchanged status feedback. Visual captures
were inspected against the approved appearance. Actual VoiceOver/NVDA speech is
unmeasured.

Use the [browser prerequisites](../../web/device-ui/README.md) and
[CI commands](../../.github/workflows/README.md) to reproduce the suites.

## Matched actual OTA measurements

Before images are the archived `d8b0042` PR head. After images contain the current
controller, identified by the asset hashes below. Both phases use identical
disposable source/build directories, public compile-only configuration, frozen
build settings, Python 3.13.0, ESPHome 2026.9.0, ESP-IDF 5.5.5 and
`esp-14.2.0_20260121`. Dependency versions and resolved hashes are unchanged.
Measurements use actual `firmware.ota.bin` files, not linked-image estimates.

| Variant | Before bytes | After bytes | Refactor delta | Total delta from `16fd942` main | Application budget remaining |
| --- | ---: | ---: | ---: | ---: | ---: |
| reference | 1,263,520 | 1,265,264 | +1,744 | +9,120 | 307,600 |
| isolated | 1,265,296 | 1,267,040 | +1,744 | +9,136 | 305,824 |
| qualification | 1,270,688 | 1,272,432 | +1,744 | +9,152 | 300,432 |
| forced rollback | 1,270,688 | 1,272,432 | +1,744 | +9,152 | 300,432 |

The **1,572,864-byte** application budget is unchanged. Both **2,097,152-byte**
slots and the separate **3.5 MiB** shared audio partition remain. Reference slot
free space is 831,888 bytes; the largest variant leaves 824,720 bytes, exceeding
the required 512 KiB headroom. Capacity checks verify factory/OTA payload equality,
partitions, pinned dependencies, test/qualification isolation and audio exclusion.

| Embedded asset | Before gzip bytes | After gzip bytes | Delta | Source SHA-256 |
| --- | ---: | ---: | ---: | --- |
| `index.html` | 3,392 | 3,392 | 0 | `e149859c0a88df4e9d996e0f9602fbfc73d80a723c9ce3b999ac9f2f9761f40c` |
| `app.js` | 13,090 | 14,835 | +1,745 | `d33775fd779b07fb907f0d32d68b7ced2eedbc3f8ac7e26caee50be5374e9496` |
| `style.css` | 2,963 | 2,963 | 0 | `9752c994e6672c191aa5481c284d0ff0a9a3c797d4be14794fba93cd5b60d36e` |

All three exact gzip byte sequences were verified in every after OTA. Total
compressed UI size is 21,190 bytes, versus 19,445 before this refactor and 12,125
on main. Formatting readable source and replacing state guards accounts for the
measured application growth; no new device dependency or partition is required.

| Variant | Before OTA SHA-256 | After OTA SHA-256 |
| --- | --- | --- |
| reference | `5e0e77ab1ccf82813714cd5e8d058da00153120f037a1264a92af84fe29258cd` | `6919c425ddf1dbd743a14c1a821e79604a6a521738a9c7e91bbe1cf4314d0718` |
| isolated | `ef234e67a6cac4872982c77a7c7cedd88a6bea251777b7bf5952b7965554800b` | `4ba2c195270c096a6ca12dd02495e3babaa862cd73972300237d1309249fa2ed` |
| qualification | `da796b329891f1580f4b73de1ef8ac50e9ad95de35806d666fcc561bbf87b813` | `9728ebe6e2bfbaa591b1bcde29a1f5b3a443b3d7b888063c3868ffc6ec380c19` |
| forced rollback | `2678f1d34378e48610063069a934c3409ab90f67fce9fc8acedf49eacfb9a639` | `71585976ecdd3a09235ce06ed0852ad55df8f2c9b74edae08ccd7dcb1f3097df` |

## Runtime and physical evidence

Static RAM is unchanged: 115,219 bytes for reference/isolated and 115,291 bytes for
qualification/rollback. The refactor changes browser state and adds no polling
interval or device runtime dependency. Builds and browser checks do not establish
runtime heap, fragmentation, PSRAM, concurrent playback/network headroom or device
latency. No new physical measurements were taken. Synthetic CI audio is compile
evidence only and must not be installed or played.
