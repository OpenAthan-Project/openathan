# Quran streaming

The product development firmware can play one MP3Quran surah on the speaker.
The local Quran page loads the live reciter catalog, searches across it, pages
through matching reciters, and offers each selected recording edition's available
surahs. This feature requires separate physical qualification before release;
see the [implementation validation](quran-streaming-validation-2026-10-10.md).

## Playback contract

Audio travels directly from MP3Quran to the device over verified HTTPS. The browser
only selects numeric reciter, edition and surah identifiers. Closing it does not
stop playback. One surah plays to completion, without a queue or automatic next
surah. Stop remains available while resolving or loading a recording.

Scheduled Athan replaces Quran using the existing single announcement pipeline.
Quran never resumes automatically after Athan, Stop, reboot, loss of Wi-Fi,
speaker failure or firmware maintenance. A late metadata result cannot restart a
session invalidated by Stop or Athan. Another Play replaces the previous Quran
session; Play is refused during Athan or another outstanding metadata request.
The physical Stop control uses the same playback adapter.

The UI distinguishes catalog feedback from authoritative playback feedback.
`loading` means the device is resolving or starting a request; `playing` observes
the player pipeline and is not proof of audible output. Decoder/reader failures
report an error; a queued request that never starts times out after 30 seconds.
An uncertain Play response triggers status readback and is never automatically
repeated. A Stop is confirmed only after both active playback and Quran loading
are absent from a fresh accepted status.

Browser storage remembers selection identifiers, without writing prayer settings,
consumption history or shared audio. Pause, seek, bookmarks, schedules, offline
downloads, automatic retries and additional providers are outside this version.

## Boundaries and implementation

The portable `Playback` capability supplies optional streaming, source, state and
session-epoch methods. Existing adapters default to unsupported streaming. The
core scheduler has no network or provider dependency. `openathan_device/Quran`
owns MP3Quran metadata and identifier resolution; `PartitionAudio` owns playback.
Waveshare's adapter retains its codec readiness and health gates. The optional
screen identifies Quran separately from Athan.

The authenticated local API queues one background metadata task. It parses one
reciter/surah object at a time, rather than retaining the complete raw catalog.
The ESPHome main loop alone starts playback and consumes completed results.
Cancellation revokes results immediately; a task already in a blocking network
call finishes before accepting another metadata request. Metadata browsing leaves
current audio running. Streaming requires completed setup, healthy settings,
Wi-Fi and a valid clock for TLS. Updates, USB maintenance and shutdown cancel it.

Bounds are 1 MiB per metadata response, 16 KiB per object, 1,024 reciters,
20 reciters per returned page, 32 editions per reciter, 114 distinct surahs,
256 bytes per name, 128 bytes per search and 32 KiB per normalized response.
Metadata requests have four-second I/O timeouts and a 30-second parsing/read-loop
deadline. Redirect opening and an in-flight blocking call finish within their
I/O timeouts before cancellation takes effect. Names and IDs come from the
provider; malformed or oversized responses
fail without substituting another recording. Search uses the English catalog and
ASCII case folding. An edition is playable only if its selected surah is listed.

Only `https://www.mp3quran.net/api/v3/` metadata URLs and audio URLs on
`cdn.mp3quran.net` or `server<N>.mp3quran.net` (one to three decimal digits) are
allowed. Audio paths reject credentials, explicit ports, queries, fragments,
escaped characters and traversal. The shared HTTP helper disables automatic
redirects, checks each absolute redirect before connecting, permits at most four,
and uses the ESP-IDF certificate bundle. Foreign hosts, insecure URLs, relative
redirects and newly introduced provider hosts fail explicitly. Playback requires
HTTP 200 and MP3 content type. No user-entered URL or provider credential is used.

