# OpenAthan device UI

The reference firmware embeds this responsive interface and serves it locally
at its unique `http://openathan-<suffix>.local/` address, with an IPv4 fallback.
This README owns the current behavior and browser-testing contract. The
[design system](../../DESIGN.md) owns visual rules; the
[surface brief](../../.impeccable/surfaces/web-device-ui-index-html.md) records the
approved composition. Dated validation reports retain evidence for their listed
revisions.

## Current behavior

### Prayer and playback observations

Today leads with the device’s authoritative next prayer, its local time and
readiness, followed immediately by volume and named Skip/Restore controls. The
highlight stays on that occurrence when skipped. An occurrence outside today’s
timetable has a dated hero and no highlighted row. Connection loss freezes the
last observed information and marks it stale; it does not assert playback stopped.
Stop stays reachable from Today and Settings while playback is reported active.
The current row matches the device's next prayer and UTC instant, rather than
prayer name or browser time alone. Only the confirmed Today timetable has a
highlight; preview and setup-review tables do not.

`GET /api/status` supplies clock/setup readiness, playback-active state, the next
occurrence, skip and today's timetable. Displayed times use the returned local
strings and confirmed 12/24-hour preference. The optional read-only `local_date`
supplies the date header from the saved device timezone when the clock is ready;
clients tolerate its absence and hide the header rather than use the browser's
date. No playing-prayer identity, playback history or device-accurate countdown
is inferred. Skip/Restore keep the reported day/prayer key and settings revision.

Stop bypasses the preference-write scheduler. An HTTP-successful response that
still reports `playing:true` remains pending; only authoritative `playing:false`
confirms completion. A failed response triggers readback. Failed readback reports
an unconfirmed Stop and warns that playback may remain active. Today and Settings
share that pending, confirmed or unconfirmed feedback.

### Everyday preferences

Settings groups Athan preferences, prayer calculations, optional screen/lights,
time format and updates. Markup follows the phone section order for keyboard and
screen-reader navigation; desktop placement retains two columns. First run hides
everyday preferences and updates until setup finishes.
Fresh unsupported-hardware observations hide the affected controls and suspend
their queued saves. Older responses cannot restore them; edits remain available
if the capability returns. Unsupported defaults are never confirmed preferences.

Volume, enabled prayers, screen/light preferences and time format save automatically.
Both volume controls edit the same value: later input in either view cancels an
older keyboard timer. Sliders display input immediately, save on release and
coalesce keyboard changes for 350 ms. Idle focused sliders follow confirmed values;
an ongoing drag retains its edited value. Feedback and recovery stay beside the
affected group, distinguishing unsaved input, saving, confirmed storage, applying,
unavailable output and failure.

### Prayer drafts and first-run setup

Location, timezone, calculation conventions, high-latitude rules and offsets remain
a separate draft until **Preview timetable → Confirm prayer changes**. Timezone
rules come from confirmed settings; same-zone saves preserve the current rules
unless the owner explicitly requests a refresh. First run
follows location → calculation → review → Finish setup. Manual setup may finish
while waiting for a valid clock, with an explicit warning; helper proposals require
a ready preview. Drafts survive Today/Settings navigation and device-status refreshes; browser navigation
warns before discarding prayer edits. Back, Discard, draft changes and changed saved
calculations invalidate pending previews, including late errors. Only one current
preview can be submitted. Preview never activates setup or changes durable state;
invalid schedules cannot be confirmed. Setup step changes use native validation
and focus their destination headings. Storage recovery into incomplete setup opens
the setup view without replacing drafts; repeated polls retain focus. Fresh revision-one
configuration requires choosing a location. Recovery into active setup keeps the view.
When another client completes setup, untouched fields adopt the accepted saved
configuration, even without a revision change. Actual local drafts are retained.

### Saving, recovery and response ordering

Preference writes merge into confirmed settings, preserving coordinate precision
and excluding calculation drafts. Settings, display, lights and time format keep
their existing independent revision contracts. The write scheduler serializes
writes; Stop remains independent, and a blocked domain does not prevent healthy
independent preferences from saving.
Successful contact resumes eligible queued preferences; blocked domains and
unreleased drags retain their existing recovery and release requirements.
Uncertain writes require readback before another write in that domain.
An unconfirmed Skip or Restore holds further prayer-settings and Skip/Restore
writes until status readback. Once contact returns, screen, lights and time-format
saves remain eligible under their own revision, recovery and release rules.
Recovery offers Check saved state, Retry with my
edits or Use saved values. Recovery updates only its own groups. It resolves
captured edits while preserving later edits,
including changes returning to the same value and unreleased drags.
Use saved values also discards captured drags that have not yet been released;
releasing them afterward cannot save the discarded value. Calculation
conflicts require fresh preview/confirmation. Discard cancels an unsent reviewed
prayer change while keeping automatic preferences. With no preference edits or
settings operation remaining, it releases the canceled review's block and restores
Preview and Skip. Remaining preference edits keep their recovery controls. Sent
changes require confirmed outcomes or an explicit recovery choice.

