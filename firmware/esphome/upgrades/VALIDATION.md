# Attended firmware upgrade qualification

This maintainer procedure tests the reference AtomS3R C126 + Voice Pyramid A167
with its retained recordings. It temporarily replaces the normal schedule with
isolated test operation. Coordinate a listener and an operator for physical
power cuts before accessing the device. Use the
[provisioning preflight](../provisioning/HARDWARE_TEST.md) and
[scheduler runbook](../scheduler/VALIDATION.md) for identity, storage and playback
checks. Automated tests and successful development builds do not establish
physical acceptance or public-release readiness.

The qualification configuration uses the ordinary provisioning test namespaces,
including `oa_upgrade_test`, a fixed private LAN HTTPS origin, a private test CA
and a separate P-256 signing key. Production trust, URLs and automatic checks
remain unchanged. Ordinary provisioning test firmware still cannot install
production releases. Qualification firmware is rejected by release packaging.
No public release, `latest` selection or website change is part of this procedure.

All instrumentation and test-specific behavior live in the separate qualification
component. The production updater is the shared engine, connected by hooks that
compile only into qualification builds. Production reconciliation/persistence
retain their original behavior; repeated-hold adjustments belong to the test
adapter. Build checks verify definitions, compiled-source exclusion and payloads;
release packaging rejects qualification and ordinary isolated test images.

## Build and review before hardware access

Run the [pinned CI checks](../../../.github/workflows/README.md), updater host
tests and [browser checks](../../../web/device-ui/README.md). Rebase onto current
`main`, review the final diff and open a draft PR; require its exact-head checks
to pass before installing a candidate. Keep source, dependency locks, before/after
OTA byte counts and hashes in the dated validation record.

Create a new private directory outside the repository. Replace the example IP
with the build host's reachable RFC1918 IPv4 address. Keys expire; generate a new
profile and rebuild all candidates if the origin or trust material changes.

```sh
export QUALIFICATION_DIR=/tmp/openathan-private-upgrade-session
python tools/upgrade_qualification.py init \
  --directory "$QUALIFICATION_DIR" --address 192.168.1.2 --port 8443
```

Copy only the returned origin and **public** CA/key paths into the ignored
`firmware/esphome/upgrades/secrets.yaml`:

```yaml
qualification_origin: 'https://192.168.1.2:8443'
qualification_ca_file: '/tmp/openathan-private-upgrade-session/ca.pem'
qualification_public_key_file: '/tmp/openathan-private-upgrade-session/public-key.pem'
```

Build four immutable candidates from the reviewed commit. The failed-startup
switch must be true only for v0.0.3. Use distinct build directories. The
`OPENATHAN_BUILD_COMMIT` environment variable embeds source identity.

```sh
export OPENATHAN_BUILD_COMMIT=$(git rev-parse HEAD)
for version in v0.0.1 v0.0.2 v0.0.3 v0.0.4; do
  failure=false
  if [ "$version" = v0.0.3 ]; then failure=true; fi
  ESPHOME_BUILD_PATH="/tmp/openathan-upgrade-$version" python -m esphome \
    -s qualification_version "$version" -s qualification_startup_failure "$failure" \
    compile firmware/esphome/upgrades/qualification.yaml > "/tmp/openathan-upgrade-$version.log" 2>&1
  python tools/check_feasibility.py \
    --build-dir "/tmp/openathan-upgrade-$version/openathan-test" \
    --log "/tmp/openathan-upgrade-$version.log" --audio-image "$APPROVED_AUDIO_IMAGE"
  python tools/upgrade_qualification.py stage --directory "$QUALIFICATION_DIR" \
    --application "/tmp/openathan-upgrade-$version/openathan-test/build/firmware.ota.bin" \
    --version "$version" --commit "$OPENATHAN_BUILD_COMMIT"
done
```

`APPROVED_AUDIO_IMAGE` must be the independently validated, approved shared image
already on the speaker. The application contains no recordings. CI fixtures are
compile-only and must never be installed or played. Also prepare the production
application from clean, merged `main` with the same pinned toolchain; bind that
source/hash to the restoration plan. Do not install an unmerged production build
as the final production image.

The feed serves only signed descriptors and these four application files. It
does not expose private keys, directory listings or arbitrary files. Keep the
session directory mode 700 and files mode 600. Never put keys, USB snapshots,
credentials or raw device responses in Git.

## Fresh scoped USB transition

