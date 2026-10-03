# Firmware upgrades

Official reference firmware can check for published stable releases and install
application-only updates from its authenticated device page. Prayer calculation,
scheduling and recordings continue to work independently of the release service.
The flow is included in published pre-1.0 reference releases. See the
[current production/public evidence and limits](release-validation-2026-10-03.md)
and [source-bound qualification results](upgrade-qualification-2026-10-01.md).
The [maintainer qualification runbook](../../firmware/esphome/upgrades/VALIDATION.md)
uses isolated records and a private HTTPS feed to exercise interruption and rollback.

## Owner flow

The Firmware section shows the installed version, the available release, its
release notes and the last successful check. The device checks after networking
and clock synchronization, then approximately daily. **Check for updates** also
starts a check. A failed check retains the current application and does not pause
the scheduler.

**Install update** durably queues that specific offered release. Closing the page
or restarting does not cancel the request. Installation waits until playback ends
and the next announcement is at least 15 minutes away. An activated device must
have a healthy schedule and a valid clock. **Cancel queued update** cancels before
boot selection, including during a download; the old application remains active.

The worker streams into the inactive application slot. Interrupted downloads
restart from the beginning after a five-minute retry delay and a fresh safe-window
check. If playback starts or scheduling becomes unsafe, staging stops. A verified,
staged image waits for the final safe-window check before boot selection/restart.
The page reconnects and reads actual device status instead of assuming success
from a disconnected HTTP response. Failed startup is reported as restored firmware.
Browser storage is an optional reconnect hint; denied reads/writes do not block
settings or update controls, and device status remains authoritative.
Safety and cancellation are rechecked after blocking network operations and
immediately before flash erase/write/finalization. If the worker cannot be created,
the durable request remains queued and cancellable, with a five-minute retry delay.
Failed OTA starts release any handle allocated before an erase failure; retries
retain the request without accumulating those allocations.

Existing settings, credentials, prayer consumption/skip records, light preferences
and shared recordings are preserved. Upgrade bookkeeping has its own NVS namespace
(`oa_upgrade`, or `oa_upgrade_test` in isolated builds). Storage failures fail
closed without erasing or restoring existing records.

## Local API

All endpoints use the existing Digest authentication and same-origin policy.

| Endpoint | Input | Result |
| --- | --- | --- |
| `GET /api/firmware` | None | Installed identity, offered release, queue/progress, last check and outcome |
| `POST /api/firmware/check` | `{}` | Starts an asynchronous check |
| `POST /api/firmware/install` | `{"version":"vX.Y.Z"}` | Persists the exact offered version |
| `POST /api/firmware/cancel` | `{}` | Cancels before boot selection |

`GET /api/status` includes the same state as `firmware`. Conflicting actions return
409; unavailable initialization/storage returns 503. An uncertain POST response
must be reconciled with a GET, never automatically repeated.

## Release contract and trust

Keep the existing five fresh-install assets and schema-1 installer manifest.
Upgrade-capable releases add `firmware.ota.bin` and `upgrade.json` to the bundle
and checksum list. The OTA application must equal the factory application's bytes.
The website importer verifies its selected manifest, factory and audio assets;
extra upgrade assets do not alter the fresh-install manifest. Its automatic
policy adopts published stable latest after validation, with a manual pin or
disable policy available. This does not install updates on existing speakers.

`upgrade.json` contains `payload` (the exact signed JSON string) and `signature`
(lowercase hexadecimal DER ECDSA signature). SHA-256/P-256 verification uses the
committed public key. The payload binds schema, stable version, exact source
commit, hardware/layout, settings/audio format compatibility, rollback requirement,
application byte count and SHA-256. The firmware derives release asset URLs from
the verified tag; callers cannot supply URLs. TLS certificate verification is
mandatory, and redirects are restricted to GitHub release-storage HTTPS hosts.
Each validated URL (at most 4,096 bytes) gets an HTTP transmit buffer of its
length plus 512 bytes, capped at 4,608 bytes. ESP-IDF formats the request line
and the first headers together; the extra space accommodates GitHub's long
signed asset queries and the default headers. The previous redirect client is
released before allocating the next one. The four-redirect limit and four-second
HTTP timeout still apply. Allocation or transport failure retains the running
application; a queued installation remains subject to the existing retry policy.
See the [download regression and capacity results](upgrade-download-validation-2026-10-02.md).

