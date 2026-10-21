# Firmware checks

AUDIO-Board production and isolated development jobs compile the screenless
profile and enforce the same dependency, storage and capacity contracts. Host
tests cover checked amplifier sequencing, expander read failures, key debounce,
RGB output and persistent physical volume changes. These do not establish
audible playback or hardware acceptance.

`firmware.yml` runs on pull requests and pushes to `main`, with read-only
repository permissions and exact stable action revisions. It runs the C++ suite
with UndefinedBehaviorSanitizer, the complete Python suite, and schema validation
for the six developer configurations, the provisioning reference configuration,
the isolated provisioning-validation configuration, and the isolated upgrade
qualification configuration and the isolated Quran qualification configuration.
Waveshare V2 audio, display and attended diagnostic development configurations
are also generated and compiled. These require isolated storage and disable every
public update transport. Capacity inspection checks their 16 MiB headers explicitly;
a separate Waveshare production job checks the registered 16 MiB production
contract, with production storage and signed updates.
The Waveshare RTC host fixture checks strict register validation, interrupted
initialization, verified SNTP writes and startup restoration, including scheduler
handoffs across clock corrections and saved skips. Hardware RTC/offline-restart
acceptance remains separate from CI.
A Python host regression compiles the pinned production SNTP source with the RTC
adapter to verify offline startup preservation and genuine network updates.
Separate jobs compile `scheduler/device.yaml`, `scheduler/validation.yaml` and
`openathan.yaml` and `provisioning/validation.yaml`, then enforce dependency pins, partition layout,
the 1.5 MiB application budget and factory/OTA payload consistency. Two additional
jobs build healthy upgrade qualification and forced startup-failure variants.
Their generated test CA and signing key are ephemeral and never enter releases.
An additional Waveshare Quran qualification job compiles the private-fixture
transport and runtime observations, with isolated storage and updates disabled.
The device build also tests the production settings JSON bridge with the exact resolved
ArduinoJson library. The host suite exercises interrupted settings writes and
the bridge against pinned ESPHome timezone conversion code. Additional host tests
cover the production local API, USB protocol, activation/credential storage, Digest
authentication and Wi-Fi transactions. Python tests generate both provisioning
configurations and exercise the pinned ESPHome scan policy to protect discovery
of alternative networks while connected. A separate browser job builds the production
C++ Digest verifier and runs the embedded UI with simulated settings endpoints in
Chromium and WebKit. It covers sustained authentication, client-nonce rotation,
expiry renewal on reads and writes, revision conflicts, dropped responses and
narrow-screen layout. Chromium checks that renewal causes no second sign-in prompt.

The device build also runs `tools/check_phone_setup.py --arduinojson PATH` with
the resolved library. Production and isolated tests execute the bootstrap HTTP,
credential and connection lifecycle methods against deterministic platform I/O.
They cover first-run admission, existing-password verification, interrupted
handoff, failed joins, storage faults and USB/phone transaction exclusion. CTest
checks the bounded DNS codec and setup admission helpers; Chromium/WebKit exercise
the separate setup assets, including explicit busy/rate-limit recovery. These
checks do not establish physical captive-popup or radio-handoff behavior.

Production and isolated host builds exercise the same storage implementations.
Tests prove cleanup rejects production namespaces, preserves unrelated records,
and remains safe across interrupted writes/commits. Both settings/API builds
verify `test_mode`; browser tests verify the isolated build's warning banner.
Updater host tests cover production behavior, ordinary isolated installation
denial, both qualification holds, erased/corrupt baseline metadata and forced
startup rollback. Python tests cover real local TLS, feed faults, release
exclusion and stale-state/write-failure guards for scoped USB transitions.
Updater transport regressions cover long GitHub asset queries, maximum-length
URLs, per-redirect buffer sizing, cleanup and failed allocation/transport retries.
The reference build also compiles its resolved ESP-IDF request formatter and
header sender with the SDK header implementation against a host transport. It
checks that complete requests are transmitted, including the header-space
boundary; it does not establish device TLS or runtime memory headroom.

Upgrade and Quran qualification C++ live in separate components compiled only
into their maintainer variants. Host checks reject inclusion in production or
incomplete test profiles. Capacity checks require generated definitions and verify component
exclusion in production/ordinary isolated builds; release tests reject both
qualification instrumentation and isolated storage markers in publishable images.

CI uses Python 3.13 on Ubuntu 24.04 and the pinned ESPHome requirements. The
macOS-specific constraints file must not be installed on Linux. Builds use
public compile-only settings and generated, non-decodable audio-image fixtures;
no credentials, recordings, hardware connection or private archive is needed.
The workflow does not upload firmware artifacts or create releases. Release tests
exercise temporary synthetic bundles and mocked draft uploads. The host job also
uses Node 24.19.0 and the pinned website validator to check installer compatibility.
See [release preparation](../../docs/development/releases.md) for the explicit
build, package and draft-upload commands.

## Documentation-only pull requests

