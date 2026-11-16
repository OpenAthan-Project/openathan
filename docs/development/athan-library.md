# Author an Athan catalog

The catalog lists recordings for separate regular and Fajr selections. The device
stores the selected pair locally. The catalog and audio files are release assets;
keep the recordings outside Git. The existing `release/recordings.json` continues
to govern firmware release audio and is not the catalog registry.

## Prepare and review recordings

1. Record the exact source, permission, attribution, role, transformations, size,
   SHA-256, and review status in `release/athan-recordings.json`. Document the
   evidence in [Release recording rights and attribution](../../AUDIO-LICENSES.md).
2. Put the exact MP3 files in a directory outside the repository, named by their
   registry `file` fields. The directory must contain no other files.
3. Build and verify an unsigned review bundle:

   ```sh
   python3 tools/athan_catalog.py build --candidate --assets /path/to/assets --output /path/to/new-review-bundle
   python3 tools/athan_catalog.py verify --candidate --bundle /path/to/new-review-bundle
   ```

The candidate bundle includes unapproved entries and is not a signed or
publishable release. Review the content, permission, attribution, sound quality,
and device behavior before changing approval. Document the results and commit
approval against the exact output hashes. No listening or physical qualification
is implied by successful catalog validation.

## Build and verify a signed bundle

Use a new version for each catalog release. Before signing, check existing
published versions and their asset hashes. Never replace a published version or
its files. These checks require a release operator; the local tool cannot inspect
GitHub or enforce remote immutability. Publishing is a separate decision.

Commit the registry at `HEAD`, and confirm every entry is approved. Supply the
production signing key from outside the repository:

```sh
python3 tools/athan_catalog.py build --assets /path/to/assets --output /path/to/new-release-bundle --signing-key /path/to/private-key.pem
python3 tools/athan_catalog.py verify --bundle /path/to/new-release-bundle
```

For a signed bundle, verification also requires the approved registry committed at
`HEAD`. Check out the corresponding release revision before verifying an older
version. The tool checks the private key against `release/upgrade-public-key.pem` and
verifies the resulting P-256 signature. It rejects unapproved recordings,
noncanonical or invalid metadata, unexpected files, mismatched lengths and hashes,
and any pair whose OAUDIO01 image exceeds `0x300000` bytes, including the
4096-byte header and 16-byte alignment. It writes a new directory atomically and
refuses an existing output directory. Version length is limited to 31 bytes, each
version component to 32 bits, catalog entries to 16, payload to 24 KiB, and the
signed envelope to 32 KiB. The signing key and generated media stay outside Git.

The signed `catalog.json` and each MP3 are intended for the same immutable release
version. The device uses `latest/download/catalog.json` to discover the current
version and downloads its assets from the versioned release. This tool does not
create a release, upload files, or make a catalog public.
