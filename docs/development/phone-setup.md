# First-run phone setup

This unreleased feature lets a newly installed speaker complete setup from a
phone. The public installer continues its existing USB flow until a supported
firmware release is published and adopted by the separate website repository.

## Journey

1. Power the speaker. Join its open `OpenAthan-Setup-<six hexadecimal digits>`
   Wi-Fi network using the phone's network settings or the displayed Wi-Fi QR.
2. Use the automatic setup popup, or open `http://192.168.4.1/setup/` manually.
   Stay connected despite the phone's no-internet notice.
3. Choose a device password, scan/select home Wi-Fi or enter a hidden SSID, and
   enter its password. A previously saved device password must be supplied again.
4. Read the permanent speaker URL and reconnection instructions before starting
   the connection attempt. The setup network closes while the speaker tries
   home Wi-Fi; a lost response alone does not prove the credentials were saved.
5. Rejoin home Wi-Fi, open the permanent URL and sign in as `admin`. Complete the
   existing location, calculation and timetable-review wizard, then Finish setup.
   The optional display's button/QR view finds the URL again afterward.

The Wi-Fi QR joins a network; it does not also open a webpage. Atom separates
instructions and a full-screen QR. Waveshare combines QR and text inside the
neutral prayer ring. The screen is optional; setup instructions and USB remain
available without it. Captive-popup behavior, physical scan readability and the
phone's network handoff need attended checks on actual devices.

## Enrollment and durable state

Enrollment requires healthy incomplete activation state, healthy credential
storage and an empty Wi-Fi record. A saved password alone may resume enrollment,
but it cannot be replaced through this interface. Active installations, corrupt
records and configured-device outages never start an enrollment network. A
partial USB installation with Wi-Fi saved but no password uses USB to finish.

Initialization waits until ESPHome setup has completed, so the activation store
has been inspected before the AP can open. Password policy and checked storage
are shared with USB: 12–128 printable ASCII characters. Home Wi-Fi keeps the
existing 2.4 GHz personal-network contract; enterprise networks and upstream
captive-portal sign-ins are unsupported.

Save or verify the device password before attempting home Wi-Fi. A failed join
times out after 30 seconds and restores the same setup network. A successful
matching join authorizes the checked Wi-Fi save. The AP closes before the station connection starts,
so the client must receive handoff instructions before the attempt starts.
A save failure restores a read-only error page and blocks further enrollment
writes until restart and record inspection. Neither enrollment nor failure
erases settings, activation, prayer history or shared audio. Prayer activation
remains the existing explicit Finish setup operation.

## Bootstrap interface

The existing HTTP server owns these routes. Setup admission requires the live
enrollment/AP state and an accepted socket addressed to the AP IP. Bootstrap requests are
rejected during station connection attempts or whenever a station IP is present. Setup assets and APIs are unavailable through the station
interface. Safe captive-detection GETs redirect to the fixed setup URL; normal
settings remain behind their existing Host, origin and Digest checks.

| Route | Purpose |
| --- | --- |
| `GET /setup/` | Local HTML, with adjacent compressed JS/CSS assets |
| `GET /setup/status` | State, setup SSID, permanent speaker URL, password-presence flag and error |
| `POST /setup/scan` | Start a bounded scan; no credential change |
| `GET /setup/networks` | Scan state and bounded SSID/signal/security results |
| `POST /setup/connect` | Supply device password, SSID and Wi-Fi password; start an asynchronous attempt |

POSTs require same-origin JSON. Accepted connection requests return 202 with
handoff information. Invalid input returns 400, an incorrect existing device
password 403, a busy transaction 409, rate limiting 429 and storage failure 503.
USB and phone provisioning share transaction exclusions. No passwords appear in
status responses, display snapshots, URLs or QR payloads.

The AP network and its HTTP traffic are unencrypted. Anyone nearby can join while
enrollment is available. It is intended for initial setup in a trusted place;
normal credential recovery remains USB. The bounded local DNS responder serves
the AP only and does not forward traffic or require internet access. The reusable
scheduler and generic ESPHome package acquire no AP or device-UI dependency.

## Validation

Automated acceptance covers enrollment gates and interface isolation, bounded DNS
packets, credential validation and storage failures, retry/interruption states,
display navigation and decoded QR payloads, and Chromium/WebKit bootstrap flows.
Firmware acceptance includes matched pinned builds, capacity checks and preserved
partition/update contracts. Static RAM and compilation do not prove runtime heap
or PSRAM headroom.

The [dated validation report](phone-setup-validation-2026-10-15.md) records the
matched firmware measurements, automated results and simulated UI previews.

Before hardware claims, follow the [provisioning hardware runbook](../../firmware/esphome/provisioning/HARDWARE_TEST.md)
in a separately coordinated session with isolated records and compatible
application-only recovery. Check fresh boot, power interruption, password-only
resume, incorrect Wi-Fi retry, successful save, no-AP configured outages, both
screen layouts, iPhone/Android captive popup and manual fallback, and the final
authenticated settings handoff. Do not install or play synthetic CI fixtures.
