# Provisioning hardware acceptance runbook

**Preparation is implemented; the full physical matrix remains pending.** Bounded
first-run and USB recovery sessions are recorded in the
[validation report](../../../docs/development/feasibility-report.md#provisioning-hardware-subsets--2026-09-25).
This runbook is
for the existing AtomS3R C126 + Voice Pyramid A167. Execute hardware steps only in
a separately coordinated session. The reference and test images have no product
OTA endpoint: this procedure uses a verified, application-only USB update.
The public website, publishing, media licensing and release qualification are
outside this procedure.

## 1. Build and checkpoint without accessing hardware

Start from a clean checkout of the candidate commit. Run `prepare_ci.py` below in
a disposable checkout; it refuses existing secret files. Activate the
[pinned build environment](../feasibility/README.md). `IDF_PATH` below must point
to the **resolved ESP-IDF 5.5.5 source directory**, not another SDK installation.
Keep public CI audio fixtures separate from the device's existing private audio.

```sh
python tools/prepare_ci.py --output-dir /tmp/openathan-ci-audio
cmake -S . -B build -DCMAKE_CXX_FLAGS='-fsanitize=undefined -fno-sanitize-recover=all'
cmake --build build
ctest --test-dir build --output-on-failure
python tools/run_tests.py

export ESPHOME_BUILD_PATH=/tmp/openathan-acceptance-build
python -m esphome compile firmware/esphome/openathan.yaml > /tmp/oa-production-build.log 2>&1
python -m esphome compile firmware/esphome/provisioning/validation.yaml > /tmp/oa-test-build.log 2>&1
python -m esphome compile firmware/esphome/scheduler/device.yaml > /tmp/oa-developer-build.log 2>&1
python tools/check_feasibility.py --build-dir "$ESPHOME_BUILD_PATH/openathan" --log /tmp/oa-production-build.log --audio-image /tmp/openathan-ci-audio/athan-audio.bin
python tools/check_feasibility.py --build-dir "$ESPHOME_BUILD_PATH/openathan-test" --log /tmp/oa-test-build.log --audio-image /tmp/openathan-ci-audio/athan-audio.bin
python tools/check_feasibility.py --build-dir "$ESPHOME_BUILD_PATH/openathan-feasibility" --log /tmp/oa-developer-build.log --audio-image /tmp/openathan-ci-audio/athan-audio.bin
# Keep the existing synthetic image in a separate output directory:
ESPHOME_BUILD_PATH=/tmp/openathan-synthetic-build python -m esphome compile firmware/esphome/scheduler/validation.yaml > /tmp/oa-synthetic-build.log 2>&1
python tools/check_feasibility.py --build-dir /tmp/openathan-synthetic-build/openathan-feasibility --log /tmp/oa-synthetic-build.log --audio-image /tmp/openathan-ci-audio/athan-audio.bin
OPENATHAN_ARDUINOJSON_INCLUDE="$ESPHOME_BUILD_PATH/openathan/managed_components/bblanchon__arduinojson/src" python -m unittest discover -s tests -p test_settings_adapter.py -v
(cd web/device-ui && npm ci --ignore-scripts && npx playwright install chromium webkit && OPENATHAN_TEST_BROWSERS=chromium,webkit npm test)
```

The production namespace defaults are unchanged. The test component selects all
namespaces at compile time; callers cannot supply namespace overrides:

| Records | Production | Provisioning test |
| --- | --- | --- |
| Settings and consumption/skip | `openathan` | `oa_test` |
| Activation | `oa_setup` | `oa_setup_test` |
| Wi-Fi and password verifier | `oa_network` | `oa_network_test` |

The original synthetic scheduler build retains its separate `oa_validation`
records. The new test image uses **real prayer calculations and the real clock**.
It runs instead of the production schedule while installed. Arrange the testing
window accordingly and restore production operation at the end.

## 2. Coordinated preflight and private evidence

Record the source commit, firmware hashes, dependency locks, tool versions,
operator, wall-clock times and expected device MAC. Use a new evidence directory
outside every repository, with restrictive permissions. Retain exact application
recovery images; an old full-flash image would also restore old prayer history.

```sh
umask 077
export EVIDENCE="$HOME/Documents/OpenAthan-Provisioning-Acceptance-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$EVIDENCE"
git rev-parse HEAD > "$EVIDENCE/source-commit.txt"
python -m esphome version > "$EVIDENCE/esphome-version.txt"
python -m esptool version > "$EVIDENCE/esptool-version.txt"
python -m serial.tools.list_ports
```

Choose the actual current USB port and set `PORT`; never reuse a historical port
without identifying it. Before stopping the installed developer firmware, export
its status with `tools/device_console.py --host HOST --secrets PRIVATE_YAML
--get-settings PRIVATE_JSON --seconds 10`. Record revision, settings, volume,
consumed-through watermarks, skip, upcoming prayer and application status.
For a previously migrated product image use authenticated `/api/status` instead.
These earlier network snapshots are contextual; the stopped-device NVS snapshot
below is the exact comparison baseline.

Connect data through the AtomS3R USB-C port. Use the established single bottom
USB-C power arrangement for audible tests; do not introduce an unverified dual
power arrangement. Close other serial clients. USB reads reset/stop the running
application and are part of this coordinated hardware session.

```sh
python -m esptool --chip esp32s3 --port "$PORT" --after no-reset read-mac
python -m esptool --chip esp32s3 --port "$PORT" --after no-reset get-security-info
python -m esptool --chip esp32s3 --port "$PORT" --after no-reset read-flash 0 0x800000 "$EVIDENCE/before-a.bin"
python -m esptool --chip esp32s3 --port "$PORT" --after no-reset read-flash 0 0x800000 "$EVIDENCE/before-b.bin"
cmp "$EVIDENCE/before-a.bin" "$EVIDENCE/before-b.bin"
```

Stop if MAC, flash size, security mode, image identity or the two reads differ.
Extract artifacts while the application remains stopped:

```sh
python - "$EVIDENCE" <<'PY'
from pathlib import Path
import hashlib, json, struct, sys, zlib
out = Path(sys.argv[1]); flash = (out / 'before-a.bin').read_bytes()
assert len(flash) == 0x800000
regions = {'partition-table.bin': (0x8000, 0x1000), 'before-nvs.bin': (0x9000, 0x5000),
           'before-otadata.bin': (0xe000, 0x2000), 'app0-backup.bin': (0x10000, 0x200000),
           'app1-backup.bin': (0x210000, 0x200000), 'before-audio.bin': (0x410000, 0x380000)}
for name, (offset, size) in regions.items(): (out / name).write_bytes(flash[offset:offset+size])
entries = []
for offset in (0xe000, 0xf000):
    seq, = struct.unpack_from('<I', flash, offset)
    state, crc = struct.unpack_from('<II', flash, offset+24)
    if seq not in (0, 0xffffffff) and crc == zlib.crc32(struct.pack('<I', seq), 0xffffffff):
        entries.append((seq, state))
assert entries, 'No valid OTA selection: stop and investigate'
seq, state = max(entries)
assert state == 2, 'Selected image is not confirmed VALID: stop and investigate'
slot = (seq-1) % 2
report = {'active_slot': slot, 'active_offset': hex(0x10000 + slot*0x200000),
          'sha256': {name: hashlib.sha256((out/name).read_bytes()).hexdigest() for name in regions}}
(out/'baseline.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps({'active_slot': slot, 'active_offset': report['active_offset']}))
PY
python "$IDF_PATH/components/partition_table/gen_esp32part.py" "$EVIDENCE/partition-table.bin" > "$EVIDENCE/partition-table.csv"
python "$IDF_PATH/components/nvs_flash/nvs_partition_tool/nvs_tool.py" "$EVIDENCE/before-nvs.bin" --color never --dump blobs > "$EVIDENCE/before-records.txt"
python "$IDF_PATH/components/nvs_flash/nvs_partition_tool/nvs_tool.py" "$EVIDENCE/before-nvs.bin" --color never --dump none --integrity-check > "$EVIDENCE/before-integrity.txt"
```

Require the live partition entries to match `feasibility/partitions.csv`, and
investigate any integrity errors before proceeding. Compare reconstructed payloads
for every key in `openathan`, `oa_setup`, `oa_network`, and `oa_validation` after
each isolated testing stage. Namespace absence is also part of the baseline.
Compare the labelled blob sections and their bytes, not raw NVS pages: NVS may
relocate records during garbage collection. Do not publish these dumps; they may
contain Wi-Fi credentials and password-equivalent verification material.

## 3. Install the isolated application and establish fresh setup

Set `ACTIVE_OFFSET` from the verified baseline report. Copy the exact
`openathan-test/build/firmware.ota.bin` to the private evidence directory and
record its SHA-256. This runbook deliberately replaces the **currently selected
application slot** over USB without changing OTA metadata; it does not test an
OTA transport or promise automatic rollback. Preserve its full slot backup.

```sh
export TEST_BIN="$ESPHOME_BUILD_PATH/openathan-test/build/firmware.ota.bin"
export ACTIVE_OFFSET="$(python -c 'import json,os; print(json.load(open(os.environ["EVIDENCE"]+"/baseline.json"))["active_offset"])')"
python -m esptool --chip esp32s3 --port "$PORT" --after no-reset write-flash "$ACTIVE_OFFSET" "$TEST_BIN"
python -m esptool --chip esp32s3 --port "$PORT" --after no-reset verify-flash "$ACTIVE_OFFSET" "$TEST_BIN"
```

**Never use `firmware.factory.bin`, `erase-flash`, bootloader writes, partition
writes, or the CI audio fixture on this existing device.** After verification,
physically restart and reconnect USB. Read the returned device identity:

```sh
python tools/provision_device.py --port "$PORT" status
```

Require `openathan-test-<actual suffix>.local`. Set `TEST_HOST` to that exact value.
Before a fresh run, clear only the test records (the explicit operation below
removes test Wi-Fi/password credentials too):

```sh
python tools/provisioning_test.py --port "$PORT" --expect-hostname "$TEST_HOST" inspect
python tools/provisioning_test.py --port "$PORT" --expect-hostname "$TEST_HOST" clear --namespaces oa_test oa_setup_test oa_network_test
```

Require `cleared` and `restart_required`. The image stops playback and rejects
settings/provisioning writes until power is cycled. Readback of an uncertain
cleanup uses `inspect`; never automatically resend it. If it reports restart
required, or cleanup failed, keep testing stopped, restart, inspect the records,
and explicitly repeat cleanup only after reviewing the outcome. Cleanup commits
an incomplete marker first, clears prayer/network records, then clears that marker.
A power cut before the first marker commit can retain the previous test setup;
therefore an uncertain cleanup is never counted as a fresh-start pass.

After restart, expect incomplete setup, absent password, and unprovisioned Wi-Fi.
Provision using the existing console (passwords use hidden prompts):

```sh
python tools/provision_device.py --port "$PORT" scan
python tools/provision_device.py --port "$PORT" wifi --ssid "TEST_NETWORK_SSID"
python tools/provision_device.py --port "$PORT" password
```

Open the returned hostname/IP URL on the same LAN; sign in as `admin`. Require
`test_mode: true`, the **Test firmware** banner, blank required location/timezone/
method inputs, and no automatic announcements or history changes before Finish
setup. Record initial history through `/api/status`, wait through an upcoming seed
schedule occurrence if feasible, and compare it again. Preview must not change it.
To test missing time with working HTTP, cold-start on a controlled test network
that blocks NTP (UDP 123) until that observation is complete; do not change the
system clock. Expect “Waiting for time synchronization.”

## 4. Acceptance matrix

Use the real calculated schedule and comfortable volume. For timed actions,
record the displayed occurrence before acting; wait for that occurrence. Do not
advance the system clock or restore old history to accelerate a test.

| Scenario | Required observation |
| --- | --- |
| Manual setup and preview | Enter latitude/longitude, supported timezone and method; review Asr/prayers/volume/Advanced. Preview is side-effect-free. Finish setup persists settings before activation. |
| Invalid time | Preview waits; activation may finish but playback remains blocked. After NTP returns, only future occurrences arm. |
| Wi-Fi failure/retry | Wrong password and nonexistent SSID fail; previous working credentials survive reboot. Correct retry succeeds only after durable acknowledgment. |
| Connected Wi-Fi discovery (pending) | While connected to the saved network, run the USB `scan` command with another visible network nearby. Require the alternative SSID in the results and normal scan completion; repeat while disconnected. Scanning must not replace saved credentials or change settings/history. Retain results privately; radio behavior remains unqualified until this test is performed. |
| Hidden network / same SSID | Hidden SSID connects; a wrong replacement password for the currently connected SSID cannot succeed using the old connection. |
| USB reconnection | Disconnect during a command; reconnect, inspect status, reconcile before explicitly retrying. No reflash/reset is required for ordinary recovery. |
| Password recovery | Replace password over USB. Old authentication fails; a fresh browser signs in with the new password. Settings, revision and history do not change. |
| Phone browsers | Record phone OS/browser versions. Safari/Chrome load all assets locally using both hostname and IP; banner, preview, controls and keyboard layout remain usable. |
| Concurrent edits | Two browsers edit the same revision. The second save reports conflict and preserves edits for review. |
| Uncertain response | Interrupt the connection during Save. Reload; accept a complete old or new snapshot with matching revision, never mixed fields or an automatic second mutation. |
| Timezone and DST | Verify decoded rules and preview against the expected local date; unchanged timezone keeps rules, explicit refresh uses bundled rules. Do not change the device clock. |
| Volume and playback | Save volume and wait for `applied`; confirm audible change at a comfortable level. Stop ends audio. Full normal/Fajr playback and concurrent HTTP/Wi-Fi operation need listener and stability observations. |
| Occurrence-bound skip | Skip the displayed event, restart and verify its identity; cancel that skip. A stale page/request must not skip a later event. |
| Power cuts | Cut bottom power before save, around save/activation, and after activation. After restart, incomplete setup stays silent and does not consume events; active setup retains a complete snapshot and cannot replay consumed events. |
| Storage preservation | After each stage, production and old scheduler-test record payloads match the stopped-device baseline; the complete shared-audio hash matches. |

For readback, `curl --digest --user admin "http://$TEST_HOST/api/status"` prompts
for the password. Save responses privately. Distinguish durable settings success
from `volume_pending`, `volume_failed`, and faults. Physical cuts rarely establish
the precise flash commit instant: record acknowledgment timing and observed state;
use the automated fault-injection results for deterministic boundary coverage.

To check records/audio, stop in the bootloader again, read NVS at `0x9000` for
`0x5000` bytes and audio at `0x410000` for `0x380000` bytes with the same read-flash
syntax, then run the same NVS parser and compare the production blob sections.
Always read twice while stopped if the result is used as recovery evidence.
A changed production record, storage fault, unexpected reset, or audio regression
**stops the test**. Preserve evidence and investigate before another write.

## 5. Production migration and restoration

This is a separate coordinated application write after the isolated checks pass.
Capture final test evidence and production-record/audio comparisons first. Read
and validate current OTA metadata again; never assume the slot from an earlier
session is still selected. Use the same application-only USB procedure with
`openathan/build/firmware.ota.bin` and the freshly verified active offset.

Require the production hostname, `test_mode: false`, and no test banner.
Recognized existing `openathan/settings` must be adopted as configured with its
revision and fields unchanged. Missing device-password enrollment must lock local
HTTP access without disabling established scheduling. Enroll the production
Wi-Fi/password through USB, then verify the saved settings and future schedule.
Production credentials are separate from test credentials and will not transfer.

Compare production consumption at startup and after operation: watermarks cannot
move backward; advancing naturally during time spent testing is expected once
production scheduling resumes. Verify no catch-up/replay, unchanged shared audio,
intended volume, and one real upcoming announcement. Record absent capabilities
and unavailable browser/network cases as pending rather than passes.

If recovery is needed, use the captured compatible **application-slot backup**
at its verified address. Never restore historical NVS or a full-flash backup as
routine recovery. Restoring the earlier developer application resumes the retained
production schedule; verify status before leaving it running. Test data can remain
isolated until a later coordinated cleanup; production firmware rejects the test
maintenance commands.

Finish with a private report listing exact commits/image hashes, final firmware,
settings revision, record/audio comparisons, each pass/fail/pending result, physical
power cuts, listener observations and recovery used. No physical result is implied
by this runbook or by successful builds.

## Test-only USB contract

The existing `OATHAN` v1 framing is unchanged. Production rejects both commands:

- `0x70`, no arguments: `test-v1`, exact test hostname, `ready` or `restart_required`.
- `0x71`: exact test hostname followed by `oa_test`, `oa_setup_test`,
  `oa_network_test` in that order. Reject any other set before opening NVS.
  Success returns `cleared`, `restart_required`; invalid requests use error 1,
  storage/maintenance faults use 255. Writes are not retried automatically.

The only public HTTP addition is the informational `test_mode` boolean on status
snapshots. Cleanup has no HTTP endpoint and does not erase any flash partition.
