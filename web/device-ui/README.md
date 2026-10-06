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
time format and updates. The markup follows that phone section order so keyboard
and screen-reader navigation reach prayer settings directly after preferences.
Desktop grid placement retains the two columns. First run keeps preference and
update controls hidden until setup finishes. Volume, enabled prayers, screen/light
preferences and 12/24-hour format save automatically. Sliders display input
immediately, save on release and coalesce keyboard adjustments. Save feedback and recovery actions
appear beside the affected group. Time format, lights and display have independent
revision domains; prayer preferences share the existing settings revision.

Location, timezone, calculation conventions, high-latitude rules and offsets stay
in a draft until **Preview timetable → Confirm prayer changes**. First run guides
location → calculation → review → Finish setup. Manual setup can finish while
waiting for a valid clock, with an explicit warning. Drafts survive Today/Settings
navigation and refreshes. If storage recovery reveals incomplete setup, the page
opens the setup view; fresh revision-one configuration requires choosing a
location, while existing drafts are kept. Repeated incomplete-state polls retain
the current setup focus. Recovery into active configuration keeps the current view.
Browser navigation warns before discarding prayer edits.
Automatic preference writes start from confirmed settings, retaining coordinate
precision and excluding calculation drafts. Writes are serialized, with Stop
available independently; uncertain outcomes require readback before more writes
in their revision domain. Failed/conflicting edits remain available for recovery.
Recovery compares newly confirmed calculation settings before replacing the saved
snapshot, invalidating a prayer preview when another client changed calculations.
Delayed Skip/Restore replies and readbacks preserve playback status confirmed by a
newer Stop. A successful status response reporting unreadable prayer storage is a
device fault, not a lost connection: prayer edits and previews are disabled,
drafts are retained, and independent preferences and reported playback Stop remain
available. No prayer defaults are substituted for unavailable saved settings.
Discard cancels a reviewed prayer change while it is queued and unsent, preserving
automatic preference edits. After transmission, Discard waits for confirmation or
an explicit recovery choice; it cannot cancel a write already received by the speaker.
A delayed Stop reply or readback preserves newer observed Skip/Restore and
playback state. Complete status responses also retain newer independently saved
screen, light and time-format revisions and newer firmware-check results.
A Skip queued behind conflicted or faulty prayer settings waits for that domain's
recovery while healthy independent preferences continue saving. Later optional
storage faults retain confirmed values and pending edits, disable only the
affected controls/recovery actions, and show nearby storage feedback. Unreadable
stores with no confirmed values do not substitute defaults. A delayed save reply
cannot clear a newer storage fault or discard its edits before fresh readback.
This ordering guard also covers prayer settings, including delayed save readback
and Skip responses after a newer Stop observation. Rejected stale confirmations
retain pending preferences and reviewed prayer drafts without reporting connection
loss. Recovery checks fresh saved state before confirming an already committed write.
At the same prayer-settings revision, older complete replies retain the newer
observed occurrence, timetable, device date, readiness and Skip state. Action
feedback follows that accepted state when the prayer changes during a request.
Stale screen, light and time-format readbacks also keep the observed connection
and active Stop controls available while requiring fresh saved-state recovery.
Save acknowledgments in all four revision domains also protect newer healthy
snapshots and application feedback. Stale replies/readbacks retain edits and
require fresh readback before confirmation; newer conflicting revisions offer
Retry or Use saved values. The confirmed time format remains in use while an edit
is unresolved. New calculation observations invalidate a preview even during a
preference save, retaining the prayer draft for fresh review.

Recovery resolves the edits present when its readback starts. Later slider,
prayer-toggle, light-toggle and time-format edits are retained; released changes
save with the freshly confirmed revision, while an unfinished drag waits for
release and stays labeled unsaved. Choosing saved prayer settings retains a
draft edited during the readback for a new preview. Focused sliders follow the
latest confirmed value when idle, so the next keyboard adjustment starts from
that value; ongoing input keeps its edited value across status polling.

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
records automated browser/API coverage, matched OTA sizes and runtime limitations.
Browser fixtures are simulated; they do not establish physical-device behavior.