All firmware check names remain present on every pull request. Each firmware job
checks the PR merge identity and complete changed-path list before installing
build dependencies. For documentation-only changes, it reports an intentional
omission in its job summary and skips dependency installation, compilation,
capacity checks and the regressions requiring resolved firmware libraries.
Host tests and browser tests still run normally.

The policy in `tools/ci_build_policy.py` allows Markdown under `docs/` and an
explicit list of existing prose-only Markdown paths elsewhere. It excludes
`AUDIO-LICENSES.md`, which release tooling consumes, and third-party metadata.
Unknown paths, mixed changes, renames involving non-documentation paths, empty
diffs and detection failures run the complete builds. Every push to `main` also
runs the complete suite. The workflow uses step conditions rather than skipping
the workflow or removing matrix entries, preserving all existing check names.

## Reproduce from a clean checkout

Use a fresh disposable checkout and a Python 3.13 virtual environment. Install
CMake, a C++20 compiler, FFmpeg and OpenSSL development headers (Linux only),
then run:

```sh
python -m pip install -r firmware/esphome/requirements.txt
cmake -S . -B build -DCMAKE_CXX_FLAGS='-fsanitize=undefined -fno-sanitize-recover=all'
cmake --build build
ctest --test-dir build --output-on-failure
python tools/run_tests.py
python tools/prepare_ci.py --output-dir /tmp/openathan-ci-audio
export ESPHOME_BUILD_PATH=/tmp/openathan-ci-device
python -m esphome compile firmware/esphome/scheduler/device.yaml > /tmp/openathan-ci-device.log 2>&1
python tools/check_feasibility.py \
  --build-dir /tmp/openathan-ci-device/openathan-feasibility \
  --log /tmp/openathan-ci-device.log \
  --audio-image /tmp/openathan-ci-audio/athan-audio.bin
```

Use unused temporary paths. Fixture preparation refuses existing secret files
or symlinks, and refuses audio output inside the repository. Never install these
placeholder builds or attempt to play the synthetic payloads. Repeat compilation
with `scheduler/validation.yaml` and a separate build/log path for the test harness.
For upgrade qualification, compile `upgrades/qualification.yaml` in its own build
directory; the image name is `openathan-test`. To build the startup-failure variant:

```sh
ESPHOME_BUILD_PATH=/tmp/openathan-ci-upgrade-failure python -m esphome \
  -s qualification_version v0.0.3 -s qualification_startup_failure true \
  compile firmware/esphome/upgrades/qualification.yaml
```

Use the [upgrade runbook](../../firmware/esphome/upgrades/VALIDATION.md) for actual
recordings, private trust setup, source-bound artifacts and attended hardware work.
Compile `waveshare/quran-qualification.yaml` separately for controlled Quran
failures; its image name is `openathan-test`. The
[Quran failure runbook](../../docs/development/quran-failure-qualification.md)
covers private trust, approved real recordings and attended recovery.

CI establishes source/build behavior, not audible playback or hardware recovery.
See the [dated device results](../../docs/development/feasibility-report.md).

The device job also runs `tools/check_quran.py --arduinojson` against its
resolved ArduinoJson headers. This compiles the production metadata worker and
shared redirect policy under UBSan with deterministic HTTP/task adapters. It
checks cancellation, Athan priority, URL restrictions, paging/search, malformed
responses, transport failures, stalled/disconnected reads, fresh-Play recovery
and unavailable recordings. A separate UBSan executable checks qualification
routing and the original logical redirect policy. Python tests exercise real
local TLS rejection, served faults and profile/release exclusion. These checks
do not establish physical streaming or device TLS acceptance.

The reference job runs `tools/check_mp3_decoder.py --build-dir` against its
resolved micro-MP3 0.4.0 dependency. It compiles the production decoder and audio
transfer buffers with UBSan and deterministic host memory/ring-buffer adapters,
linking the codec's unmodified native host library. Synthetic single-frame, short and 52-second
MP3 fixtures verify decoded PCM, chunked input, graceful partial EOF, sink
backpressure and decoder reuse. Empty, text-only, ID3-only and incomplete first
frames must fail in Quran-enabled builds; the non-Quran path retains upstream
EOF behavior. No hardware playback or audible acceptance is established.


The reference host checks also run the production pipeline completion state
machine and optional dua downloader with deterministic I/O. The downloader checks
interrupted erase/write recovery without changing the normal/Fajr data, signature
and size rejection, and cancellation. These synthetic checks do not establish
hardware flash latency, listening quality or runtime memory headroom; see the
[dua contract](../../docs/development/dua.md).

Two additional builds compile `dua/qualification.yaml` and
`waveshare/dua-qualification.yaml`. These use private media trust with firmware
updates disabled, isolated storage and a separate dua qualification component.
Host tests check generated profiles, release exclusion and a loopback-only TLS
feed with exact-version routes and media faults. The device job executes the
production downloader under this profile too. See
[private dua qualification](../../docs/development/dua-qualification.md).
