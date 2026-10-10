---
version: 2
slug: "firmware-esphome-components-openathan-display-render-h"
primary_target: "firmware/esphome/components/openathan_display/render.h"
related_targets: ["firmware/esphome/components/openathan_display/presenter.h", "firmware/esphome/components/openathan_display/status_display.cpp"]
---

# Waveshare prayer ring and countdown

Mode: Read. Target: the optional Waveshare V2 360×360 round LCD.
Status: user-approved Ring + countdown composition, implemented at unreleased
development scope. Host renders are illustrative. Attended installation,
saved-state checks and owner readability confirmation passed for both the first
separate-AM/PM layout and the approved inline-suffix refinement. Clean runtime
heap/PSRAM remains unmeasured.
See the dated report for evidence limits.

## Approved composition

The screen answers which prayer comes next and how long remains. Follow every
calculated prayer, including muted and skipped announcements. Clock, prayer
name, local scheduled time, AM/PM, countdown and proximity ring share one
occurrence. Shared Isha/Fajr occurrences use the scheduler's primary identity
and skip matching; a suppressed late Isha shows Muted.

Keep the existing bitmap font and text sizes on a black background. Center the
steady outer ring at (180,180), with outer radius 172px and inner radius 164px:
an 8px band with an 8px inset. Its color is green `#00FF00` above 30 minutes,
orange `#FF6000` above 10 through 30 minutes, and red `#FF0000` at 10 minutes
or less. Color reflects exact remaining seconds; the countdown supplies words.

| Content | Top y | Bitmap scale | Color |
| --- | ---: | ---: | --- |
| Local clock, with AM/PM in 12-hour mode | 64 | 2× | `#D0D8D8` |
| Next Athan / Will be skipped / Muted / Not ready yet | 112 | 2× | `#D0D8D8` |
| Prayer name | 140 | 4× | White |
| Scheduled time | 188 | 6× | White |
| Scheduled AM/PM suffix in 12-hour mode | 216 | 2× | `#D0D8D8` |
| Countdown | 252 | 2× | `#D0D8D8` |
| Offline, only when disconnected | 310 | 2× | `#D0D8D8` |

Center the scheduled time and AM/PM suffix as one group with a 12px gap.
Align their last occupied bitmap row at y=229. The longest 12-hour group,
`12:59 AM` or `12:59 PM`, is 284px wide. In 24-hour mode, center the time alone;
the countdown stays at the same y position. AM/PM has no separate row.

Positive UTC duration rounds up to whole minutes before splitting into hours
and minutes. Use `In 2hr 15min`, `In 1hr` or `In 8min`, omitting zero units;
below 60 seconds show `In <1min`. Midnight and daylight-saving changes affect
the local times, while UTC determines elapsed duration. Offline scheduling and
countdown continue with a valid clock.

Setup, clock-waiting, playback and fault screens retain their existing centered
2× layout and messages, with no ring or countdown. The Atom square display
retains its existing layout and audible-next behavior.

## Implementation contract

Use the portable LED timetable through a read-only event view, independently of
physical LEDs and LED preferences. Preserve schedule validation, conflict rules,
date/time correction and settings cache invalidation. No settings or HTTP API
are added. Keep the existing framebuffer and saved screen brightness, poll once
a second and redraw only when visible content changes. Add no animation,
external font, image or graphical UI dependency.

The phone UI keeps its existing [design system](../../DESIGN.md) and
[surface brief](web-device-ui-index-html.md); its web palette and system fonts
do not replace this embedded bitmap surface.

## Evidence

The [display component guide](../../firmware/esphome/components/openathan_display/README.md)
owns runtime behavior and fixture rendering. The
[dated validation report](../../docs/development/waveshare-countdown-validation-2026-10-09.md)
records automated checks, matched firmware capacity and remaining hardware work.
No generated mockup is bundled in firmware. Rendered bounds and a successful
build do not establish physical readability or concurrent runtime headroom.
