---
version: 2
slug: "web-device-ui-index-html"
primary_target: "web/device-ui/index.html"
related_targets: ["web/device-ui/app.js","web/device-ui/style.css"]
---

# Device UI redesign

Mode: Operate. Target: `web/device-ui/index.html` and adjacent assets.

Status: approved composition implemented in the production asset paths at
unreleased development scope. Browser evidence uses simulated APIs. No hardware
installation, public website change, deployment or release is included.

## User decisions

- Full UX redesign with Today + Settings and a replacement visual identity.
- Next Athan board: ink-dark surfaces, readable tabular prayer times, amber accents.
- Automatic saving for volume, enabled prayers, screen/light preferences and time format.
- Preview and confirm for location, calculation conventions and offsets.
- Selected composition: A's large next-time hero, C's immediate volume/skip
  placement, and B's two-column timetable with one upcoming-prayer highlight.
- The user approved restoring an upcoming-prayer highlight: Asr remains highlighted
  when skipped, until the device reports a later occurrence. The original generated
  `selected.png` predates this revision and is retained as historical provenance.
  The user approved the revised browser mockups and explicitly authorized embedded
  integration. The approved “Next prayer” revision keeps the hero and a matching
  current-day highlight on the same occurrence, even when its Athan is skipped.
- Current implementation references use the real production assets with simulated
  API data: [phone Today](../../docs/development/device-ui-redesign/today-phone.png),
  [desktop Today](../../docs/development/device-ui-redesign/today-desktop.png),
  [skipped prayer](../../docs/development/device-ui-redesign/skipped-phone.png),
  [desktop Settings](../../docs/development/device-ui-redesign/settings-desktop.png)
  and [setup review](../../docs/development/device-ui-redesign/setup-review-phone.png).
- When the next occurrence belongs to a later day, show it only in the hero with
  its date; today's timetable has no highlighted row. Do not manufacture a
  tomorrow timetable or change the approved hierarchy.

## Direction contract

THESIS: Make the speaker's next action and everyday controls immediately clear;
replace the long all-purpose configuration form with Today and grouped Settings.

OWN-WORLD: Ink-dark flat surfaces, bright system-sans text, amber actions,
tabular times, strong control boundaries, clear timetable rules, native controls.

STORY: A household member sees whether Athan will play, at what time and volume,
then changes a simple preference or reviews a consequential timetable change.

FIRST VIEWPORT: Device header and date, large next prayer/time/readiness, volume
and named Skip directly underneath, then a two-column timetable with one upcoming-prayer highlight and
labeled navigation. Desktop places hero/controls beside the timetable.

SIGNATURE INTERACTION: Slider values follow input immediately; saving begins on
release, then nearby text confirms the result. Consequential prayer changes stay
in a draft and reveal a timetable review before explicit confirmation.

FORM: Next Athan board, grounded candidate 5, direction seed b03e3581. The cultural
reference is a readable departure board, translated into a calm household control
surface without physical-board simulation, code-only labels or decorative effects.

FINISH: unreviewed and undocumented is unfinished; this build ends with the finish review, the verdict, DESIGN.md, and every shipping raster carrying its provenance

## Implemented behavior and constraints

Keep operation local, assets compressed, system fonts, existing APIs and persistence
domains. Preserve coordinate precision, revisions, consumption history and
readback-before-retry. Automatic writes must not commit unconfirmed prayer edits.
Drafts survive in-page navigation; browser navigation warns before discarding them.

### Authoritative prayer and playback state

`GET /api/status` supplies the device clock readiness, setup/application state,
playback-active boolean, next occurrence, skip and today's timetable. The UI uses
the returned local strings and saved 12/24-hour preference. The optional read-only
`local_date` field supplies the date header when the clock is ready; clients
tolerate absence and never substitute the browser's date or timezone. Its valid
clock value comes from the saved device timezone. This additive status field
does not identify the playing prayer or change persistence.

Connection loss marks retained observations stale and the current playback state
unknown, without claiming the speaker stopped. Do not invent playback history,
current-playing prayer identity or a device-accurate countdown.
Highlight the upcoming scheduled occurrence, independently of whether its Athan
is skipped. Label the hero “Next prayer” and bind it to that same observed occurrence.
Asr stays in the hero and highlighted until authoritative device state advances;
skip and restore retain both. Skipping shows “Athan skipped today” and “Restore
Asr today”, with no separate next-audible announcement. Later-day actions use the
reported occurrence date when `local_date` is available. Use a flat amber-tinted
field and amber name/time, preserving eligibility wording with no duplicate
visible next badge.
Only the confirmed Today timetable has a current row; draft/setup review tables do not.
Match the row by the authoritative device next occurrence prayer and UTC instant,
never browser time or prayer name alone. A later-day occurrence remains a dated
hero only because no current-day row has its UTC instant. Skip/Restore requests
retain the reported day/prayer key and settings revision. Preserve the
last observed hero name/time, highlight, eligibility and Skip/Restore action with
stale connection feedback. If contact is lost during Skip/Restore, show an
unconfirmed outcome without claiming success, then reconcile the authoritative
device state on reconnect. Show no hero name/time or highlight while waiting
for valid time.
The current row uses `aria-current=time` and a visually hidden next-prayer label.

