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

## Follow-up review of `9e380af`

Two P2 findings escaped the refactor suite: the browser fixture copied submitted
timezone rules instead of modeling the API's same-zone rule preservation, and
Discard cases did not combine an unsent review with a concurrent revision change.
Ten added Chromium/WebKit scenarios failed against `9e380af`; four adjacent
preference-recovery cases already passed. All 14 targeted cases now pass.

Drafts exclude device-managed timezone rules, so previews and reviewed saves merge
with the current confirmed rules. Explicit timezone refresh retains its existing
API semantics. Discard clears a canceled review's block only when no preference
edits or settings operation remain, restoring Preview and Skip. Released edits
and unfinished drags keep their recovery paths; sent writes retain their existing
confirmation requirement. The correction adds six controller lines without adding
a state owner, scheduler or dependency. The fixture models same-zone normalization
and preserves exact posted payloads for assertions.

The matched follow-up OTA delta from `9e380af` is **+48 bytes** for reference and
**+64 bytes** for isolated, qualification and forced rollback. The current tables
below include the refactor and subsequent corrections against the original
`d8b0042` baseline.

## Follow-up review of `967d8f7`

Successful reads restored contact without restarting eligible queued saves, and
form synchronization used the previous setup state when another client completed
setup. Sixteen new Chromium/WebKit cases failed against `967d8f7`; 12 adjacent
draft and drag safeguards already passed. The 28 added cases cover polling and
Refresh, independent display/light/time-format saves, unreleased drags, remote
setup completion with and without a revision change, and actual local drafts.

Accepted responses now resume the existing scheduler after rendering. Its existing
serialization, blocked-domain and release guards remain authoritative; reconnecting
does not retry an uncertain volume write. The accepted incomplete-to-active setup
transition fills untouched fields from confirmed settings, preserving dirty drafts.
Four additional assertions reproduced stale "Setup not finished" feedback after
the initial form correction; that same transition now updates the nearby feedback.
The controller adds four net lines and removes a duplicate queue-resumption call,
without another state owner, timer, scheduler or dependency.
The matched OTA delta from `967d8f7` is **+32 bytes** in each affected variant.

## Self-review of `9d30161`

The complete PR review reproduced two remaining acceptance gaps. In WebKit,
choosing Use saved values while a native pointer drag was held could leave that
captured edit intact; releasing afterward saved the discarded value. Recovery
now discards the captured versions even when the readback already matches saved
values. Later edits still survive through the existing version check.

Optional display/light responses report `supported: false`, revision zero and
no loaded preferences after a firmware variant stops initializing that hardware.
Those fresh observations were rejected as older saved revisions, retaining
obsolete controls and queued writes. The common acceptance policy now orders
unsupported observations independently of saved values, using the existing domain
order. It hides controls and suspends their queued writes; healthy domains stay
usable, retained edits can resume when support returns, and older replies cannot
restore the capability. Unsupported brightness defaults never become confirmed
preferences, including a later transition into unreadable storage.

Eighteen new Chromium/WebKit cases use native pointer/keyboard input, controlled
clocks and response gates. Thirteen failed on `9d30161`; five adjacent safeguards
already passed. All 18 pass with the focused changes, which add 12 net controller
lines without a new state owner, timer, scheduler or dependency.
Matched actual OTAs add **128 bytes** for reference/isolated and **112 bytes**
for qualification/forced rollback compared with `9d30161`.

## Follow-up review of `f702405`

A lost Skip or Restore response followed by failed status readback left a global
uncertainty guard blocking the entire write scheduler. A later firmware observation
restored contact, but screen, lights and time-format edits still could not save.
Twenty new Chromium/WebKit cases failed against `f702405`; eight adjacent
prayer-settings readback safeguards already passed.

The existing scheduler now applies that guard to Skip/Restore and the settings
revision domain. Eligible independent domains keep their own capability, storage,
recovery and drag-release guards. Prayer-preference and reviewed-calculation writes
still wait for authoritative status readback; queued edits survive, and the action
is never repeated. All 28 new cases pass with controlled clocks and failed status
reads held until independent saves are verified. The controller adds one net line
without a new state owner, timer, scheduler, request or dependency.
Matched actual OTAs add **16 bytes** in every affected variant compared with
`f702405`; compressed JavaScript adds **10 bytes**, with unchanged static RAM.

