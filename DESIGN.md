---
name: OpenAthan device UI
description: Next Athan board — approved device UI implementation, unreleased development scope
colors:
  primary: "#f0bb67"
  primary-hover: "#ffcf86"
  upcoming-background: "#2b2c29"
  background: "#101820"
  surface: "#19252f"
  surface-raised: "#22323f"
  foreground: "#f3f6f8"
  muted: "#b6c5d1"
  control-border: "#7a8d9c"
  divider: "#354854"
  success: "#8ed8b5"
  error: "#ffabab"
typography:
  display:
    fontFamily: 'system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif'
    fontSize: "min(3.875rem, 25vw)"
    fontWeight: 700
    lineHeight: 1.1
    letterSpacing: "-0.03em"
  display-compact:
    fontFamily: 'system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif'
    fontSize: "min(3.5rem, 25vw)"
    fontWeight: 700
    lineHeight: 1.1
    letterSpacing: "-0.03em"
  display-desktop:
    fontFamily: 'system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif'
    fontSize: "4.5rem"
    fontWeight: 700
    lineHeight: 1.1
    letterSpacing: "-0.03em"
  title:
    fontFamily: 'system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif'
    fontSize: "1.25rem"
    fontWeight: 700
    lineHeight: 1.3
  section:
    fontFamily: 'system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif'
    fontSize: "1.125rem"
    fontWeight: 700
    lineHeight: 1.5
  body:
    fontFamily: 'system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif'
    fontSize: "1rem"
    fontWeight: 400
    lineHeight: 1.5
  label:
    fontFamily: 'system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif'
    fontSize: "0.9375rem"
    fontWeight: 500
    lineHeight: 1.5
  feedback:
    fontFamily: 'system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif'
    fontSize: "0.875rem"
    fontWeight: 400
    lineHeight: 1.5
  button:
    fontFamily: 'system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif'
    fontSize: "1rem"
    fontWeight: 600
    lineHeight: 1.4
  readiness:
    fontFamily: 'system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif'
    fontSize: "1.125rem"
    fontWeight: 500
    lineHeight: 1.25
  timetable-time:
    fontFamily: 'system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif'
    fontSize: "1.125rem"
    fontWeight: 600
    lineHeight: 1.5
  timetable-time-desktop:
    fontFamily: 'system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif'
    fontSize: "1.375rem"
    fontWeight: 600
    lineHeight: 1.5
rounded:
  square: "0"
  control: "8px"
  notice: "12px"
spacing:
  xs: "4px"
  sm: "8px"
  md: "12px"
  lg: "16px"
  section: "20px"
  group: "24px"
  desktop: "32px"
  column: "64px"
components:
  button-primary:
    backgroundColor: "{colors.primary}"
    textColor: "{colors.background}"
    typography: "{typography.button}"
    rounded: "{rounded.control}"
    padding: "9px 16px"
  button-primary-hover:
    backgroundColor: "{colors.primary-hover}"
  button-secondary:
    backgroundColor: "{colors.surface}"
    textColor: "{colors.foreground}"
    typography: "{typography.button}"
    rounded: "{rounded.control}"
    padding: "9px 16px"
  button-secondary-hover:
    backgroundColor: "{colors.surface-raised}"
  field:
    backgroundColor: "{colors.surface}"
    textColor: "{colors.foreground}"
    typography: "{typography.label}"
    rounded: "{rounded.control}"
    padding: "10px 12px"
  navigation:
    backgroundColor: "{colors.background}"
    textColor: "{colors.muted}"
    rounded: "{rounded.square}"
  navigation-current:
    textColor: "{colors.primary}"
  readiness:
    textColor: "{colors.success}"
    typography: "{typography.readiness}"
  timetable-row:
    textColor: "{colors.foreground}"
  notice:
    backgroundColor: "{colors.surface}"
    textColor: "{colors.foreground}"
    rounded: "{rounded.notice}"
    padding: "16px"
---

# Design System: OpenAthan device UI

<!-- SCOPE: approved composition implemented in the production asset paths at unreleased development scope. Browser evidence uses simulated APIs. No hardware installation, deployment or release acceptance; historical generated-comp pixel reproduction remains unqualified. -->

## Overview

**Creative North Star: "Next Athan board"**

The Next Athan board makes the speaker's next action understandable at a glance.
Information carries the identity: readable time, explicit state and precise,
comfortable controls. Ink-dark flat surfaces and system fonts support a clear
household control surface that works locally, without a framework or remote
assets. Native HTML, CSS and JavaScript are compressed into firmware by the
existing asset pipeline.

