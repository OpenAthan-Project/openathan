# Firmware release preparation

The firmware repository builds and packages releases. The website consumes a
reviewed release; it never compiles firmware. The [installer contract](https://github.com/OpenAthan-Project/website/blob/ba356c80f00ff07046b7433c03e0fdc9802f64b0/installer/README.md)
remains schema v1, for AtomS3R C126 + Voice Pyramid A167, ESP32-S3 with 8 MiB flash
and the dual-2 MiB application / 3.5 MiB shared-audio layout.

The selected normal and Fajr recordings have a documented
[media approval](../../AUDIO-LICENSES.md). Packaging and uploading require a source
revision containing that approval and the exact approved MP3 files. Other private
test recordings, CI audio fixtures and historical recovery images are not release
inputs. This tooling never flashes, publishes, or selects a website release.

## Prerequisites

Use Git, Python 3.13, CMake, a C++20 compiler, and the pinned
[ESPHome environment](../../.github/workflows/README.md). Linux also needs OpenSSL
development headers. On macOS use the documented constraints file. Install the
requirements from the source revision being released; the checks reject dependency
drift. Uploading additionally needs authenticated `gh` access to
`OpenAthan-Project/openathan` (`gh auth status --hostname github.com`).

Paths below are examples; use new directories outside the repository. Commands
read only committed source. Local changes and ignored settings are not incorporated.
The operator must trust the selected source revision: builds execute its build
configuration and production audio-validator source.

## Build an exact commit

```sh
python tools/release.py build --commit <full-40-character-commit-sha> \
  --output-dir /tmp/openathan-build
```

The command exports source to a fresh staging directory and compiles only
`firmware/esphome/openathan.yaml`. It checks dependency pins, ESP32-S3 image
checksums/digests, the actual factory partition table, empty initial NVS, matching
factory/OTA application payloads, and the 1,572,864-byte application budget.
No audio is required. Output includes both images, the source archive, build record,
compile log and compiled evidence. Preserve this directory locally through packaging;
raw evidence may contain local paths and is never uploaded.

A failed operation leaves no finished output directory. Choose a fresh directory
for retry. A build record reports byte counts and remaining budget; static RAM
measurements do not establish runtime heap/PSRAM headroom. Builds are traceable to
source and toolchain pins; byte-for-byte reproduction across hosts is not claimed.

## Package reviewed recordings

First review both tracks' redistribution rights, attribution, content and quality.
Commit their exact hashes and approval in [the registry](../../release/recordings.json)
and document those hashes and rights in [AUDIO-LICENSES.md](../../AUDIO-LICENSES.md).
Then build that source revision. Packaging reads this committed metadata, not
later working-tree edits. The registry always lists `normal` then `fajr`; each
approved entry requires `sha256`, HTTPS `source` and `license` URLs, `attribution`,
and `approved: true`. Approval records human review; tooling cannot establish rights.

```sh
python tools/release.py package --build-dir /tmp/openathan-build \
  --tag v0.1.0 --normal /path/to/normal.mp3 --fajr /path/to/fajr.mp3 \
  --output-dir /tmp/openathan-bundle
```

The version is an explicit `vX.Y.Z` target, not a selection of the first public
release. Packaging preserves MP3 bytes, checks their approved hashes, builds the
shared partition, and executes the production C++ audio-format validator from the
source commit. Format/hash validation does not decode MP3 or replace listening tests.

The completed bundle contains exactly:

| Asset | Purpose |
| --- | --- |
| `manifest.json` | Installer schema v1, source commit, layout, sizes, hashes and license link |
| `firmware.factory.bin` | Fresh-install firmware, starting at offset zero |
| `athan-audio.bin` | Exactly 3.5 MiB at `0x410000`, outside the application |
| `SHA256SUMS` | SHA-256 of the other four files |
| `build-report.json` | Public source/toolchain identity and capacity measurements |

Only factory and audio appear in the manifest's two parts. OTA binaries remain
local evidence; this milestone does not add a product OTA mechanism. The report
excludes raw logs, credentials and personal paths. Existing outputs and symlink
files are rejected; regenerate a changed bundle into a fresh directory.

## Upload a draft

Rebase and review the implementation before its PR; merge is a separate decision.
For release preparation, the selected source must be on `main` and its latest
main-push Firmware CI run must have passed. Deliberately create and push the
version tag at that exact commit before uploading, after checking it is unused:

```sh
git tag -a v0.1.0 <full-40-character-commit-sha> -m 'OpenAthan v0.1.0'
git push origin refs/tags/v0.1.0
python tools/release.py upload-draft --bundle /tmp/openathan-bundle
```

The upload command never creates or moves a tag. It revalidates local files and
committed media approval, checks the remote tag and main ancestry, and uses
`gh release create --draft --verify-tag --latest=false`. It uploads only the five
assets and downloads them to verify exact bytes. There is no automatic publication
or background upload from CI.

After a network failure, rerun the same command. It reconciles the existing draft
and uploads only missing files. Identical assets are retained; changed, unexpected,
partially accepted or duplicate assets stop the command for operator review. It
never uses `--clobber` or modifies a published release. Do not publish or edit the
draft concurrently with upload; the command rechecks state but remote changes are
not an atomic transaction.

Success reports the draft URL and manifest SHA-256. Complete physical installation,
interruption/recovery and scheduled-playback qualification against these exact
artifacts. Record public evidence and limitations, explicitly publish the reviewed
release, then propose a separate website catalog PR using the exact tag and
manifest hash. Set the website's review flags only after those reviews are complete.

## Validation

Run the host and Python suites using the [CI guide](../../.github/workflows/README.md).
Release tests use synthetic non-executable firmware and non-decodable audio in
temporary directories, plus mocked GitHub responses. Nothing is uploaded or flashed.

CI also checks producer-generated files with the actual website validator from the
commit and file hash in [installer-contract.json](../../release/installer-contract.json).
To reproduce, use Node 24.19.0 and a checkout of that website commit:

```sh
python tests/check_installer_contract.py --website /path/to/pinned-website
```

The firmware workflow remains read-only and does not publish build artifacts.
