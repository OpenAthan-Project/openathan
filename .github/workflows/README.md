# Firmware checks

`firmware.yml` runs on pull requests and pushes to `main`, with read-only
repository permissions and exact stable action revisions. It runs the C++ suite
with UndefinedBehaviorSanitizer, the complete Python suite, and schema validation
for all six developer configurations. Separate jobs compile `scheduler/device.yaml`
and `scheduler/validation.yaml`, then enforce dependency pins, partition layout,
the 1.5 MiB application budget and factory/OTA payload consistency.

CI uses Python 3.13 on Ubuntu 24.04 and the pinned ESPHome requirements. The
macOS-specific constraints file must not be installed on Linux. Builds use
public compile-only settings and generated, non-decodable audio-image fixtures;
no credentials, recordings, hardware connection or private archive is needed.
The workflow does not upload firmware artifacts or create releases.

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

CI establishes source/build behavior, not audible playback or hardware recovery.
See the [dated device results](../../docs/development/feasibility-report.md).
