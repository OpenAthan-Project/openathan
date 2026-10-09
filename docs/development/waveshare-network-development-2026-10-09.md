# Waveshare network investigation and clean development candidate — 2026-10-09

The intermittent HTTP stall reproduced during an idle paired Python/Chromium
observation. Instrumentation located the Python failures before response
headers; the browser also reached its request deadline during that interval.
No firmware cause or fix has been established. The clean development profile
retains the existing implementation and its isolated storage/update restrictions.
The exact clean image was installed after attended preservation checks and
explicit application-only approval; the intermittent cause remains unresolved.

## Previously completed acceptance

The [earlier validation report](waveshare-validation-2026-10-09.md) retains full
normal/Fajr listening, scheduled Asr, cold-power persistence, moved-occurrence
replay protection and the completed 60-minute observation on the attended V2 unit.
Those results were not repeated for this investigation.

Reanalysis of the earlier hour identified four unplanned read timeouts and two
unplanned connect timeouts. Read failures did not retain enough detail to
distinguish delayed headers from delayed response bodies. The two planned
power-cut failures remain excluded from that count.

| Unplanned failure, UTC | Collector connection label | Error phase | Last observed playback |
| --- | --- | --- | --- |
| 19:50:03 | Keep alive | Read; headers/body unknown | Idle |
| 20:00:02 | Keep alive | Read; headers/body unknown | Idle |
| 20:00:15 | Keep alive | Connect | Idle |
| 20:07:30 | Connection close | Read; headers/body unknown | Playing |
| 20:10:09 | Keep alive | Read; headers/body unknown | Playing |
| 20:10:22 | Keep alive | Connect | Playing |

These are error-report times, not recovered request-start times. The successful
earlier browser observation began at 20:10:30 UTC, after the last failure;
it cannot establish how the browser behaved during the failing interval.
The retained encrypted telemetry shows only the planned cold-power reset and
no warning, error or connection event within 30 seconds of an unplanned failure.
Telemetry arrival around the 20:00 pair was delayed, with advancing uptime after
delivery resumed. This narrows the correlation but does not establish continuous
device responsiveness. Python and encrypted telemetry were known concurrent
clients; other clients were not inventoried.

## New method and observations

