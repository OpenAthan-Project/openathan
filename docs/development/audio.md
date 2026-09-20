# Audio Strategy

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