Stop is reachable in Today and the sticky Settings playback container while
playback is reported active. It has an independent request path and remains
available during preference saves. An HTTP-successful Stop response that still
reports `playing:true` stays pending. Only authoritative `playing:false` confirms
completion. A failed response triggers readback; failed readback reports Stop
unconfirmed and warns that playback may still be active. Both views retain the
same pending, confirmed or unconfirmed feedback.

### Automatic preferences and prayer drafts

Volume and enabled prayers share the settings revision domain. Display brightness,
light preferences and time format retain their independent revision domains and
existing request payloads. Screen/light controls appear only for supported
capabilities; absence does not block prayer scheduling or local setup.

Slider output follows input immediately; pointer release or coalesced keyboard
changes enqueue the preference write. Writes are serialized while each domain
retains its own confirmed snapshot, desired/pending edits, revision and recovery
state. Preference saves merge into confirmed settings and never include the
separate unconfirmed location/calculation/offset draft. Save feedback distinguishes
stored values, pending application, unavailable output and storage/application
failure. Status refreshes keep unsaved edits; conflicts keep them for an explicit
choice. Uncertain writes read the matching domain before another write. Recovery
offers checking saved state, retrying retained preference edits or using saved
values; prayer conflicts require a new preview before confirmation. Discarding
preference edits preserves an independent prayer draft.

Location, timezone, calculation/Asr/high-latitude conventions and offsets remain
a versioned prayer draft. A matching ready preview enables explicit confirmation;
new edits or authoritative calculation changes invalidate the old preview.
Preview never activates setup or changes durable state. Coordinate fields retain
their supplied precision. Both 12-hour and 24-hour views use the saved device
preference for Today and preview values.

### First-run setup and optional helper

First run leads through Location → Calculation → Review timetable → Finish setup,
with native validation and destination focus on step changes. Everyday preferences
and navigation stay out of this flow. Manual entry works locally; the optional
HTTPS location helper opens separately and returns a validated proposal that
does not overwrite a dirty draft without an explicit choice. A helper proposal
requires a ready preview before activation. Manual setup can finish with a
`waiting_for_time` preview and a clear warning that announcements wait for a valid
clock. Invalid schedules cannot be confirmed. Existing owner-requested firmware
update controls remain in Settings with their original safety/state contract.

## Validation and evidence

The [dated validation report](../../docs/development/device-ui-redesign-validation-2026-10-05.md)
records 76 passing Chromium/WebKit checks, 103 Python tests without skips, 16
CTest tests, production/isolated actual API adapters, nine configuration checks
and four matched pinned firmware builds. Browser checks cover 320px, 390px and
desktop, enlarged text, keyboard/focus, practical targets, contrast, 12/24-hour
formatting and live text without polling chatter. They inspect semantics and DOM
behavior; actual assistive-technology speech is unmeasured.

Each final OTA contains the exact current gzip assets and passes the existing
1,572,864-byte application budget and partition/capacity contract. Reference OTA
growth is 5,936 bytes; isolated, qualification and rollback growth is 5,968 bytes
each. Exact before/after sizes, remaining budgets and source/toolchain details
belong to the dated report. Static RAM is unchanged; heap, fragmentation, PSRAM,
concurrent playback/network headroom and response latency remain unmeasured for
this revision. No physical results are implied by source, browser or build checks.

## Approved integration scope

Integration is a code-led extension of the approved browser composition. The
production-asset browser captures are the current critique reference; the original generated
composition predates the user’s highlight and Next prayer revisions and remains
historical provenance. Its pixel reproduction gate stays open with no production
pixel-fidelity certification. The user approved the reviewed prototype and
explicitly requested implementation. A fresh finish review requested one truthful
Stop-feedback correction; its verdict scored that fix Resolved and returned
`ship` only at the listed-fix scope. That verdict does not certify the whole
surface, historical comp, hardware, runtime headroom or release. Production
API/browser and matched capacity evidence is recorded in the dated report.