Private collectors retained UTC and runtime-local monotonic times, individual
Digest attempts, actual TCP reuse/connect events, response-header/body phases,
byte counts, browser network timings and encrypted device telemetry. Python kept
eight-second connect/read limits; the installed UI kept its twelve-second
deadline. Requests timeouts apply to transport operations, not total request
duration; see the [Requests documentation](https://requests.readthedocs.io/en/latest/user/advanced/#timeouts).
Authentication values were excluded from collector output.

Two independent Python sessions alternated reads approximately every five
seconds. This separates connection-policy comparison from the five-minute
Digest expiry. Controlled local probes verified classification of connect,
delayed-header and delayed-body failures before interpreting the device results.
The collectors blocked browser POSTs. No deliberate playback, USB opening,
reset, settings change or history change was part of the idle observation.

A short initial collector attempt ended before a valid paired window: Python
and Node monotonic clocks have different origins on this host. It was retained
as an interrupted attempt. A shared UTC start signal with independent monotonic
duration measurement corrected the method.

The completed paired idle window ran 21:42:39–21:52:39 UTC:

- Python completed 109 successful polls and two failures. Both failures followed
  request transmission and eight seconds waiting for response headers, one under
  each connection label. No body or connect failure occurred in that window.
- Chromium completed 312 HTTP 200 responses and six request-deadline failures.
  The browser failures overlapped the Python stall. There were no script errors.
- Forty encrypted telemetry samples showed no reset, playback, codec/display
  failure or warning. Telemetry arrival had a 37.346-second gap, followed by
  batched samples with advancing device uptime. This suggests a shared delivery
  interruption, but does not locate it in the device, access point or client.
- Both Python Digest renewals completed normally after the stall. The stall
  preceded nonce expiry; this occurrence was not an expired-nonce renewal failure.
- Settings, revision, consumption watermarks, display/light preferences, time
  format and active setup matched before and after the read-only window.

The `Connection: close` label did not create a fresh-connection comparison:
the traces showed TCP reuse despite that request header. The previous collector
therefore established header-policy results, not independent evidence that
new connections avoid or reproduce the fault. The confirmation method closes
the client adapter pool after every close-policy poll while preserving Digest
state; challenge retries may reuse a connection within one poll.

The additional confirmation window ran 21:53:14–22:03:14 UTC, with timestamped
device-only ICMP observations and verified pool closure. Python completed 114
successful polls (57 per policy), with no HTTP failure. Each close-policy poll
started a fresh TCP connection; its two Digest challenge retries reused the
connection within that poll. Chromium completed 319 HTTP 200 responses with no
request failure or script error. Both Digest renewals completed normally. There
was one ICMP timeout without a coincident HTTP failure. Forty telemetry samples
showed no reset, playback, codec/display failure or warning, and settings/history
again matched before and after. No compilation ran during either network window.

This passing window does not establish that closing connections resolves the
stall: the persistent client also passed, and the earlier event was intermittent.
The comparison additionally changes how many persistent sockets remain open.
There was no playback window because the failure already reproduced while idle;
the earlier full listening and playback-load evidence remains separate.

Device-filtered packet capture was unavailable in this session. Transport
location remains unresolved; synchronized deadlines alone do not prove a
firmware defect. Wi-Fi NONE remains an observed configuration, not a proven fix.

## Runtime and candidate boundaries

The paired idle measurements belong to the then-installed diagnostic overlay:
sampled internal free minimum 236,296 bytes, largest internal block 192,512 bytes,
and PSRAM free minimum 8,098,844 bytes. Final PSRAM free was 8,106,820 bytes and
largest PSRAM block 7,995,392 bytes. Its reported internal low-water mark
169,464 bytes was retained from earlier uptime; it is not a new-window minimum.
There was no playback in this window, so it provides no new recovery-after-audio
measurement and does not measure the clean candidate's runtime memory.

In the confirmation window, sampled internal free minimum was 238,056 bytes,
largest internal block remained 192,512 bytes, PSRAM free minimum was 8,103,936
bytes and final PSRAM free was 8,108,624 bytes. Largest PSRAM block remained
7,995,392 bytes. These are idle diagnostic-overlay observations, not a clean
candidate or concurrent-audio headroom qualification.

The clean entry point is `firmware/esphome/waveshare/development.yaml`. Generated
configuration retains ESPHome 2026.9.0 / ESP-IDF 5.5.5, the exact V2 hardware
identity, rotation 180°, isolated storage and disabled updates, and excludes the
diagnostic native API. The lower-8-MiB partition table, dual 2 MiB application
slots, 3.5 MiB audio partition and 1,572,864-byte OTA ceiling remain unchanged.

Three independent builds used source `bef8c404579fd1a063154464a39acb23230d6fee`,
the same pinned configuration and the existing `v0.4.0` / `development` identity:

| Build | Actual OTA bytes | Delta from rebuilt baseline | Application budget remaining |
| --- | ---: | ---: | ---: |
| Rebuilt clean display baseline | 1,256,256 | — | 316,608 |
| Compile-only clean comparison | 1,256,256 | 0 | 316,608 |
| Private installable clean candidate | 1,256,256 | 0 | 316,608 |

The exact installable OTA SHA-256 is
`d62faa0ace985ce21d95924bd42c2273289ee7888c5ef142472a2c7b74c4347a`.
It is 85,888 bytes smaller than the installed diagnostic overlay, leaving
840,896 bytes in its 2 MiB slot. Capacity inspection verified the dependency
pins, 16 MiB image headers, partition table, matching factory/OTA application,
isolated storage, disabled updates and absence of audio payloads in the application.
The installable artifact was inspected against the existing approved audio;
synthetic fixtures were confined to the separate compile-only builds.
Independent build timestamps and private credential inputs mean equal sizes do
not imply identical binary hashes.

All 17 UBSan host tests and 105 Python tests passed, including the settings
bridge against the resolved ArduinoJson dependency. The initial Python invocation
omitted paths to the out-of-tree host artifacts; correcting those paths produced
the complete passing suite. All 12 pinned configuration checks and 488 browser
cases across Chromium and WebKit passed, with no skips. No firmware, dependency,
public API or UI source changed. Atom and the audio-only/diagnostic Waveshare
profiles retain their previous source-bound measurements rather than a new
compile claim.

## Attended preservation and installation

Two complete stopped-device 16 MiB reads matched. The actual unit, disabled
Secure Boot/flash encryption, matching partition table, selected VALID app0 at
`0x10000`, current NVS records/integrity, OTA metadata and approved shared-audio
hash were freshly verified. The current diagnostic application matched retained
compatible recovery bytes. Settings, revision, consumption/skip identities and
preferences were captured privately; historical full-flash restoration is not
routine recovery because it would restore old settings/history.

After explicit approval, live idle/scheduling state, unit/security, selected VALID
slot, current application recovery and shared audio were rechecked. A single
application-only write installed the exact candidate above at `0x10000`.
Independent complete flash readback at 22:26:25 UTC verified the application and
every byte outside its `0x133000` sector erase extent before restarting. Bootloader,
partition table, NVS, OTA metadata, inactive application, shared audio and unused
flash were unchanged. No recovery write was needed. The vendor bytes in the
inactive slot remain an unqualified rollback target.

Authenticated startup and synchronized standalone scheduling returned, with
unchanged settings, revision, history and saved controls. The owner confirmed
upright presentation, saved 35% brightness, 12-hour time and “Use phone” guidance.
The exact installed identity is the candidate source/OTA SHA-256 above; the
native diagnostic API is absent. Clean runtime heap/PSRAM remains unmeasured.

A separate two-minute focused clean startup check completed 23 successful Python
polls and 65 Chromium HTTP 200 responses, with no request or script failure.
Settings/history/preferences matched across that interval. It verifies focused
startup behavior, not sustained reliability, Digest expiry renewal, concurrent
audio headroom or resolution of the intermittent cause. The earlier hour and
full listening acceptance were not repeated.

The [separate public-support plan](waveshare-public-support-plan.md) covers
production migration, qualified rollback, optional screen failure and
hardware-specific release/installer support. None of those phases is implemented
or published by this milestone.
