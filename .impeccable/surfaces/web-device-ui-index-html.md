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

## Approved composition

The [device UI README](../../web/device-ui/README.md) owns the current runtime,
API-observation, saving/recovery, draft/setup and browser-testing contract.
The [design system](../../DESIGN.md) owns extracted visual tokens and component
rules. This brief records the approved hierarchy and interaction presentation.

- Today leads with the device date, “Next prayer” hero, time and readiness,
  immediately followed by volume and named Skip/Restore controls. The ruled
  two-column timetable highlights one matching current-day occurrence with a
  flat amber field, amber name/time, `aria-current=time` and a visually hidden
  next-prayer label; there is no duplicate visible badge. A later-day occurrence
  appears only in the dated hero.
- Skipped Athan keeps the same hero and highlight, shows “Athan skipped today”
  and the named Restore action. Connection feedback marks retained information
  stale. Waiting for valid time shows no invented upcoming prayer.
- Stop remains reachable in Today and the sticky Settings playback container.
  Both placements display the same pending, confirmed or unconfirmed feedback.
- Settings groups everyday preferences, prayer calculations, optional screen/
  lights, time format and updates. Native controls show input immediately, with
  nearby save feedback and recovery choices. Reviewed calculations use a
  separate timetable and explicit confirmation.
- First run presents Location → Calculation → Review timetable → Finish setup,
  with destination focus on step changes. Everyday preferences, navigation and
  updates stay outside this flow. Manual entry stays available; the optional
  location helper returns a proposal for review.

## Validation and evidence

The [redesign validation report](../../docs/development/device-ui-redesign-validation-2026-10-05.md)
retains the initial integration and dated review evidence. The
[holistic review report](../../docs/development/device-ui-holistic-review-2026-10-06.md)
records its controller revision, invariant-to-test matrix, full automated checks
and matched pinned OTA measurements. The
[maintenance validation report](../../docs/development/device-ui-maintenance-validation-2026-10-07.md)
records the subsequent controller cleanup and its regression/capacity checks.
Browser checks cover 320px, 390px and desktop,
enlarged text, keyboard/focus, touch targets, contrast, 12/24-hour formatting and
live text without polling chatter. They inspect semantics and DOM behavior;
actual assistive-technology speech is unmeasured.

Each final OTA must contain the exact current gzip assets and pass the existing
1,572,864-byte application budget and partition/capacity contract. Exact sizes and
runtime evidence belong to the dated reports. Source, browser and build checks do
not imply physical-device or runtime heap/PSRAM results.

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
