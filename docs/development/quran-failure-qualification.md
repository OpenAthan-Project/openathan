# Controlled Quran failure qualification

This maintainer runbook prepares an attended Waveshare Box V2 session using
`firmware/esphome/waveshare/quran-qualification.yaml`. It exercises the ordinary
Quran worker and audio reader against a private HTTPS fixture server. The profile
adds transport routing and observations, uses isolated provisioning storage, and
disables firmware updates. It inherits encrypted diagnostic controls from the
Waveshare diagnostic profile. It cannot be packaged as release firmware.

Host tests and successful builds establish fixture and source behavior. They do
not establish audible playback, device TLS verification, decoder recovery or
runtime memory headroom. Keep physical results in a dated validation record.
The [harness validation report](quran-failure-validation-2026-10-11.md) records
automated results and the pinned OTA comparison.
The separate continuous streaming soak still needs the ordinary MP3Quran path.

## Prepare the private server and reviewed image

Use the [pinned build environment](../../.github/workflows/README.md), including
FFmpeg for validation of test recordings. Run the C++/Python checks and
`tools/check_quran.py` with the resolved ArduinoJson headers. Rebase onto current
`main`, review the final diff, and pass the draft PR's checks before hardware use.
Retain source identity, dependency locks, OTA hashes and capacity results.

Create a fresh directory outside Git. Replace the example IP with the build
host's reachable RFC1918 IPv4 address; use a port between 1024 and 65535.

```sh
export QURAN_FIXTURE_DIR=/tmp/openathan-private-quran-session
python tools/quran_qualification.py init "$QURAN_FIXTURE_DIR" \
  --address 192.168.1.2 --port 8443
```

The directory is mode 700; certificates, keys, controls and request logs are mode
600. Initialization refuses an existing directory. The CA expires after seven
days and server certificates after two days. Regenerate and rebuild when the
origin or CA changes, or certificates expire. The initial case is malformed
metadata, so a recording cannot start before explicit fixture selection.

Copy the returned origin and **public** CA path into the ignored Waveshare
`secrets.yaml`, alongside the normal private build settings:

```yaml
quran_qualification_origin: 'https://192.168.1.2:8443'
quran_qualification_ca_file: '/tmp/openathan-private-quran-session/ca.pem'
```

Build the reviewed source with a separate output directory:

```sh
export OPENATHAN_BUILD_COMMIT=$(git rev-parse HEAD)
ESPHOME_BUILD_PATH=/tmp/openathan-quran-device python -m esphome compile \
  firmware/esphome/waveshare/quran-qualification.yaml \
  > /tmp/openathan-quran-device.log 2>&1
python tools/check_feasibility.py \
  --build-dir /tmp/openathan-quran-device/openathan-test \
  --log /tmp/openathan-quran-device.log --audio-image "$APPROVED_AUDIO_IMAGE"
```

`APPROVED_AUDIO_IMAGE` is the independently validated shared audio already on the
device. CI's placeholder settings and non-decodable audio are compile-only;
never install those images or play their payloads. Use an approved real MP3
outside Git for normal, truncated and stalled audio cases. The tool requires a
bounded `.mp3` file and runs a complete FFmpeg MP3 decode before selecting it.
It records the hash and refuses to serve a changed recording. It neither
fetches provider recordings nor bundles any third-party media.

The fixture server serves only its two metadata routes and one recording route;
it never proxies a request. Each original provider URL is validated before being
mapped to the private origin, preserving its logical host and path. Redirects
return through the original policy before another connection is opened. TLS uses
the embedded private CA and verifies the **private origin's** hostname. This
session does not establish the public provider's certificate-chain acceptance.

## Fresh attended device preflight

Coordinate an operator and listener, then follow the
[scheduler](../../firmware/esphome/scheduler/VALIDATION.md) and
[provisioning hardware](../../firmware/esphome/provisioning/HARDWARE_TEST.md)
runbooks. Reidentify MAC, ESP32-S3, 16 MiB flash, 8 MiB PSRAM, partition layout,
selected application and OTA state. Inspect the actual current clock and prayer
window before interrupting operation. Preserve production settings, credentials,
consumption history, shared audio and recovery capability.

Retain a freshly matched compatible application-only recovery image and a
reviewed write/preservation plan. Use only a scoped application write and
independent readback; do not restore historical full-flash settings or history.
Do not use the Atom upgrade-transition procedure for this different board.
Require `test_mode: true`, updates disabled, the reviewed source identity and
`quran.qualification.identity: OPENATHAN_QURAN_QUALIFICATION_V1` after startup.
Any storage fault, unexpected restart or identity mismatch stops the session.

Initialize or reuse isolated setup through the existing device procedure. Keep
real time synchronization and select a conservative listening volume. Production
prayer settings/history remain untouched. Clear test settings/history only
through the scoped test maintenance procedure when necessary.

