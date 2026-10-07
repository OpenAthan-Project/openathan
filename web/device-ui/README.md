# OpenAthan device UI

The reference firmware embeds this responsive interface and serves it locally
at its unique `http://openathan-<suffix>.local/` address, with an IPv4 fallback.
Today leads with the device’s authoritative next prayer, its local time and
readiness, followed immediately by volume and named Skip/Restore controls. The
highlight stays on that occurrence when skipped. An occurrence outside today’s
timetable has a dated hero and no highlighted row. Connection loss freezes the
last observed information and marks it stale; it does not assert playback stopped.
Stop stays reachable from Today and Settings while playback is reported active.

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

Location, timezone, calculation conventions, high-latitude rules and offsets remain
a separate draft until **Preview timetable → Confirm prayer changes**. Timezone
rules come from confirmed settings; same-zone saves preserve the current rules
unless the owner explicitly requests a refresh. First run
follows location → calculation → review → Finish setup. Manual setup may finish
while waiting for a valid clock, with an explicit warning; helper proposals require
a ready preview. Drafts survive Today/Settings navigation and device-status refreshes; browser navigation
warns before discarding prayer edits. Back, Discard, draft changes and changed saved
calculations invalidate pending previews, including late errors. Only one current
preview can be submitted. Storage recovery into incomplete setup opens the setup
view without replacing drafts; repeated polls retain focus. Fresh revision-one
configuration requires choosing a location. Recovery into active setup keeps the view.
When another client completes setup, untouched fields adopt the accepted saved
configuration, even without a revision change. Actual local drafts are retained.

Preference writes merge into confirmed settings, preserving coordinate precision
and excluding calculation drafts. Settings, display, lights and time format keep
their existing independent revision contracts. The write scheduler serializes
writes; Stop remains independent, and a blocked domain does not prevent healthy
independent preferences from saving.
Successful contact resumes eligible queued preferences; blocked domains and
unreleased drags retain their existing recovery and release requirements.
Uncertain writes require readback before another write in that domain.
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
also preserve newer observations. Older responses cannot clear newer storage
faults, overwrite newer healthy values or acknowledge edits prematurely. Stale
confirmations retain edits and require fresh saved-state recovery. An older update
poll cannot hide a newer available update. A readable storage fault stays connected,
retains last confirmed values and disables only affected controls. Unreadable stores
without confirmed values show unknown values. Prayer drafts survive faults, and
confirmed time format stays in use during unresolved edits. No settings defaults,
playback identity or stopped playback are inferred from missing information.

Assets work without the public website or a CDN. Device access uses the password
chosen over USB and browser-native Digest login with username `admin`.
An optional **Find my location** link opens the public HTTPS helper in a new tab.
It can suggest browser coordinates or an approximate IP location, then returns
proposed values in a versioned URL fragment. The fragment is checked and removed
from the address bar; only supported timezones are suggested. The device requires
a ready timetable preview before a returned location can be saved. If its clock
has not synchronized, wait and preview again. Manual entry remains available
without the helper or internet access.
If status or the timezone list is temporarily unavailable, the proposal waits
in the open page; **Refresh** retries both reads.
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

Linux requires OpenSSL development headers. To use a different build directory,
set `OPENATHAN_DIGEST_TEST_BRIDGE` to the adapter's absolute path.

See the [light contract](../../docs/development/lights.md) for countdown/status
colors, separate persistence, and LED hardware acceptance.

The [firmware upgrade flow](../../docs/development/firmware-upgrades.md) checks
for stable releases and queues owner-requested application updates between prayers.
The authenticated **Updates** group provides check, install, cancel and
reconnect/status controls. The public USB installer remains a fresh-install and
credential-recovery tool. Quran/adhkar and owner-facing audio replacement remain
future work.

The [redesign validation report](../../docs/development/device-ui-redesign-validation-2026-10-05.md)
retains integration evidence. The [holistic review report](../../docs/development/device-ui-holistic-review-2026-10-06.md)
records current browser/API coverage, the finding-to-test matrix, matched OTA sizes
and runtime limitations.
Browser fixtures are simulated; they do not establish physical-device behavior.
