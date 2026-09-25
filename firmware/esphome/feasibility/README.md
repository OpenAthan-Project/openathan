# Firmware feasibility build

This compile-only milestone integrates reusable prayer calculations, offline
MP3 partition playback and C126 + A167 audio hardware. It is not release
firmware. No scheduler, provisioning wizard, device UI or Home Assistant is
required or included. See the [capacity report](../../../docs/development/feasibility-report.md).

## Dependencies and build

Use Python 3.13 and `pip install -r firmware/esphome/requirements.txt` in a
virtual environment. ESPHome is pinned to 2026.9.0 and the board pins its
recommended ESP-IDF 5.5.5. Adhan C++ v1.0.2 is vendored unchanged under MIT.
ESP-IDF C++ exceptions are enabled for the Adhan builds; their cost is included
in the measurements. No development branches or prereleases are used.

The [dependency manifest](dependencies.json) records the exact ESPHome-selected
compiler and firmware component versions/hashes. `check_feasibility.py` rejects
drift in those dependencies. ESPHome manages the framework's Python tooling;
keep each generated `dependencies.lock` and build log with its artifacts.
The recorded macOS/Python 3.13 constraints reproduce the host ESPHome environment;
other host platforms should use the pinned requirements and ESPHome's supported
platform-specific dependencies.

For that macOS profile, install with
`pip install -r firmware/esphome/requirements.txt -c firmware/esphome/requirements-macos-py313.lock`.

Create an ignored `secrets.yaml` in this directory with `wifi_ssid`,
`wifi_password`, and `ota_password`. Compile-only placeholders are sufficient
for these measurements. Use local credentials only for a separately authorized
device test. The scripts below compile and inspect; they do not upload.

From the repository root, using the virtual environment:

```sh
export ESPHOME_BUILD_PATH=/tmp/openathan-feasibility/baseline
esphome compile firmware/esphome/feasibility/baseline.yaml > /tmp/openathan-baseline.log 2>&1
export ESPHOME_BUILD_PATH=/tmp/openathan-feasibility/calculations
esphome compile firmware/esphome/feasibility/calculations.yaml > /tmp/openathan-calculations.log 2>&1
export ESPHOME_BUILD_PATH=/tmp/openathan-feasibility/shared
esphome compile firmware/esphome/feasibility/shared-audio.yaml > /tmp/openathan-shared.log 2>&1
```

The full variant includes a fixed upstream prayer reference diagnostic at boot.
It does not use that location or date as product settings. The front button
selects normal audio on a short click, Fajr on a 2–5 second click, and stops an
active announcement on either click. Nothing plays automatically at boot.
The initial player control volume is 70%, mapped within a 60% output ceiling
(42% speaker setting). ESPHome restores a saved volume in preference to this
initial default. Full-recording listening tests passed at 60% control volume;
the later requested 70% setting was verified to persist after reboot, without
another full listening test. The supplied stereo MP3s pass through ESPHome's decoder
and mono speaker pipeline without altering the stored recordings.

## Audio image and storage contract

```sh
python tools/audio_image.py --normal /absolute/path/normal.mp3 \
  --fajr /absolute/path/fajr.mp3 --output-dir /tmp/openathan-audio
python tools/check_feasibility.py \
  --build-dir /tmp/openathan-feasibility/shared/openathan-feasibility \
  --log /tmp/openathan-shared.log \
  --audio-image /tmp/openathan-audio/athan-audio.bin
```

Repeat the check for each variant's build directory and log. It enforces the
1.5 MiB application budget, dependency pins, partition layout, matching
factory/OTA application payloads, and absence of the supplied recordings'
initial data blocks in the application. Source review additionally confirms
that no audio asset is compiled into the application.

The builder preserves input bytes, rejects empty/oversized/missing inputs,
and writes `athan-audio.bin` plus a JSON manifest outside the repository. It
does not decode MP3s or assess their contents, quality or redistribution rights.

| Region | Offset | Size |
| --- | --- | --- |
| Bootloader, partition table, NVS, OTA metadata | 0 | 64 KiB |
| Application 0 | `0x10000` | 2 MiB |
| Application 1 | `0x210000` | 2 MiB |
| Shared audio | `0x410000` | 3.5 MiB |
| Reserved | `0x790000` | 448 KiB |

The raw data partition subtype is `0x40`, label `athan_audio`. OAUDIO01 uses
little-endian fields: eight magic bytes, a 32-bit used length, a 32-bit track
count (2), then two 48-byte entries. Each entry contains 32-bit ID (1 normal,
2 Fajr), relative offset, length, codec (1 MP3), and a 32-byte SHA-256. Bytes
112–143 contain SHA-256 of bytes 0–111. The header occupies 4096 bytes; payloads
are 16-byte aligned. Padding is `0xFF`. Hashes detect corruption, not authenticity.

The adapter maps the partition for its entire boot lifetime, validates metadata
and both hashes, and keeps two persistent ESPHome `AudioFile` objects. Missing,
wrong-sized, corrupt, or unmappable storage disables playback. It never writes
audio storage. Both tracks mapped, verified and played on the reference device;
see the dated results in the capacity report. Adapter readiness indicates valid
storage and player initialization, not confirmation that the DAC/amplifier is
powered or that sound is audible. Inspect hardware setup logs during installation.

Regular ESPHome application OTA targets the inactive application slot. The
recordings are a separate image, absent from both application OTA and generated
factory firmware. The first installation needs a separately planned USB partition
migration and audio write. Never apply this partition table as an ordinary OTA
update to the previous single-slot diagnostic image. Shared audio updates are
not atomic and are outside this milestone. The two slots do not prove rollback.

## Host verification

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python tools/run_tests.py
```

The C++ image validator and adapter use CommonCrypto on macOS or OpenSSL on other
hosts; firmware uses ESP-IDF mbedTLS. Adapter tests compile the actual adapter
against small ESPHome/IDF host substitutes to inject absent partitions, mapping
failures, corrupt payloads and delayed playback pointers. They cannot establish
real MMU availability, audible playback, dynamic memory, or OTA recovery.

Reusable pieces: core calculations and image validation, the image builder,
partition contract, ESPHome audio adapter and reference hardware configuration.
Temporary pieces: the three measurement entry points, fixed-date calculation
logging and diagnostic button behavior. Keep media and private diagnostic
archives outside Git. This milestone does not flash or publish firmware.
