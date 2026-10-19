# Private dua qualification preparation

These maintainer profiles exercise the production dua downloader and sequence
with private media trust on either reference board. They use isolated settings,
credentials, history and enabled intent. Firmware Wi-Fi/USB updates remain disabled;
the Waveshare restriction against upgrade-qualification firmware remains intact.
The ordinary product trust key and release transport do not change.

Preparation runs on the build host. It does not access or install on a device.
Use the [scheduler](../../firmware/esphome/scheduler/VALIDATION.md) and
[provisioning](../../firmware/esphome/provisioning/HARDWARE_TEST.md) runbooks for a
separately authorized, attended session. Isolated preferences do **not** isolate
the physical audio banks. Preserve the complete current shared-audio partition,
settings/history and fresh compatible application recovery before installation.

## Private trust and approved media

Start from reviewed, merged source and the [pinned toolchain](../../.github/workflows/README.md).
Create a new private directory outside Git using the build host's reachable
RFC1918 address. The generated HTTPS certificate lasts seven days; an address or
trust change requires rebuilding the candidates. Only public trust enters them.

```sh
export DUA_QUALIFICATION_DIR=/tmp/openathan-private-dua-session
python tools/dua_qualification.py init \
  --directory "$DUA_QUALIFICATION_DIR" --address 192.168.1.2 --port 8443
python tools/dua_qualification.py stage \
  --directory "$DUA_QUALIFICATION_DIR" --commit "$(git rev-parse HEAD)" \
  --normal /private/path/normal.mp3 --fajr /private/path/fajr.mp3 \
  --dua /private/path/dua.mp3
```

Staging checks the committed recording approval and the real dua decoder/duration.
It preserves all three recordings, signs the exact dua bytes with the private key,
and creates an immutable `v0.0.1` fixture. `baseline-audio.bin` is the unchanged
two-track image; `fresh-install-private-audio.bin` additionally contains one
committed bank. Compare them before `0x300000` and validate the signed bank. Neither
image is automatically installed. Production signing keys are rejected.

Put the returned **public** paths and origin in the ignored `secrets.yaml` beside
each selected profile, using `dua_qualification_origin`,
`dua_qualification_ca_file` and `dua_qualification_public_key_file`. Keep all keys,
recordings, build outputs and private evidence outside Git.

## Build both profiles

Use `firmware/esphome/dua/qualification.yaml` for Atom and
`firmware/esphome/waveshare/dua-qualification.yaml` for Waveshare. Each has the
`openathan-test` name with MAC suffix and qualification version `v0.0.1`.

```sh
export OPENATHAN_BUILD_COMMIT=$(git rev-parse HEAD)
ESPHOME_BUILD_PATH=/tmp/openathan-dua-atom python -m esphome compile \
  firmware/esphome/dua/qualification.yaml > /tmp/openathan-dua-atom.log 2>&1
python tools/check_feasibility.py \
  --build-dir /tmp/openathan-dua-atom/openathan-test \
  --log /tmp/openathan-dua-atom.log \
  --audio-image "$DUA_QUALIFICATION_DIR/v0.0.1/baseline-audio.bin"
```

Repeat with the Waveshare profile and separate build/log paths. Record hashes,
configuration, source identity, pins and matched before/after OTA sizes. Generated
definitions and compiled sources must exclude upgrade/Quran qualification, keep
`OPENATHAN_UPDATES_ENABLED 0` and include isolated storage and the separate dua
qualification component. Capacity checks enforce those conditions. Release
packaging rejects these applications. CI images and media are synthetic and
must never be installed or played.

## Host-only feed checks

The feed serves only `/releases/download/v0.0.1/dua.json` and `dua.mp3`. Verify
trusted TLS, wrong-host rejection, signatures, exact payload hash and denied
private-key/other-version paths over loopback before later LAN use. Starting the
feed does not enable dua, change settings or trigger a device request.

```sh
python tools/dua_qualification.py serve --directory "$DUA_QUALIFICATION_DIR"
```

`offer --fault` selects `none`, `signature`, `hash`, `truncate`,
`descriptor-disconnect` or `stall`. `--delay` accepts 0–10 seconds for a stalled
response or between 4 KiB chunks. Use delays above the four-second request timeout
for a transport stall. Reset to `none` explicitly for recovery. Do not infer a
hardware verdict from host fixture behavior.

## Attended evidence

After a fresh identity/preservation check and authorized application-only
installation, require `test_mode: true`, private version `v0.0.1`, confirmed
startup and `dua.qualification.identity: OPENATHAN_DUA_QUALIFICATION_V1`.
Provision and activate isolated settings deliberately. Use real calculations and
the existing safe window; do not alter the clock or production consumption.

Record scheduled normal/Fajr/shared playback, complete output drain, the
two-second pause, Stop/mute/disable, offline/reboot behavior, interruption/retry
and bank preservation. Capture `GET /api/dua` or status through each case. Private
snapshots add internal free/largest/minimum heap, PSRAM free/largest blocks and
the minimum completed dua-worker stack space. Stack, heap and audible outcomes
are distinct evidence; these observations do not establish clean-product runtime
headroom. Firmware-update concurrency needs its own applicable profile/session,
because both firmware-update transports are disabled here.

Close the feed after the session. Restore only from fresh compatible evidence,
reconcile any test-bank writes, and verify the preserved production records.
