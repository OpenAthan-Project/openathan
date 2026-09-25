# USB provisioning and local settings

`../openathan.yaml` is the buildable reference **development candidate** for the
AtomS3R C126 + Voice Pyramid A167. It includes USB provisioning, a device-hosted
setup/settings page, and first-run activation. The public website and browser
installer are separate work. Release recordings and physical acceptance of this
new image remain pending; compiling this image is not release qualification.

For existing-device acceptance, use the [isolated test build and hardware runbook](HARDWARE_TEST.md).
It keeps test prayer, activation and credential records separate and includes a
USB-only maintenance helper. Production builds reject those maintenance commands.

## Build and developer access

Use the pinned ESPHome environment described in [firmware feasibility](../feasibility/README.md).
This configuration needs no secrets file:

```sh
export ESPHOME_BUILD_PATH=/tmp/openathan-reference
python -m esphome compile firmware/esphome/openathan.yaml
```

USB uses the AtomS3R USB-C data port. Normal audio operation uses the single power
cable through the Pyramid's bottom USB-C. Choose the actual current serial port;
close other serial clients before running the console. The console does not
reset, flash, erase, or automatically repeat a write.

```sh
python tools/provision_device.py --port /dev/cu.YOUR_DEVICE status
python tools/provision_device.py --port /dev/cu.YOUR_DEVICE scan
python tools/provision_device.py --port /dev/cu.YOUR_DEVICE wifi --ssid "Your network"
python tools/provision_device.py --port /dev/cu.YOUR_DEVICE password
```

Passwords are entered using hidden prompts, not command-line arguments. Initial
Wi-Fi supports 2.4 GHz WPA2/WPA3 personal networks, including hidden SSIDs. Enterprise
networks and captive-portal sign-ins are not supported. The device password is
12–128 printable ASCII characters; a long, unique passphrase is recommended.
Open the returned local URL and sign in with username **admin** and that device
password. The hostname has a MAC suffix, e.g. `openathan-a1b2c3.local`; the console
also returns the current IPv4 address. Keep the phone on the same LAN. Guest
network client isolation may prevent access.

Enter latitude/longitude, an IANA timezone, and the calculation method. Review
the preview, then choose **Finish setup**. Default choices for a fresh device
are Standard Asr, automatic high-latitude handling, zero offsets, all five prayers
enabled, and 70% volume. The reference hardware's existing 60% output ceiling
remains. With no valid clock, configuration can be finished but announcements
wait for time synchronization. A cold boot without a usable time source cannot
establish the current time from saved settings alone.

## Installer integration contract

Firmware builds artifacts; a future website task consumes them. No website or
`/install` implementation is included here.

- The device implements Improv Serial version 1 over USB Serial/JTAG, 115200 baud.
  Standard commands are Wi-Fi settings (1), current state (2), device info (3),
  network scan (4), and network state (7). Unknown commands return error 2.
- Scan results are deduplicated using ESPHome's scan policy and capped at 32.
  An empty RPC result ends the list. Scans and Wi-Fi changes are mutually exclusive.
- Wi-Fi provisioning times out after 30 seconds. The previous connection must
  disconnect before a matching new join can authorize storage. Success requires
  a checked `nvs_commit`, then returns hostname and IP URLs. Connection failure
  restores the previous saved credentials. Storage failure is reported explicitly;
  after an uncertain write, read status and restart before another write.
- A single local `improv_serial` external component owns USB input and output.
  It deliberately replaces the stock ESPHome reader **only in this reference
  image**. It handles standard Improv frames and the password extension below.
  Do not instantiate a second reader. The serial logger is silent; consumers
  should nevertheless ignore boot noise and validate framing/checksums.
- A future browser installer must release its serial reader before its password
  client acquires the port. Wi-Fi and password recovery send commands only.
  Failure to recognize firmware must not automatically start an installation.

### OpenAthan USB extension, version 1