All response paths use the same acceptance rules. Durable values follow their
revisions; playback, timetable, readiness, application feedback and update status
also preserve newer observations. Timetable, upcoming occurrence, local date and
prayer readiness stay aligned with the accepted settings revision. A delayed
lower-revision Stop reply can confirm playback independently without replacing
those prayer observations. A newly accepted settings revision supplies its own
timetable; same-revision responses retain request freshness. Older responses cannot
clear newer storage faults, overwrite newer healthy values or acknowledge edits prematurely. Stale
confirmations retain edits and require fresh saved-state recovery. Reconnect uses
the same policy: an older reconnect response cannot confirm a newer unanswered
Skip or Restore, even after Stop has confirmed an earlier action. Fresh status
readback is still required. An older update
poll cannot hide a newer available update. A readable storage fault stays connected,
retains last confirmed values and disables only affected controls. Unreadable stores
without confirmed values show unknown values. Prayer drafts survive faults, and
confirmed time format stays in use during unresolved edits. No settings defaults,
playback identity or stopped playback are inferred from missing information.

### Local access and optional location helper

Assets work without the public website or a CDN. Device access uses the password
chosen over USB and browser-native Digest login with username `admin`.
An optional **Find my location** link opens the public HTTPS helper in a new tab.
It can suggest browser coordinates or an approximate IP location, then returns
proposed values in a versioned URL fragment. A proposal does not replace a dirty
prayer draft without an explicit choice. The fragment is checked and removed
from the address bar; only supported timezones are suggested. The device requires
a ready timetable preview before a returned location can be saved. If its clock
has not synchronized, wait and preview again. Manual entry remains available
without the helper or internet access.
If status or the timezone list is temporarily unavailable, the proposal waits
in the open page; **Refresh** retries both reads. Skip to content and other local
anchors preserve that pending proposal. A newer helper fragment replaces it;
unrelated fragment navigation does not replay an already applied suggestion.
The published reference firmware uses the deployed `https://openathan.com/location/`
helper. Forks changing that URL must deploy a compatible helper before shipping.
The isolated acceptance build displays a prominent **Test firmware** banner;
its authenticated status identifies it with `test_mode: true`.

The firmware's existing settings service owns validation, persistence and
revision checks. First-run activation is explicit, and incomplete setup does not
consume prayer history. The interface preserves unsaved edits during status
refreshes and reads back uncertain saves without repeating them.

See [provisioning and the local API](../../firmware/esphome/provisioning/README.md)
for build instructions, the USB console, recovery, API contracts and tests.
The [current release summary](../../docs/development/release-validation-2026-10-03.md)
records physical evidence and its limits. `index.html`, `app.js`, and `style.css` are
compressed into firmware during code generation; no public website build is
involved. The npm dependency is for browser testing only.

## Browser validation

Browser tests use the production C++ Digest verifier with simulated settings
endpoints. Build its host adapter from the repository root before running tests:

```sh
cmake -S . -B build
cmake --build build --target digest_test_bridge
cd web/device-ui
npm ci --ignore-scripts
npx playwright install chromium webkit
OPENATHAN_TEST_BROWSERS=chromium,webkit npm test
```

Tests control races with response gates and browser clocks. Startup waits for
rendered settings or setup; subsequent waits observe each scenario's confirmed
values, recovery choices, playback state or preview controls. Completed requests
alone do not establish that the UI applied a response. No global list of pending
feedback phrases or additional test-only controller state is needed.

Linux requires OpenSSL development headers. To use a different build directory,
set `OPENATHAN_DIGEST_TEST_BRIDGE` to the adapter's absolute path.

See the [light contract](../../docs/development/lights.md) for countdown/status
colors, separate persistence, and LED hardware acceptance.

The [firmware upgrade flow](../../docs/development/firmware-upgrades.md) checks
for stable releases and queues owner-requested application updates between prayers.
The authenticated **Updates** group provides check, install, cancel and
reconnect/status controls. The public USB installer remains a fresh-install and
credential-recovery tool, with a separately gated preserving USB updater for
capable firmware. While a USB request is active, the local Updates group displays
transfer, interruption or owner power-handoff status and hides network cancellation;
firmware rejects competing mutations. See the [USB protocol](../../docs/development/usb-firmware-updates.md).
Quran/adhkar and owner-facing audio replacement remain
future work.

The [redesign validation report](../../docs/development/device-ui-redesign-validation-2026-10-05.md)
retains integration evidence. The [holistic review report](../../docs/development/device-ui-holistic-review-2026-10-06.md)
records its browser/API coverage, finding-to-test matrix, matched OTA sizes and
runtime limitations. The [maintenance validation report](../../docs/development/device-ui-maintenance-validation-2026-10-07.md)
records the subsequent controller organization, scenario-specific test waits and
matched capacity measurements.
Browser fixtures are simulated; they do not establish physical-device behavior.
