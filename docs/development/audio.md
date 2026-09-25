# Audio Strategy

## Developer implementation

The current scheduler plays normal and Fajr recordings from a separately
provisioned shared audio partition. Saved volume is applied through the portable
playback adapter with readback; the reference board retains its 60% output
ceiling. Settings changes preserve current playback, and consumption is committed
before a scheduled recording starts. A crash after consumption can omit playback
but must not replay that prayer. See the [settings guide](../../firmware/esphome/scheduler/SETTINGS.md)
and dated [device validation](feasibility-report.md).

These developer results do not settle release recording selection, content
approval or redistribution rights.

## Initial release

The base device should support:

- one normal Athan;
- one Fajr Athan;
- local playback without a cloud service;
- independent Athan volume;
- clean interruption/dismiss behavior.

## Quran

Quran recitation should initially be streamed so that the required hardware remains inexpensive and simple. The AtomS3R's PSRAM can be used for network/audio buffering; PSRAM is not persistent storage.

Offline Quran is a later optional expansion and will require external persistent storage such as microSD or USB mass storage.

## Media licensing

Do not commit third-party Athan or Quran recordings to this repository unless redistribution rights are explicit and compatible. Audio-provider integrations should keep licensing and credentials separate from core firmware.