Use the Improv serial frame layout and checksum, replacing the six-byte header
`IMPROV` with `OATHAN`. Frames are: header, version (1), type, payload length,
payload, and the low eight bits of the sum of all preceding bytes. An optional
newline follows. Type 3 is an RPC request, type 4 an RPC response, and type 2 a
one-byte error. Payloads are limited to 255 bytes; partial frames expire after
one second. RPC payloads contain command, argument-byte count, then strings,
each prefixed by its one-byte UTF-8 length.

| Command | Request fields | Response fields |
| --- | --- | --- |
| 1: status | None | Protocol `1`, Wi-Fi state, password state (`absent`, `ready`, `fault`), setup state (`incomplete`, `active`, `storage_fault`), password revision, hostname, credential-storage state (`ready`, `fault`), then available URLs |
| 2: set/reset password | New password | `saved`, new password revision |

USB possession authorizes password recovery; the old password is not required.
Credential-storage faults include corrupt records and failed writes; they remain
visible even if the previous Wi-Fi connection has been restored.
The device never returns password material. Every successful reset invalidates
all HTTP authentication nonces. Extension errors use 1 for invalid input, 2 for
unsupported commands, and 255 for storage failure. A response lost after commit
has an uncertain outcome: read status, never blindly repeat the write.

### Flash artifacts and updates

The existing partition contract is unchanged: two 2 MiB app slots, plus shared
audio at `0x410000`, size `0x380000`. A **fresh installation** needs the matching
factory application image and `athan-audio.bin`; the factory image intentionally
ends before shared audio. Validate images with `tools/check_feasibility.py` and
its generated-audio fixture for CI, or approved media for a separately authorized
hardware test.

A compatible ordinary update replaces only an application slot and preserves
NVS and shared audio. The reference candidate does not expose a product OTA UI
or an unauthenticated upload endpoint. A merged factory image can overwrite NVS
even without a full-chip erase. Existing developer devices therefore require a
separately controlled compatible application upgrade, not the fresh-install path.
Historical full-flash backups restore old consumption history and are unsuitable
for routine network/password recovery.

## Local HTTP contract

The local server listens on port 80. Assets and all API endpoints require HTTP
Digest authentication. Requests are transferred to the ESPHome main loop before
settings, scheduler, or credential state is accessed. Bodies are capped at 4096
bytes; queues and authentication replay state are bounded. Timed-out queued work
is cancelled before mutation; uncertain in-progress outcomes require readback.

Use `GET /api/status` for a complete settings snapshot, revision, setup state,
time readiness, application status, current/next playback, skip, and today's
schedule, plus an informational `test_mode` boolean. Fresh incomplete devices do not display a timetable based on seed
coordinates. `GET /api/timezones` returns the supported IANA names and the pinned
tzdata version (2026.4).

All writes use `POST`, `Content-Type: application/json`, and an `Origin` matching
the exact device origin. Host headers must match the device's hostname or current
IPv4 address. No cross-origin API access is enabled.

| Endpoint | Request | Behavior |
| --- | --- | --- |
| `/api/preview` | Complete settings request | Side-effect-free candidate timetable or waiting/invalid status |
| `/api/settings` | Complete settings request | Durable revision-checked save; preserves activation |
| `/api/activate` | Complete settings request | Saves settings first, then durably activates setup |
| `/api/stop` | `{}` | Stops current audio |
| `/api/skip` | Expected revision and displayed occurrence | Skips only that still-current occurrence |
| `/api/cancel-skip` | Expected revision and displayed skip key | Cancels only that selected skip |

Settings requests reuse `{schema: 1, expected_revision, settings}` from
[the settings service](../scheduler/SETTINGS.md). They may additionally include
`refresh_timezone: true`. The server resolves a changed timezone using its bundled
catalog. An unchanged timezone keeps the saved rules unless refresh is explicitly
requested. Thus existing POSIX-named settings remain editable without forcing a
conversion to IANA. Clients cannot substitute arbitrary rules through this API.

