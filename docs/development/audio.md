# Audio Strategy

## Released Athan playback

The current scheduler plays normal and Fajr recordings from a separately
provisioned shared audio partition. Saved volume is applied through the portable
playback adapter with readback; the reference board retains its 60% output
ceiling. Settings changes preserve current playback, and consumption is committed
before a scheduled recording starts. A crash after consumption can omit playback
but must not replay that prayer. See the [settings guide](../../firmware/esphome/scheduler/SETTINGS.md)
and dated [device validation](feasibility-report.md).

The normal and Fajr recordings are selected and approved in the
[recording registry](../../release/recordings.json), with their exact bytes,
permission basis and attribution in [AUDIO-LICENSES.md](../../AUDIO-LICENSES.md).
The [current release summary](release-validation-2026-10-03.md) distinguishes
production listening from format checks and earlier source-bound qualification.

## Reference release

The reference firmware supports:

- one normal Athan;
- one Fajr Athan;
- local playback without a cloud service;
- independent Athan volume;
- clean interruption/dismiss behavior.

Application-only firmware updates retain installed recordings. A newer recording
in a fresh-install bundle does not replace audio on an existing speaker. There
is no owner-facing audio update flow; replacing shared audio requires a separately
reviewed preserving procedure. Keep recordings outside application slots.

## Quran

Quran recitation should initially be streamed so that the required hardware remains inexpensive and simple. The AtomS3R's PSRAM can be used for network/audio buffering; PSRAM is not persistent storage.

Offline Quran is a later optional expansion and will require external persistent storage such as microSD or USB mass storage.

## Media licensing

Do not commit third-party Athan or Quran recordings to this repository unless redistribution rights are explicit and compatible. Audio-provider integrations should keep licensing and credentials separate from core firmware.