This records the approved composition implemented in the production asset paths
at **unreleased development scope**. The user approved the browser composition
and explicitly authorized integration. The implementation extends that composition
with authoritative local API state; browser captures use simulated APIs.
The original generated composition predates the approved upcoming highlight and
Next prayer hero revisions. Its historical pixel reproduction gate remains open,
without exact reproduction certification. This document does not establish
physical behavior, runtime headroom or release readiness.

**Key Characteristics:**

- Ink-dark surfaces with restrained amber emphasis.
- Familiar native controls and plain-language states.
- Clear rules and tabular time figures rather than repeated nested cards.
- Local feedback beside the action that caused it.

The source for these values is the production [stylesheet](web/device-ui/style.css),
[markup](web/device-ui/index.html) and [controller](web/device-ui/app.js).
The [surface contract](.impeccable/surfaces/web-device-ui-index-html.md) owns the
approved page composition and API behavior. The
[dated validation report](docs/development/device-ui-redesign-validation-2026-10-05.md)
records the independent finish review and its listed-fix verdict, automated
API/browser validation and matched firmware capacity measurements. Historical
prototype reviews remain evidence of the earlier approved browser work, with
their original simulation-only scope.

Representative captures show the real production assets with simulated APIs:
[phone Today](docs/development/device-ui-redesign/today-phone.png),
[desktop Today](docs/development/device-ui-redesign/today-desktop.png),
[skipped prayer](docs/development/device-ui-redesign/skipped-phone.png),
[desktop Settings](docs/development/device-ui-redesign/settings-desktop.png) and
[setup review](docs/development/device-ui-redesign/setup-review-phone.png).
No capture shows a physical speaker or supplies firmware raster assets.

## Colors

The frontmatter is normative for the implemented device UI. Amber is a restrained action and
selection color within an ink-dark, blue-tinted neutral palette.

### Primary

- **Restrained amber:** explicit primary actions, active navigation, warnings and keyboard focus.
- **Light amber:** the implemented primary-button hover response.

### Neutral

- **Ink background:** page and navigation canvas.
- **Flat blue-black surface:** controls, notices and playback containers.
- **Raised blue-black surface:** secondary-button hover response.
- **Bright foreground:** primary text and timetable values.
- **Muted blue-grey:** supporting information, eligibility and inactive navigation.
- **Strong control border:** visible field and secondary-button boundaries.
- **Quiet divider:** timetable rules and boundaries between settings groups.
- **Upcoming background:** a subdued warm field under the next scheduled prayer.
  Amber name/time plus semantic wording identifies the row without implying playback.

### State

- **Mint success:** readiness and confirmed save text.
- **Soft pink error:** failed or unconfirmed save text.

**The Words Carry State Rule.** Readiness, warning and save outcomes always have
words; accent color and the optional state dot reinforce that wording.

## Typography

**Display Font:** the system sans stack in frontmatter.
**Body Font:** the same system sans stack.

**Character:** Familiar local-browser text makes the interface legible without
loading a font. Time values use tabular figures; the display weight belongs to
the next time, while labels and feedback stay quieter.

### Hierarchy

- **Display:** the phone next-time role; compact and desktop variants preserve its weight, line height and tracking.
- **Title:** timetable and regular section headings; the settings-view heading has its own larger source treatment.
- **Section:** settings-group headings.
- **Body:** ordinary explanatory copy and controls.
- **Label:** field names; fields inherit this role.
- **Feedback:** nearby save results, hints and supporting information.
- **Readiness:** plain text with a small circular indicator; desktop inherits the body line height.
- **Timetable time:** tabular values with a larger desktop variant.

**The Native Type Rule.** Use the supplied system stack and tabular time figures;
no external font service or bundled display face is required.

## Layout

The device UI uses flat groups and ruled lists. Its repeated spacing steps live
in frontmatter; they are extracted values, not a requirement to fill every gap.
The device container is centered with a maximum width (1120px). Phones use side
padding (20px), reducing to (16px) at widths up to (350px). At desktop widths
starting at (768px), the container uses side padding (32px) and the two columns
are separated by a gap (64px).