## Failure and recovery matrix

Start with normal fixtures and the approved recording:

```sh
python tools/quran_qualification.py select "$QURAN_FIXTURE_DIR" normal \
  --media "$APPROVED_TEST_MP3"
python tools/quran_qualification.py serve "$QURAN_FIXTURE_DIR"
```

Load the local Quran page and select **Private qualification recording**,
**Fixture edition**, surah 1. Confirm clear audio and a normal finish. Retain
fresh playback/catalog observations, request logs and owner confirmation.
Selection changes affect future HTTP requests; they do not rewrite an active
response. For every fault, stop prior playback, select the case, then issue one
explicit Play with the current playback token. Catalog faults can also be
triggered by an explicit catalog refresh. Do not retry an uncertain Play.

| Case | Required observation |
| --- | --- |
| `metadata-status`, `audio-status` | HTTP 503 fails without starting or retaining recording ownership. |
| `metadata-malformed`, `metadata-oversize` | Parsing fails, no recording starts, clients close, catalog or playback reports its own error. |
| `metadata-truncate` | Premature disconnect fails without accepting incomplete JSON. |
| `metadata-stall` | Partial body stalls; metadata returns an error within its I/O/deadline bounds. |
| `audio-content-type`, `audio-decode` | Wrong MIME type or deliberate text without MP3 frames reports an error and releases ownership; no persistent loading/playing state. A silent `idle` finish is not a pass. |
| `audio-truncate` | Reader terminates and clears ownership; record whether the pipeline reports an error or a partial EOF. It cannot prove normal full-recording completion. |
| `audio-stall` | Partial audio stops or errors within the reader's bounds, without automatic retry or resumption. |
| `metadata-redirect-http`, `audio-redirect-http` | HTTPS downgrade is rejected before a second connection. |
| `metadata-redirect-host`, `audio-redirect-host` | Foreign logical host is rejected before a second connection. |
| `metadata-redirect-loop`, `audio-redirect-loop` | At most five requests occur (initial request plus four redirects); operation fails and releases clients. |

Select faults in another terminal; only the audio truncation/stall cases require
media. Stall duration defaults to 40 seconds and is bounded to 45 seconds.

```sh
python tools/quran_qualification.py select "$QURAN_FIXTURE_DIR" metadata-stall
python tools/quran_qualification.py select "$QURAN_FIXTURE_DIR" audio-stall \
  --media "$APPROVED_TEST_MP3"
```

For TLS faults, stop the server, keep the same origin/embedded CA, and restart
with `--tls-mode untrusted` or `--tls-mode wrong-host`. Both must fail before any
HTTP request is logged. Restart with `--tls-mode trusted` for recovery. An
unfinished handshake on one connection must not block other clients; the server
bounds workers to eight and handshake and request-read timeouts to five seconds.
Response bodies use 4 KiB writes, each with a five-second timeout. There is no
total recording-duration limit: normal transfers can continue while bounded
writes succeed. A blocked write or disconnected client closes the connection and
releases its worker without retrying. Intentional stall cases still send their
short prefix, wait the selected stall duration, and close; that wait is separate
from the socket timeout.

After **each** failure, observe at least 30 seconds of silence and stable state
without another HTTP/audio start. Select `normal` with the approved recording,
then explicitly Play again and confirm clear playback. Repeat Stop while a
metadata read is blocked and verify that its late result cannot start audio.
After normal catalog setup, repeat scheduled Athan preemption with the phone page
closed, and reject Quran Play during Athan. Keep the clock genuine and use only
isolated schedule offsets to arrange the attended test. Confirm exactly one
Athan, silence afterward and no Quran resumption.

Snapshots under `quran.qualification` expose HTTP-attempt count, completed
metadata jobs, the metadata task's minimum unused stack in **bytes**, internal
free/largest/low-water heap, PSRAM free/largest block and reset reason. Sample
throughout faults, recovery and concurrent catalog activity; retain uptime and
source identity to identify restarts and sampling gaps. These observations apply
to the diagnostic qualification image. Static RAM and bounded tests do not prove
clean production runtime headroom or a leak-free continuous soak.

## Restore and report

Stop playback and the fixture server. Restore the freshly retained compatible
production application through its reviewed application-only plan. Independently
verify application bytes and preservation of settings, credentials, current
consumption history, audio, bootloader, partition table and unaffected flash.
After startup, verify synchronized standalone scheduling, the expected next
prayer and normal controls. Keep all private evidence outside Git.

Record each case, TLS mode, logical URL, source/image hashes, elapsed bounds,
owner listening confirmation, memory samples and recovery outcome separately.
Mark unattended listening and untested cases explicitly. The server harness,
continuous real-provider soak, mono recording, cold-power preservation and
attended Wi-Fi interruption remain separate evidence gates.
