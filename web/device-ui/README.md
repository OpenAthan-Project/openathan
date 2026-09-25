# OpenAthan device UI

The reference firmware embeds this responsive interface and serves it locally
at its unique `http://openathan-<suffix>.local/` address, with an IPv4 fallback.
It includes first-run manual location/timezone/calculation setup, prayer and
volume settings, schedule previews, status, stop, skip and cancel-skip controls.
Assets work without the public website or a CDN. Device access uses the password
chosen over USB and browser-native Digest login with username `admin`.
The isolated acceptance build displays a prominent **Test firmware** banner;
its authenticated status identifies it with `test_mode: true`.

The firmware's existing settings service owns validation, persistence and
revision checks. First-run activation is explicit, and incomplete setup does not
consume prayer history. The interface preserves unsaved edits during status
refreshes and reads back uncertain saves without repeating them.

See [provisioning and the local API](../../firmware/esphome/provisioning/README.md)
for build instructions, the USB console, recovery, API contracts, tests and
remaining hardware acceptance. `index.html`, `app.js`, and `style.css` are
compressed into firmware during code generation; no public website build is
involved. The npm dependency is for browser testing only.

Quran/adhkar, LED settings, product OTA, and the public browser installer remain
separate work.