## Confirmation cleanup of `94af0bd`

Reconnect duplicated the common acceptance policy by clearing action uncertainty
whenever its status request succeeded. An older reconnect response could therefore
confirm a newer lost Skip or Restore after an intervening Stop. Eight new
Chromium/WebKit cases failed against `94af0bd`; all eight pass after removing that
override. They cover both actions and Stop from Today or Settings, retain queued
volume edits and calculation drafts, allow independent time-format saving, and
require fresh status before saving prayer preferences. Each action is sent once.

The reconnect handler retains domain recovery and timezone retries. Action
confirmation follows the accepted observation's request order. Save-response
acceptance also removes a disconnected check that could never run after successful
acceptance had established contact; its missing/stale-result, revision and fault
checks remain. These deletions remove six net controller lines without adding
state, requests, timers, dependencies or another recovery path.
Matched actual OTAs decrease **32 bytes** for reference and **48 bytes** for
isolated, qualification and forced rollback compared with `94af0bd`. Compressed
JavaScript decreases **38 bytes**, with unchanged static RAM.

## Location handoff review of `2b1cae1`

The hash-change handler treated a local anchor as a new empty helper proposal.
Using keyboard Skip to content while timezone loading or saved prayer storage was
unavailable therefore discarded the pending location. Eight Chromium/WebKit
reproductions failed against `2b1cae1`, covering both initial URL suggestions and
later helper handoffs. Six adjacent cases already passed; all 14 pass with the fix.

The existing fragment parser now updates the pending proposal only when it
recognizes a helper fragment. Native anchor navigation and focus remain intact.
New valid suggestions still replace older pending ones; invalid helper links
still report their error while preserving manual edits. Consumed suggestions are
not replayed by local anchors or Refresh, and no location settings save before
explicit review. The correction adds two net controller lines without a new
state owner, timer, request, dependency or API change.

Matched actual OTAs are unchanged for reference and increase **16 bytes** for
isolated, qualification and forced rollback compared with `2b1cae1`. Compressed
JavaScript increases **9 bytes**, with unchanged static RAM.

The first Linux CI run exposed early assertions in two new Chromium cases.
Repeated runs with the same Node version observed blank fields followed by the
correct suggestion without further interaction. These assertions now wait for
the resulting coordinates or invalid-link message, rather than network completion
alone. The production correction remains the same two controller lines.

A subsequent Linux run failed the existing Skip/Restore helper's immediate
contact assertion. That helper and its independent-domain checks now await
restored contact, confirmed saving and restored action availability. The gates
use visible UI outcomes and retain the assertions and simulated request counts.

## Timetable revision review of `352fefc`

A Stop response could have newer request order but an older prayer-settings
revision. Its playback observation remained useful, but it replaced the timetable
and upcoming occurrence after an offset save: saved Asr at 16:45 displayed 15:45.
Four new Chromium/WebKit cases initially failed against `352fefc`, covering Stop
replies and lost-reply readbacks. Reverse-delivery cases initially passed because
later save verification repaired the display. Holding that verification exposed
old prayer times after the new revision had been accepted; all eight gated cases
fail on `352fefc` and pass after the correction. They also send Skip with the
confirmed revision and corrected UTC occurrence, retaining one highlighted row.