The product package mirrors the pinned ESPHome 2026.9.0 `audio` and `speaker`
components because the original network reader followed redirects without the
provider restriction, and the player lacked a public pipeline-state observation.
Only `audio/audio_reader.cpp` and `speaker/media_player/speaker_media_player.h`
change upstream behavior, under `OPENATHAN_QURAN`. Stored-file playback retains
the upstream path. The generic scheduler profiles retain upstream components and
do not enable this feature. Each mirror includes its MIT license; the
[upstream manifest](../../firmware/esphome/components/audio/upstream.json) and
vendoring regression pin the unmodified files. An ESPHome upgrade must refresh
both mirrors, reconsider these two patches and rerun adapter and firmware checks.

## Local API

These routes use the existing device password, Digest authentication, origin
checks and main-loop exchange. Errors use the existing JSON `error` field.

| Route | Input | Result |
| --- | --- | --- |
| `POST /api/quran/catalog` | `{"kind":"reciters","offset":0,"query":""}` | Catalog state and request ID; starts bounded catalog/search loading. |
| `POST /api/quran/catalog` | `{"kind":"editions","reciter":1}` | Catalog state and request ID; loads that reciter's editions. |
| `GET /api/quran/catalog` | None | `idle`, `loading`, `ready` or `error`, request ID, error and completed data when available. |
| `POST /api/quran/play` | `{"reciter":1,"edition":9,"surah":18}` | Full device status; starts fresh identifier resolution. The example IDs illustrate the shape only. |
| `POST /api/stop` | `{}` | Full status after invalidating pending and active playback. |

Reciter data includes `reciters:[{id,name}]`, `surahs:[{id,name}]`, `total`,
`offset` and nullable `next_offset`. Edition data includes
`editions:[{id,name,surahs:[1,18]}]`. Pollers must match the returned request ID;
another client's catalog request must not be accepted as their own result.
No complete catalog is cached across requests or reboots.

`GET /api/status` adds `playback_source` (`none`, `athan`, `quran`) and optional
`quran:{supported,state,error,reciter,edition,surah}`. The Quran state is `idle`,
`loading`, `playing` or `error`. IDs describe the selected session, including
after it ends. These fields are transient observations, without durable revision
numbers; clients must preserve newer request observations. A successful Play
acknowledgment does not confirm playback. Unsupported builds can omit `quran`.

## Provider and licensing

Catalog and recording links come from [MP3Quran's API](https://www.mp3quran.net/eng/api).
The local page attributes MP3Quran. Its published
[developer-use policy](https://www.mp3quran.net/eng/privacy) permits application
use of its materials and links; recordings retain their own terms. This change
bundles no Quran recording, rehosts no provider media and establishes no right to
redistribute a future offline library. Software/documentation licensing remains
unchanged; copied ESPHome sources retain MIT attribution.

## Hardware qualification before release

Use the applicable [scheduler runbook](../../firmware/esphome/scheduler/VALIDATION.md)
or [provisioning runbook](../../firmware/esphome/provisioning/HARDWARE_TEST.md),
recheck the actual device and preserve settings, prayer history, shared recordings
and application recovery. CI fixtures are synthetic and must never be installed
or played. Qualification requires attended testing with source-bound application
images and real provider recordings:

- Verify several reciters, editions, MP3 sample rates/channel counts and complete
  playback, including a long surah and closing the browser.
- Interrupt resolving, loading and playing Quran with local and physical Stop,
  and with a due Athan. Verify exactly one Athan and no subsequent Quran restart.
- Exercise Wi-Fi loss, stalls, TLS/redirect rejection, malformed metadata, decode
  failure, reboot and firmware maintenance. Verify truthful status and explicit
  recovery through a new owner Play.
- Measure minimum internal heap, largest free block, fragmentation, PSRAM usage
  and metadata-task stack headroom during TLS, catalog parsing, streaming,
  status/UI polling and display activity. Compare long runs and repeated starts.
  Flash budget, static RAM totals and successful builds do not establish this.
- Confirm normal/Fajr playback, schedule/settings/history integrity, volume,
  optional displays and recovery still work after streaming tests.

Record observed hardware results separately from host, browser and build evidence.