Navigation is labeled Today/Settings at the phone bottom and moves above the
content on desktop. The header uses smaller phone typography and stacks at the
compact breakpoint. Timetable rows remain two columns through responsive reflow.
The hero is labeled “Next prayer” and uses the device's observed next occurrence.
Only a matching occurrence in today's timetable is highlighted. A next-day
occurrence stays in the hero with its date and leaves today's rows neutral.
Skipping Asr retains its name/time in the hero, shows amber “Athan skipped today”
and offers “Restore Asr today”. Hero and row advance when authoritative device
state advances; they retain the last observed state with stale feedback when
disconnected. Neither identifies an upcoming prayer while waiting for valid time.
The optional date header uses the device's read-only local date and stays hidden
when that value is unavailable.

The selected hero/control order and single upcoming-row emphasis belong to the
[surface contract](.impeccable/surfaces/web-device-ui-index-html.md).

Buttons, ranges and disclosure summaries have minimum targets (44px); fields and
checkbox labels use minimum heights (48px). These are minimums, not fixed boxes:
enlarged text can wrap and extend the document. The checkbox grid adapts to its
content width; coordinate fields stack at the compact breakpoint. Phone hero times
cap their scale to the viewport to keep each time legible with wider system fonts
and enlarged text. Prayer names can wrap within the timetable column; native
selects ellipsize the closed value while their options remain available.

## Elevation & Depth

There are no box shadows, gradients, glass or photographic backdrops. Depth comes
from flat tonal surfaces, quiet rules and stronger control borders. Notices and
the sticky playback bar use a distinct surface; ordinary settings groups stay
unboxed.

**The Flat Groups Rule.** Use rules and spacing for ordinary groups; reserve a
bounded surface for a notice or playback state that needs separate attention.

## Shapes

Control corners use the frontmatter's modest control radius. Notices and the
playback bar use the notice radius. Navigation is square; the readiness dot is
circular. There is no readiness pill, timetable card or card around each setting.

## Components

### Buttons

Explicit action labels, the primary amber fill and a quieter secondary surface
share the same control shape. Primary hover uses light amber; secondary hover
uses the raised surface. Disabled buttons retain their styling with reduced
opacity (0.55). Keyboard focus uses an amber outline (3px) and offset (3px).
The implementation defines no custom active-state transformation.

### Inputs / Fields

Native fields have a strong border (1px), control corners and flat surface fill.
Labels sit above fields with a gap (6px). Fields use the same amber keyboard focus
as buttons; there is no custom field-hover color. Native ranges and checkboxes
use amber through the browser's accent-color styling, with visible text labels
and current values.

### Navigation

Text labels accompany inline SVG icons. The selected view uses amber text and an
underline (2px). Phone targets have a minimum height (58px); desktop targets use
(56px). Navigation remains transparent on hover; the selected underline and
keyboard outline communicate state without a fabricated pill treatment.

### Status and feedback

Readiness is a flex row of text and a dot (12px), without a filled container.
Nearby feedback distinguishes adjusting, saving, saved, warning and failure with
words. Only successful-feedback text has a color transition (150ms ease-out),
and only when reduced motion is not requested. Network response delays are not design
motion tokens.

### Timetable rows

A definition list pairs the prayer name and smaller eligibility text with a
tabular time. Rows have transparent backgrounds, quiet horizontal rules, a
minimum height (52px) on phone and (76px) on desktop. Preview rows may show the
previous time beneath the draft value. The confirmed timetable highlights one
upcoming prayer with the warm upcoming-background field, amber name/time and
modest control-radius corners. Equal gutters keep columns aligned. Eligibility
text stays readable; a skipped Athan retains the highlight until the schedule
advances. Draft previews keep neutral rows. The current row carries aria-current
and a visually hidden next-prayer label. Its occurrence-selection rule belongs
to the surface contract.

### Notices / Containers

A bounded notice uses the flat surface, a strong border (1px) and the notice
radius. The playback container uses the same surface and radius without a
border; it stays reachable while the settings page scrolls. Setup progress uses
plain step labels and an amber current-step rule rather than a new card system.

## Do's and Don'ts

### Do:

- Do use native semantic controls, system fonts and tabular time figures.
- Do keep feedback beside its control and express every state in words.
- Do preserve readable focus, labeled navigation and expanding touch targets.
- Do use rules and spacing for ordinary groups, with surfaces for notices.

### Don't:

- Don't require remote assets, a framework or an external font service for the local interface.
- Don't add decorative motion, unlabeled controls or unexplained status codes.
- Don't present simulated values, browser checks or this document as release acceptance or real-speaker evidence.