The existing acceptance policy now rejects lower-revision prayer observations
and accepts the timetable accompanying a newly confirmed revision. Same-revision
observations retain request freshness. Playback has a separate request-order field
within the existing observation owner, so a Stop confirmation remains usable
without reviving older prayer data. Older faults and uncertain writes retain their
existing readback rules. The correction adds 12 net controller lines and one
freshness field, without another snapshot owner, timer, request path, dependency
or API change.
Matched actual OTAs increase **144 bytes** for reference, qualification and forced
rollback, and **128 bytes** for isolated compared with `352fefc`. Compressed
JavaScript increases **152 bytes**, with unchanged static RAM.

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
follow their own revisions; prayer observations stay aligned with settings revision
and use request order within that revision. Playback, application and update
observations retain their independent freshness. Older responses cannot clear newer
faults or acknowledge unconfirmed edits. The rendered snapshot projects confirmed domain values onto
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
| Skip to content clears a pending location while setup data is unavailable | Helper fragment ownership: `pending helper from … survives Skip to content during unavailable …`; startup/runtime handoff, timezone/storage recovery and keyboard focus. `latest recognized location fragment …` retains replacement/error behavior; `unrelated fragments do not replay …` preserves manual drafts |
| Late preview success/error jumps forward after Back, or Discard restores confirmation | Lifecycle acceptance: `preview lifecycle ignores a … setup preview after Back`, `preview lifecycle permits only one pending setup preview`, `Discard invalidates an in-flight timetable preview` |
| Queued reviewed calculations use a changed baseline | Fresh review required: `versioned review waits for fresh preview when its queued calculation baseline changes`; sent/unconfirmed Discard cases |
| Older available/current firmware poll or Stop result reverses update observations | Freshness per resource: `shared acceptance keeps newer update status after an older firmware poll`, `delayed Stop retains newer firmware check results` |
| Older healthy save/readback clears a newer revision, application failure or storage fault | Revision and fault acceptance: `a stale healthy … acknowledgment retains newer preferences and user edits`, same-revision application cases, older save/storage-fault cases |
| Older Skip/Restore/Stop regresses occurrence, readiness, timetable or playback | Operational freshness: delayed Skip/Restore response/readback cases, `delayed Stop cannot hide a newer authoritative playback observation`, newer timetable/date cases |
| A newer-order Stop reply carries an older settings revision and restores old prayer times | Revision-aligned observations: `revision-aligned timetable survives … prayer-save confirmation`; replies/readbacks in both delivery orders, Stop confirmation, one highlight and a subsequent Skip using the saved revision and occurrence |
| Older reconnect clears a newer lost Skip/Restore after Stop | Confirmation ownership: `older reconnect cannot confirm newer lost … after … Stop`; both actions and both Stop views, queued volume and preserved calculation draft, independent time-format saving, then fresh status without repeated actions |
| Older failed refresh/readback marks newer successful contact disconnected | Contact freshness: `shared acceptance ignores an older … refresh failure after newer contact`, `stale … readback preserves connection and active Stop` |
| Lost reply retries an already committed write, or allows another write without confirmation | Readback before writing: `lost committed response is verified once without repeating a save`, uncertain and committed-write recovery cases in all domains |
| A faulted/conflicted prayer domain prevents healthy controls or Stop | Independent controls remain usable: `a … queued Skip allows independent saves and waits for recovery`, unreadable-store cases, Stop-during-save checks |
| Recovery in another domain hides prayer recovery controls | Independent group ownership: `independent … recovery leaves prayer-draft recovery reachable`; display, lights and time format |
| Storage recovery hides first-run setup, replaces drafts or moves focus on every poll | Recovery retains interaction: setup recovery cases exercise the actual five-second timer, manual waiting-clock completion and helper ready-preview gating |
| Successful timezone save repeatedly enters recovery after another client saves the drafted zone | Confirmed rules own normalization: `concurrent … draft uses confirmed timezone rules after a … save reply`; both timezone directions, successful/lost replies, exact payloads and coordinate precision |
| Discard leaves an obsolete review conflict blocking Preview and Skip | Independent controls remain usable: `Discard after a queued review conflict …`; no edits, released preferences and unfinished drags, followed by successful recovery and Skip |
| Successful contact leaves healthy queued preferences idle after another domain's failed save/readback | Independent controls remain usable: `restored contact via … resumes …`; polling and Refresh across display, lights and time format; uncertain volume remains blocked and drags wait for release |
| Unconfirmed Skip/Restore blocks healthy independent preferences after contact returns | Domain-scoped uncertainty: `unconfirmed … permits independent …`; display, lights and time format save while status reads remain unavailable, drags wait for release, and actions are not repeated. `unconfirmed … retains … until status readback` keeps automatic prayer preferences and reviewed calculations queued until readback |
| Setup completed by another client leaves untouched fields blank or old setup feedback visible | Accepted setup transition: `remote setup completion …`; unchanged revision and changed calculations; untouched fields adopt confirmed values while actual drafts survive navigation |
| Use saved values leaves a captured drag that saves after release | Captured versions only: `Use saved values discards a captured native … drag before its release`; volume, display and lights with native pointer/keyboard interaction |
| Fresh unsupported hardware retains controls, queued saves or invented defaults | Capability freshness: `newer unsupported … hides controls despite an older supported reply`, `unsupported … suspends its queued save while healthy preferences remain usable`, `initially unsupported … does not supply a confirmed default after a storage fault`; display and lights, restored support and independent time-format saving |
| Narrow/enlarged layouts reorder controls or repeat live feedback | Discoverability and accessibility: Settings keyboard-order matrix, layout/contrast/target tests and unchanged-poll live-text checks |