The displayed occurrence includes `day` (calculation day), `prayer` (0–4), `utc`,
and optional `shared_with: {day, prayer}`. A skip key contains `day` and `prayer`.
Send the displayed object with `expected_revision`. Changed revisions or
occurrences return 409. Malformed settings return 400; storage failures return
503. Readback distinguishes a durable save from `volume_pending`, `volume_failed`,
or storage failure. Neither application code nor the console repeats uncertain
writes. Browser transport retries remain safe through revisions and occurrence
identities.

### Authentication boundary

The v1 interface uses browser-native RFC 2617 Digest (`MD5`, `qop=auth`) for
Chromium/WebKit compatibility, with a fixed `admin` username and a device-specific
realm. ESP-IDF mbedTLS supplies hashing. The device stores HA1 verification
material, not the plaintext device password. HA1 is password-equivalent and
must remain private. Nonces are random, expire after five minutes, and are checked
against bounded nonce-count replay windows, the actual request method/target,
and the configured realm. Password replacement and reboot invalidate old nonces.

This is a trusted-home-LAN interface, not an encrypted or Internet-facing service.
HTTP content can be observed or modified by an active network attacker, and
Digest permits offline password guessing from captured exchanges. There is no
Basic-auth fallback, remote access, or cloud credential storage. Missing/corrupt
credentials lock local access while an already configured scheduler continues.
Wi-Fi credentials remain recoverable by the device from NVS; flash encryption
and physical tamper resistance are outside this milestone.

## Activation and durable records

The existing `openathan/settings` and `openathan/scheduler` records retain their
formats and replay semantics. New records are separate:

- `oa_setup/state`: versioned activation marker, validated before defaults are seeded.
- `oa_network/wifi`: versioned, CRC-checked Wi-Fi snapshot.
- `oa_network/password`: versioned, CRC-checked password verifier and revision.

Fresh storage is marked incomplete durably before settings seeding. While
incomplete, polling, skip, and cancel-skip cannot consume prayers or alter history.
The finish operation commits settings before activation. Power loss between those
commits leaves setup incomplete. A recognized existing saved-settings installation
without a marker is adopted as active; history is retained. Corrupt/unsupported
records fail closed without erasure. Missing settings alongside existing history
are a fault, not a fresh installation. Generic/developer builds retain their
existing initialization behavior unless the product activation gate is attached.

## Verification and remaining acceptance

Automated checks cover the production settings/JSON API, activation and storage
faults, side-effect-free previews, timezone preservation, revision conflicts,
occurrence-bound skips, Digest replay/expiry, USB framing, credential corruption,
interrupted commits, and the Wi-Fi disconnect/join/timeout state machine. Browser
tests use a simulated device to exercise native Digest login, setup, conflicts,
lost responses, invalid time and narrow-screen layout in Chromium and WebKit.
They do not establish physical iPhone/Android or radio/USB operation.

```sh
cmake -S . -B build -DCMAKE_CXX_FLAGS='-fsanitize=undefined -fno-sanitize-recover=all'
cmake --build build
ctest --test-dir build --output-on-failure
python tools/run_tests.py
# Include the actual resolved ArduinoJson library for production API tests:
OPENATHAN_ARDUINOJSON_INCLUDE=/path/to/build/managed_components/bblanchon__arduinojson/src \
  python -m unittest discover -s tests -p test_settings_adapter.py -v
cd web/device-ui
npm ci --ignore-scripts
npx playwright install chromium webkit
OPENATHAN_TEST_BROWSERS=chromium,webkit npm test
```

Before a hardware rollout, separately verify fresh flash plus approved audio,
USB/network recovery (including same-SSID password changes), cold-power cuts at
setup/save boundaries, actual iPhone Safari/Android Chrome access, audio during
Wi-Fi loss/recovery, and retained history after a compatible application upgrade.
