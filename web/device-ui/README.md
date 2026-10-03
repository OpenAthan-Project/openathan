# OpenAthan device UI

The reference firmware embeds this responsive interface and serves it locally
at its unique `http://openathan-<suffix>.local/` address, with an IPv4 fallback.
It includes first-run manual location/timezone/calculation setup, prayer and
volume settings, schedule previews, status, stop, skip and cancel-skip controls.
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
The authenticated **Firmware** section provides check, install, cancel and
reconnect/status controls. The public USB installer remains a fresh-install and
credential-recovery tool. Quran/adhkar and owner-facing audio replacement remain
future work.
