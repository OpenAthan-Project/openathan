# Dua private qualification preparation — 2026-10-19

This is host-only preparation for an attended dua session on either reference
board. Recording approval is merged at
`3c6ec7456d2d17ab57e261805c6f6c4a68427385`; the measured qualification source is
`5f701d27caf94efb914b5a88e1ad971929d8978e`. Subsequent documentation does not
change the measured firmware inputs. The qualification changes require review
and merge before installation. No speaker was accessed, installed, reset or
played, and no release was published.

## Recording and text

The [approved recording](../../AUDIO-LICENSES.md) is unchanged: 97,700 bytes,
21.446531 seconds, SHA-256
`e012c265ed388b72c28216528c583035643401737de6fbfc69aad486268b5c49`.
Local Arabic transcription with MLX Whisper 0.4.3 and
`mlx-community/whisper-large-v3-turbo` revision
`a4aaeec0636e6fef84abdcbe3544cb2bf7e9f6fb` matched the dua wording in
[Sahih al-Bukhari 614](https://sunnah.com/bukhari:614), after removing diacritics,
punctuation and redundant whitespace. No initial text prompt was supplied; the
input hash was checked before and after transcription. The raw transcript and
verification metadata remain in the private evidence archive.

This is an automated text comparison, not human listening or verification of the
reciter's identity, pronunciation detail or speaker output. No transcoding,
normalization or trimming changed the supplied recording.

## Automated checks

- All 22 UBSan CTests and 161 pinned Python tests passed without skips.
- Seven new Python tests cover immutable approved-media staging, changed or
  unapproved media, private trust, profile generation, mutually exclusive
  qualification modes, disabled firmware updates, release exclusion and local TLS.
- The production downloader passed its 68-point interrupted erase/write checks
  under production, existing upgrade qualification and new dua qualification
  builds. Separate ordinary isolated and updates-disabled product builds refused
  production media downloads.
- Actual approved-media staging preserved normal/Fajr bytes and produced a
  verified signed optional bank under private trust. The production C++ audio
  validator accepted the extended image.
- The actual private recording and descriptor passed loopback HTTPS, exact-byte,
  hash and signature checks. Wrong-host and untrusted-CA requests failed; private
  keys, traversal and other-version routes returned 404. The listener was closed
  after validation. Separate synthetic tests exercise each supported media fault.
- Generated source and capacity checks verified isolated storage, disabled
  firmware updates and the separate dua component on both qualification builds;
  production builds exclude that component. Qualification applications are
  rejected by release packaging.

The initial full Python invocation lacked its required core-library path. Its
three prerequisite failures and the corrected complete passing run are retained
separately in private evidence; no test was weakened or skipped.

## Matched firmware measurements

Both sides used ESPHome 2026.9.0, ESP-IDF 5.5.5,
`esp-14.2.0_20260121`, Python 3.13.5, the same public build settings and the approved
two-track audio image. Exact 40-character source identities were supplied on both
sides. Absolute values are not directly comparable with earlier builds using a
development identity.

Production pairs use the same respective configurations. Qualification pairs
compare existing isolated firmware with the new private version/trust/component.
The Atom baseline explicitly disables firmware updates to match the new profile;
the Waveshare isolated baseline already disables them. A separate initial Atom
comparison with updates enabled is retained in private evidence and is not used
to estimate qualification overhead below.

| Profile | Before OTA bytes | After OTA bytes | Delta | Application budget remaining | Static RAM delta |
| --- | ---: | ---: | ---: | ---: | ---: |
| Atom production | 1,405,376 | 1,405,376 | 0 | 167,488 | 0 |
| Waveshare production | 1,409,104 | 1,409,104 | 0 | 163,760 | 0 |
| Atom dua qualification | 1,380,912 | 1,387,264 | +6,352 | 185,600 | 0 |
| Waveshare dua qualification | 1,384,656 | 1,390,992 | +6,336 | 181,872 | +16 |

All nine actual OTA images passed capacity, dependency, profile, flash-header,
partition and factory/application consistency checks. Both 2 MiB application
slots and the 3.5 MiB shared-audio partition are unchanged. Remaining application
budget is relative to 1,572,864 bytes; slot headroom is a further 524,288 bytes
larger. No recording is embedded in an application image.

## Remaining attended qualification

Use the [private qualification guide](dua-qualification.md) and linked hardware
runbooks. Refresh the host address/certificate, live speaker identity and
compatible application recovery before installation. Isolated preferences do not
isolate the physical media banks: preserve the complete current shared-audio
partition and production records before any authorized test writes.

Scheduled handoff, the two-second pause, audible Stop/mute/disable, offline and
reboot behavior, interrupted installation/recovery, scheduler continuity/lateness
during flash operations, and runtime heap/fragmentation/PSRAM remain unobserved
for this feature on both boards. Test-profile memory observations do not establish
clean-production runtime headroom. Firmware-update concurrency requires a
separate applicable profile/session because these profiles disable firmware
updates. This preparation does not establish release readiness or authorize a
device session.
