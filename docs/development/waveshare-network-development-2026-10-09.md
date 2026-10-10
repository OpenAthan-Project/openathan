# Waveshare network findings and clean installation — 2026-10-09

The intermittent HTTP failure reproduced while idle, with Python waiting for
response headers and Chromium reaching its request deadline. The cause remains
unresolved; no firmware fix was made. The existing clean development image was
validated and installed with attended approval. Detailed diagnostics, credentials,
recordings and recovery artifacts remain private.

The later [BOOT validation](waveshare-buttons-validation-2026-10-09.md) records
physical button acceptance and the subsequent clean development installation.
Its image record supersedes the installed-image snapshot below; the network
finding remains unresolved.

## Findings and limits

A ten-minute simultaneous Python/Chromium observation produced two Python
response-header timeouts and six overlapping browser deadlines. Python retained
its eight-second connect/read limits; Chromium retained its twelve-second
deadline. Request duration was measured separately because Requests timeouts are
not total-duration limits; see the [timeout documentation](https://requests.readthedocs.io/en/latest/user/advanced/#timeouts).
Encrypted telemetry showed a delivery gap with advancing uptime, without an
observed reset or hardware fault. This does not locate the interruption.

The initial `Connection: close` requests still reused TCP connections. After
correcting the collector to close its connection pool, a second ten-minute window
passed 114 Python polls (57 per policy) and 319 Chromium HTTP 200 responses.
Both persistent and fresh connections passed, so this is not evidence of a fix.
Device-filtered packet capture was unavailable. Wi-Fi NONE and unchanged client
timeouts do not establish resolution.

The [previous acceptance](waveshare-validation-2026-10-09.md) retains normal/Fajr
listening, scheduled Asr, cold-power persistence, replay protection and the hour
of observation. Those checks were not repeated. Further network testing awaits
a specific hypothesis; public-support work remains deferred.

## Exact installed image and validation

Firmware source: `bef8c404579fd1a063154464a39acb23230d6fee`, using
`firmware/esphome/waveshare/development.yaml` without the diagnostic overlay.
ESPHome 2026.9.0 / ESP-IDF 5.5.5, isolated storage, disabled updates, rotation 180°,
standalone scheduling, shared recordings and the lower-8-MiB layout are retained.
Additional flash remains unused.

- Actual OTA: **1,256,256 bytes**, matching the rebuilt clean baseline: **0-byte
  delta**, **316,608 bytes** remaining under the 1,572,864-byte limit.
- SHA-256: `d62faa0ace985ce21d95924bd42c2273289ee7888c5ef142472a2c7b74c4347a`.
- Three clean display builds passed capacity inspection. Synthetic fixtures were
  confined to compile-only builds. No firmware/shared code changed; other variants
  retain their earlier source-bound validation.
- 17 UBSan host tests, 105 Python tests, 12 pinned configuration checks and 488
  Chromium/WebKit cases passed.

Matching current-device reads verified the selected VALID app0 at `0x10000`,
settings/history, security, partitions, compatible application recovery and audio.
After explicit approval, one application-only write installed the image above.
Independent complete flash readback verified its bytes and every byte outside the
`0x133000` sector erase extent before restart. Bootloader, partition table, NVS,
OTA metadata, inactive application, shared audio and unused flash were preserved.
The inactive vendor image remains an unqualified fallback.

Authenticated synchronized startup and saved settings/history passed. The owner
confirmed upright presentation, 35% brightness, 12-hour time and “Use phone”
guidance. A two-minute clean startup check passed 23 Python polls and 65 Chromium
HTTP 200 responses without failures. It does not establish sustained reliability
or resolution of the intermittent failure. Exact clean runtime heap/PSRAM remains
unmeasured. This is development evidence; public release qualification is pending.