Disconnect Pyramid bottom power before connecting the Atom USB-C data port.
Redetect the serial port; verify the expected device MAC, ESP32-S3, 8 MiB flash,
disabled secure boot/encryption and the exact partition table. Record current
settings/history/credentials/light preferences and the full shared-audio hash.
`IDF_PATH` must point to the resolved ESP-IDF **5.5.5** source directory.

The USB helper enters ROM and leaves the device stopped. It reads each scoped
region twice, validates NVS CRCs/records and saves the current selected
application as private recovery evidence. It does not create an 8 MiB full-flash
backup. Pending or malformed selection metadata blocks a write plan.

```sh
python tools/upgrade_transition.py inspect --port "$DEVICE_PORT" \
  --expect-mac "$DEVICE_MAC" --idf "$IDF_PATH" --directory "$SESSION_BASELINE"
python tools/upgrade_transition.py plan --snapshot "$SESSION_BASELINE" \
  --application /tmp/openathan-upgrade-v0.0.1/openathan-test/build/firmware.ota.bin \
  --bootloader "$REVIEWED_BOOTLOADER_32K" --output "$TRANSITION_PLAN"
```

Extract `REVIEWED_BOOTLOADER_32K` as the first 32 KiB of the matching factory
image. Its complete SHA-256 must match `release/rollback-bootloaders.json`.
Review the concrete plan's identity, selected slot, source-bound application and
bootloader hashes before applying it:

```sh
python tools/upgrade_transition.py apply --plan "$TRANSITION_PLAN" \
  --port "$DEVICE_PORT" --expect-mac "$DEVICE_MAC" --idf "$IDF_PATH"
```

The helper rereads all scoped regions before any write and rejects stale inputs.
It writes the selected application first, then the reviewed 32 KiB bootloader;
it verifies both by independent readback. Partition table, NVS, OTA selection,
inactive application and shared audio must remain byte-identical during this
transition. It never uses erase-all or automatically retries an uncertain write.
If anything fails, keep the device stopped, inspect the journal and reconcile
the actual state before planning another operation.

Unplug Atom USB and reconnect **bottom-only power** for networking and listening.
Require `OPENATHAN_QUALIFICATION_V1`, `test_mode: true`, v0.0.1 and the recognized
rollback bootloader. Truly erased OTA metadata is initialized only through the
IDF API, followed by restart; corrupt metadata is never repaired. Existing
UNDEFINED/PENDING metadata must become VALID after the health check. Stop on a
storage fault or unexpected reboot. Provision isolated credentials/settings using
the existing USB procedure; production credentials and schedule stay isolated.

For panic diagnosis, qualification firmware permits a descriptor `check` over
Atom-only USB even when Pyramid audio health is unavailable. Installation,
cancellation and qualification controls remain blocked until startup health
confirms; production firmware retains the health gate for checks too. Capture
serial output with DTR/RTS disabled and inspect only descriptors. This diagnostic
exception does not authorize resuming the interrupted installation matrix.

