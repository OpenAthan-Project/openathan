# Firmware checks

`firmware.yml` runs on pull requests and pushes to `main`, with read-only
repository permissions and exact stable action revisions. It runs the C++ suite
with UndefinedBehaviorSanitizer, the complete Python suite, and schema validation
for the six developer configurations, the provisioning reference configuration,
the isolated provisioning-validation configuration, and the isolated upgrade
qualification configuration.
Separate jobs compile `scheduler/device.yaml`, `scheduler/validation.yaml` and
`openathan.yaml` and `provisioning/validation.yaml`, then enforce dependency pins, partition layout,
the 1.5 MiB application budget and factory/OTA payload consistency. Two additional
jobs build healthy upgrade qualification and forced startup-failure variants.
Their generated test CA and signing key are ephemeral and never enter releases.
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

Qualification C++ is a separate component compiled only into the two maintainer
variants. Host checks reject its inclusion in production or incomplete test
profiles. Capacity checks require generated definitions and verify component
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

All eight check names remain present on every pull request. Each firmware job
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
CMake, a C++20 compiler and OpenSSL development headers (Linux only), then run:

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

CI establishes source/build behavior, not audible playback or hardware recovery.
See the [dated device results](../../docs/development/feasibility-report.md).