## Automated validation

- **464 Chromium/WebKit scenarios passed**, with zero failures or skips: all 316
  original scenarios and 148 added regressions, including 14 first-follow-up,
  28 second-follow-up, 18 self-review, 28 Skip/Restore uncertainty cases and eight
  reconnect-confirmation overlap cases, plus 14 location-fragment and eight timetable-revision overlap cases.
  The 130-case response-ordering
  batch, 24-case affected recovery batch and six independent-group cases also
  passed. All 18 self-review cases were rerun after refining fixture defaults and
  mode values against production API source. A separate optional visual-capture case passed in both engines.

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

| Variant | Before bytes | After bytes | Review delta | Total delta from `16fd942` main | Application budget remaining |
| --- | ---: | ---: | ---: | ---: | ---: |
| reference | 1,263,520 | 1,265,600 | +2,080 | +9,456 | 307,264 |
| isolated | 1,265,296 | 1,267,376 | +2,080 | +9,472 | 305,488 |
| qualification | 1,270,688 | 1,272,768 | +2,080 | +9,488 | 300,096 |
| forced rollback | 1,270,688 | 1,272,768 | +2,080 | +9,488 | 300,096 |

The **1,572,864-byte** application budget is unchanged. Both **2,097,152-byte**
slots and the separate **3.5 MiB** shared audio partition remain. Reference slot
free space is 831,552 bytes; the largest variant leaves 824,384 bytes, exceeding
the required 512 KiB headroom. Capacity checks verify factory/OTA payload equality,
partitions, pinned dependencies, test/qualification isolation and audio exclusion.

| Embedded asset | Before gzip bytes | After gzip bytes | Delta | Source SHA-256 |
| --- | ---: | ---: | ---: | --- |
| `index.html` | 3,392 | 3,392 | 0 | `e149859c0a88df4e9d996e0f9602fbfc73d80a723c9ce3b999ac9f2f9761f40c` |
| `app.js` | 13,090 | 15,173 | +2,083 | `905bb2a7f9935354c49ef21ddf620f9a39d99df022019fcdbc254964bdfe7ded` |
| `style.css` | 2,963 | 2,963 | 0 | `9752c994e6672c191aa5481c284d0ff0a9a3c797d4be14794fba93cd5b60d36e` |

All three exact gzip byte sequences were verified in every after OTA. Total
compressed UI size is 21,528 bytes, versus 19,445 before this refactor and 12,125
on main. Formatting readable source and replacing state guards accounts for the
measured application growth; no new device dependency or partition is required.

| Variant | Before OTA SHA-256 | After OTA SHA-256 |
| --- | --- | --- |
| reference | `5e0e77ab1ccf82813714cd5e8d058da00153120f037a1264a92af84fe29258cd` | `8d63279ab1d3f3b26baac1bc7bd55dabb65191b6bd50bb82e0e912e83e15432b` |
| isolated | `ef234e67a6cac4872982c77a7c7cedd88a6bea251777b7bf5952b7965554800b` | `ff2bce24419143b823eaf84eeeed348536be0316d63ed64e1d69ea1690ec17d4` |
| qualification | `da796b329891f1580f4b73de1ef8ac50e9ad95de35806d666fcc561bbf87b813` | `8cc7c2f3ffe6def773d3846d81f89de4987a4989ccb662d29993b5d62e76551f` |
| forced rollback | `2678f1d34378e48610063069a934c3409ab90f67fce9fc8acedf49eacfb9a639` | `46130b4894324dd604c17f5642923297c56eb8d3b71b40ccff68c215c34f8473` |

## Runtime and physical evidence

Static RAM is unchanged: 115,219 bytes for reference/isolated and 115,291 bytes for
qualification/rollback. The refactor changes browser state and adds no polling
interval or device runtime dependency. Builds and browser checks do not establish
runtime heap, fragmentation, PSRAM, concurrent playback/network headroom or device
latency. No new physical measurements were taken. Synthetic CI audio is compile
evidence only and must not be installed or played.
