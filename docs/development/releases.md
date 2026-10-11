# Firmware release preparation

[v0.5.0](../releases/v0.5.0.md) is the published stable release for Atom and
Waveshare Box V2. The [release qualification report](release-validation-2026-10-10.md)
records exact production images, attended Waveshare installation/update results,
public delivery and remaining limits.
The [v0.4.0 preparation record](v0.4.0-preparation-2026-10-07.md) records its
automated builds and physical-validation limits. The earlier
[release evidence](release-validation-2026-10-04.md) records reference-device
acceptance and public delivery for v0.3.0.
The earlier [preparation report](v0.3.0-preparation-2026-10-03.md) retains the
matched development measurements.

Upgrade-capable firmware also publishes signed application-only assets. Pass
`--signing-key PATH` (a private P-256 key outside Git) to the packaging command;
see the [upgrade contract and recovery requirements](firmware-upgrades.md).
Legacy five-asset releases remain supported, and the fresh-install manifest stays
schema 1.

The firmware repository builds and packages releases. The website consumes a
reviewed release; it never compiles firmware. The [installer contract](https://github.com/OpenAthan-Project/website/blob/main/installer/README.md)
retains schema v1. Profiles identify AtomS3R C126 + Voice Pyramid A167
(8 MiB flash) or Waveshare Box V2 (16 MiB flash), both ESP32-S3
and the dual-2 MiB application / 3.5 MiB shared-audio layout.

The selected normal and Fajr recordings have a documented
[media approval](../../AUDIO-LICENSES.md). Packaging and uploading require a source
revision containing that approval and the exact approved MP3 files. Other private
test recordings, CI audio fixtures and historical recovery images are not release
inputs. This tooling never flashes or publishes. Once a qualified release is
explicitly published as stable latest, the website's automatic policy can adopt
it after validation. See the [current release evidence](release-validation-2026-10-10.md).

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

The command exports source to a fresh staging directory and compiles the registered production configuration. The default remains
`firmware/esphome/openathan.yaml`; pass
`--hardware waveshare-esp32-s3-touch-lcd-1_85c-box-v2` for
`firmware/esphome/waveshare/production.yaml`. Both builds use the same source commit. It checks dependency pins, ESP32-S3 image
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
  --tag "$RELEASE_TAG" --normal /path/to/normal.mp3 --fajr /path/to/fajr.mp3 \
  --signing-key /private/path/release-signing-key.pem \
  --output-dir /tmp/openathan-bundle
```

Before running this example, set `RELEASE_TAG` to a new, unused `vX.Y.Z` version
matching `release/firmware.json` and the official YAML project version in the
selected source. Do not reuse or move an existing release tag. Packaging
preserves MP3 bytes, checks their approved hashes, builds the
shared partition, and executes the production C++ audio-format validator from the
source commit. Format/hash validation does not decode MP3 or replace listening tests.

Single-board upgrade-capable releases contain seven assets. Combined releases
contain thirteen: retain the seven Atom names below and add six Waveshare files
prefixed `waveshare-box-v2.`. Both manifests reference the same `athan-audio.bin`.
Pass `--waveshare-build-dir /tmp/openathan-waveshare-build` when packaging the Atom
build to produce the combined bundle. The two builds must share an exact commit.
Each board has its own manifest, factory, OTA, signed descriptor, checksums and
build report; Atom filenames and signed schema 1 remain compatible.

The Atom assets are:

| Asset | Purpose |
| --- | --- |
| `manifest.json` | Installer schema v1, source commit, layout, sizes, hashes and license link |
| `firmware.factory.bin` | Fresh-install firmware, starting at offset zero |
| `athan-audio.bin` | Exactly 3.5 MiB at `0x410000`, outside the application |
| `firmware.ota.bin` | Application-only update, matching the factory application's bytes |
| `upgrade.json` | Signed version/source/compatibility and application-size/hash descriptor |
| `SHA256SUMS` | SHA-256 of the other six files |
| `build-report.json` | Public source/toolchain identity and capacity measurements |

Only factory and audio appear in the fresh-install manifest's two parts. The
device updater consumes the separate signed descriptor and OTA application;
the website importer validates both installation and signed upgrade assets. Legacy
releases without upgrade identity retain the five-asset contract and four-file
checksum list. The report excludes raw logs, credentials and personal paths.
Existing outputs and symlink files are rejected; regenerate a changed bundle
into a fresh directory.

## Upload a draft

Rebase and review the implementation before its PR; merge is a separate decision.
For release preparation, the selected source must be on `main` and its latest
main-push Firmware CI run must have passed. Deliberately create and push the
version tag at that exact commit before uploading, after checking it is unused:

```sh
git tag -a "$RELEASE_TAG" <full-40-character-commit-sha> -m "OpenAthan $RELEASE_TAG"
git push origin "refs/tags/$RELEASE_TAG"
python tools/release.py upload-draft --bundle /tmp/openathan-bundle
```

The upload command never creates or moves a tag. It revalidates local files and
committed media approval, checks the remote tag and main ancestry, and uses
`gh release create --draft --verify-tag --latest=false`. It uploads the validated
bundle assets and downloads them to verify exact bytes. There is no automatic publication
or background upload from CI.

After a network failure, rerun the same command. It reconciles the existing draft
and uploads only missing files. Identical assets are retained; changed, unexpected,
partially accepted or duplicate assets stop the command for operator review. It
never uses `--clobber` or modifies a published release. Do not publish or edit the
draft concurrently with upload; the command rechecks state but remote changes are
not an atomic transaction.

Success reports the draft URL and manifest SHA-256. Review applicable physical
installation, interruption/recovery and scheduled-playback evidence against the
candidate. Reuse accepted evidence within its source limits and identify untested
changes; do not infer physical acceptance from CI. Record public evidence and
limitations, then explicitly approve publication and stable latest selection.
Draft upload does neither automatically.

Both device discovery and the website's automatic mode follow stable latest.
Publishing it authorizes website adoption after artifact/source/hash and website
checks pass; no per-release catalog PR is needed. The generated review flags
record that human publication policy rather than proving physical qualification.
A reviewed catalog PR is still used for a persistent manual pin/rollback or
disabling fresh installation. See the [website deployment policy](https://github.com/OpenAthan-Project/website/blob/main/deployment/README.md).
Website adoption does not queue an update on existing speakers; owners choose
**Install update** on the authenticated device page.

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