After each completed worker operation, qualification snapshots include
`worker_stack_min_free`, the smallest observed unused stack in bytes across
completed operations since boot. The worker samples its own
[ESP-IDF stack high-water mark](https://docs.espressif.com/projects/esp-idf/en/release-v5.5/esp32s3/api-guides/performance/ram-usage.html#run-time-methods-to-determine-stack-size)
before exiting. Record it with heap/PSRAM observations during diagnostic checks;
a successful response alone does not establish stack or runtime-memory headroom.

## HTTPS, holds and interruption matrix

Start the feed on the selected LAN address. Keep the device clock synchronized
and arrange the calculated test schedule so that upgrades can run safely between
announcements. Do not change the system clock or production prayer history.

```sh
python tools/upgrade_qualification.py offer --directory "$QUALIFICATION_DIR" --version v0.0.2
python tools/upgrade_qualification.py serve --directory "$QUALIFICATION_DIR"
# In another terminal, credentials are requested without echo:
python tools/upgrade_qualification.py observe --host "$TEST_HOST" \
  --output "$PRIVATE_OBSERVATION" --seconds 60
```

Use the authenticated Firmware page to check, install or cancel an offered
version. The observer records timestamped JSON Lines. It can arm a one-shot
hold **before requesting installation**, using `--control arm --phase
before_boot_selection` or `after_boot_selection`. Release with `--control release`
and a fresh output filename. Read actual status after uncertain POST responses;
never repeat a mutation automatically.

Holds keep the normal loop, HTTP and scheduler responsive. Counters distinguish
erase, chunk write, finalization and boot selection attempts. Before-selection
holds persist the handoff but have not selected the candidate; after-selection
holds report `restarting`, reject cancellation and select exactly once. Releasing
a hold still requires a fresh safe window and no playback. Arms disappear on
power loss; they are not durable product controls.

| Case | Required evidence |
| --- | --- |
| Invalid descriptor signature | `offer --fault signature`; failed check, no queue/erase/write/selection change. |
| Corrupt or short image | `--fault corrupt` or `truncate`; old version remains, no selection, request retained/cancellable. |
| Transport interruption | `--fault disconnect` closes a half-length response; separately stop/restart the feed or interrupt the controlled network during a delayed download. Record the actual intervention and recovery. |
| Delayed networking | `--header-delay` or `--chunk-delay` allows observation during blocking operations. Playback/safety/cancellation must prevent the next flash operation. |
| Cancel during download | Cancel after progress begins; no boot selection/restart, read back an empty queue. |
| Power cut before selection | Observe the armed phase and zero selection attempts, then cut bottom power. The old version boots and retains the request for a fresh safe-window attempt. |
| Power cut after selection | Observe the new selected offset and exactly one selection attempt, then cut power. Candidate boots PENDING and later confirms; repeated holds never reselect. |
| Healthy v0.0.2 | Running slot switches from v0.0.1, becomes VALID after startup health, durable queue clears, settings/test history/audio remain intact. |
| Pending-boot interruption | Cut before the candidate's 30-second confirmation. Bootloader rejects it and returns to the prior VALID application; capture actual OTA state/outcome. Timing is an observed physical interval, not proof of an exact instruction boundary. |
| Failed v0.0.3 startup | Injected health failure prevents confirmation; after 90 seconds the device rolls back to v0.0.2 and reports the rejected request. No operator cut is needed for this case. |
| Healthy v0.0.4 | After rollback, a fresh request installs and confirms the other slot without resetting records or recordings. |
| Queue restoration | Restart before a download and at a pre-selection hold; durable version/signature remain bound to the request. Cancel clears it. |

Complete at least **ten recorded interrupted attempts** while an older VALID
application is retained. Repeat transport, cancellation and pre-selection cases
before accepting v0.0.2; repeated v0.0.3 pending/failure cases can then roll back
to v0.0.2. Record each cut's observed phase and result; stop after an unexpected
reset, storage/preservation fault or audio regression.

On the healthy final test image, run a **60-minute bounded soak** with ordinary
HTTP/status polling, repeated permitted checks and full retained-recording
playback at a comfortable volume. Record internal free/minimum/largest-block
heap, PSRAM free/largest-block, reset reasons, audio completion and listener
observations. Compare idle recovery after plays and attempts; investigate any
progressive loss or allocation failure. Static RAM and build size do not prove
runtime headroom. This bounded session does not qualify an overnight/long soak.

## Preservation and production restoration

Enter ROM again and take a **fresh** scoped snapshot at major checkpoints and
before restoration. Compare protected production namespace presence and payload
hashes plus the complete shared audio against the original stopped baseline:

```sh
python tools/upgrade_transition.py compare \
  --baseline "$SESSION_BASELINE/snapshot.json" --current "$FINAL_TEST_SNAPSHOT/snapshot.json"
python tools/upgrade_transition.py plan --snapshot "$FINAL_TEST_SNAPSHOT" \
  --application "$MERGED_PRODUCTION_OTA" --output "$RESTORATION_PLAN"
python tools/upgrade_transition.py apply --plan "$RESTORATION_PLAN" \
  --port "$DEVICE_PORT" --expect-mac "$DEVICE_MAC" --idf "$IDF_PATH"
```

Production restoration rejects both qualification and ordinary isolated test
images, even when their embedded version matches production. The planner and
apply-time revalidation use the same test-material filter as release packaging;
an older plan cannot bypass it before scoped flash reads, writes or a write
journal starts.

The restoration writes only the freshly selected application slot and retains
the reviewed rollback bootloader. Verify production hostname/version/source,
`test_mode: false`, normal settings revision and light preferences, valid clock,
healthy future schedule and retained audio. Production consumption watermarks
must never move backward; natural progress after scheduling resumes is expected.
Observe no catch-up/replay and one real upcoming Athan with attended listening.
Do not restore historical NVS or full-flash images. Keep isolated evidence until
the results are reconciled; any cleanup is a separate bounded operation.

Save a dated private report with source/image hashes, actual cuts, raw observations,
record comparisons, final device state and limitations. Publish only sanitized
results in the linked development report. GitHub-hosted delivery/redirects,
public release selection, broader network/browser coverage and long-soak results
remain pending unless separately exercised and recorded.