`release/firmware.json` owns the candidate version and format identifiers. Keep
the official YAML project version aligned. `tools/release.py build` embeds its
exact source commit; ad-hoc builds identify their commit as `development`.
Do not publish a candidate merely because it declares a stable-format version.

Generate/manage the private P-256 key outside Git and outside release bundles.
Pass its path to `tools/release.py package --signing-key PATH` alongside the
existing packaging arguments. Packaging rejects a key that differs from the
committed trust key, an embedded version mismatch, image corruption or excessive
application size. Draft upload re-verifies the signature against the release's
source commit and retains the existing main/CI/tag gates. Keep the key backed up
privately; changing the embedded trust key requires a separately planned rotation.

## Startup recovery and existing devices

Upgrade-capable official builds enable bootloader application rollback and suppress
ESPHome's immediate application confirmation. After 30 seconds, OpenAthan
confirms only a fault-free scheduler, healthy settings/setup storage, update
storage, required audio and volume initialization, and local
server/credential-storage initialization.
Internet connectivity and clock synchronization are not required for confirmation.
A pending application that fails these checks rolls back after 90 seconds;
crashes/restarts before confirmation are handled by the rollback-enabled bootloader.
Confirmation does not establish audible quality or long-term runtime stability.

The signed request remains durable through boot-slot selection and startup.
A cut before selection retains the queue for a fresh safe-window download.
A selected candidate waits for restart without erasing or selecting another slot.
Startup reports rollback only when the requested inactive image is marked invalid
or aborted by OTA metadata; the running requested version reports success after
health confirmation. Legacy handoff markers without a descriptor require a new
owner request if no actual rejection can be established.
After a preserving USB update, a valid request for the running version or an
older version is fulfilled or superseded after startup health confirmation. Its
bookkeeping is cleared without installing older firmware. Invalid signatures or
failed bookkeeping commits still fail closed; they are not silently discarded.

Older reference bootloaders built without rollback are incompatible with Wi-Fi updates.
The device hashes its entire 32 KiB bootloader region against the committed
rollback-enabled bootloader allowlist and refuses Wi-Fi installation when it does
not match. Keep older compatible hashes when releasing newer firmware; packaging
also checks the factory bootloader against this list. Build identity establishes
the configured capability; physical failure/interruption qualification is still
required before publishing it.

The updater follows GitHub's published stable `latest` release. Draft preparation
keeps `--latest=false`; after physical qualification and explicit publication
approval, select the intended stable release as latest. Publishing assets without
that selection does not make them available through the periodic update check.
On v0.2.0, GitHub asset redirects can make checks fail with **Could not check
for updates**. A preserving USB application update to v0.2.1 or a compatible
successor is required to receive the fix; repeatedly checking the affected
firmware cannot deliver it. The [download regression report](upgrade-download-validation-2026-10-02.md)
documents the request-buffer correction. Do not use the destructive fresh
installer as an update workaround.

An application-only USB transition cannot change that bootloader. Before installing
this feature on an existing speaker, inspect its current bootloader and use a
separately reviewed preservation procedure if a bootloader transition is required.
Do not use the fresh installer, erase NVS, rewrite audio, restore historical prayer
history, or promise startup rollback with the old bootloader.
Isolated diagnostic firmware cannot install production releases and does not
automatically contact the release service.
The separate upgrade qualification configuration replaces production trust with
a private test CA/key and permits only its fixed LAN origin. It cannot be packaged
as a public release.

Use the existing [hardware runbook](../../firmware/esphome/provisioning/HARDWARE_TEST.md)
for fresh preflight and scoped preservation evidence. Qualification must test
successful slot switching, power/network interruption, failed startup, queue
cancellation/restoration and concurrent networking/audio heap/fragmentation.
Synthetic CI audio must never be installed or played. Publication and physical
installation remain separate approvals.
