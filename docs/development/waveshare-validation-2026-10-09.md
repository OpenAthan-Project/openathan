# Waveshare Box V2 development validation — 2026-10-09

Implementation and the attended development session are complete. The isolated
encrypted diagnostic firmware is installed. Scheduled playback, cold-power
persistence, replay protection and a bounded 60-minute observation were checked
on this unit. Network reliability remains unresolved; this is development
evidence, not release qualification. See the [profile and procedure](waveshare-box-v2.md).

## Automated evidence

All measurements use pinned ESPHome 2026.9.0, ESP-IDF 5.5.5 and reviewed compiler
and managed dependencies. Actual OTA bytes are measured with the unchanged
lower-8-MiB partition table and 1,572,864-byte application limit.

| Configuration | Actual OTA bytes | Change | Budget remaining |
| --- | ---: | ---: | ---: |
| Atom reference baseline (`24c5db0`) | 1,274,176 | — | 298,688 |
| Atom reference with shared changes | 1,274,880 | +704 | 297,984 |
| Waveshare audio only | 1,207,680 | — | 365,184 |
| Waveshare round display, rotation 180° | 1,256,256 | +48,576 over audio only | 316,608 |
| Waveshare encrypted diagnostics | 1,342,144 | +85,888 over display | 230,720 |
| Isolated Atom provisioning | 1,276,384 | — | 296,480 |
| Atom upgrade qualification | 1,281,920 | — | 290,944 |
| Atom startup-failure qualification | 1,281,920 | — | 290,944 |
| Scheduler real device | 1,121,104 | — | 451,760 |
| Scheduler synthetic validation | 1,138,640 | — | 434,224 |

Compile-only comparisons use synthetic external audio-format fixtures, which
were never installed or played. The private attended diagnostic build also
measures 1,342,144 bytes, with its own encryption key. Runtime memory is separate
from these capacity checks.

All 17 UBSan CTests and the complete 105-test Python suite passed. Configuration
coverage includes 16 MiB headers, production-release exclusion, isolated storage,
disabled updates, round rendering and encrypted diagnostics with exclusive USB
provisioning ownership. The strengthened exact 180° rotation assertion also
passed when acceptance resumed.

All 488 Chromium/WebKit browser cases passed, including installed-version display
with unavailable update controls. All nine firmware variants in the CI matrix
compiled locally and passed capacity inspection. Updater host tests cover production,
isolated, qualification and disabled-update behavior. Disabled updates retain
healthy startup confirmation and failed-startup rollback, preserve an unrelated
saved journal, and reject HTTP/USB/automatic update paths without task creation,
application writes or boot selection. The resolved ESP-IDF HTTP request-format
regression passed. Round rendering checks every emitted pixel against circular
bounds and preserves Atom button guidance while using “Use phone” on Waveshare.

## Observed hardware and installation

The unit identified as ESP32-S3 revision v0.2, with detected 16 MB flash and
8 MB embedded PSRAM. Secure Boot and flash encryption were disabled. Two complete
16,777,216-byte pre-install backups matched SHA-256
`c3d7b379347978388819b6498f74f1c859ec8402d672cd5deb934c18afa5ccd8`.
The original image was Waveshare's `ESP32-S3-Touch-LCD-1.85C-Test`, version 1.
Backups, credentials, source snapshots, images and recovery commands are private.
An initial backup read failed and the complete read-only retry matched; it is
retained as a failed attempt, not counted as a valid backup.

Live probes found ES8311 at `0x18` and the expander at `0x20` on GPIO10/11;
neither responded on GPIO12/13. This resolves the schematic/source discrepancy
for this unit. Runtime telemetry reports 16,777,216 flash bytes and 8,388,608
PSRAM bytes, with no codec or LCD initialization failure.

The first development image and both approved recordings passed esptool write
verification. The 180° correction was then installed application-only at
`0x10000`. Its independent readback matched SHA-256
`f11bf6dbfd4631e9be46dc698d46206d4ddf94cc17d84476156fe7f9df18fa78`.
Bootloader, partition table, NVS/OTA control bytes, inactive application and shared
audio preservation were verified. Some bulk serial reads lost data; these were
reconciled with read-only checks, without repeating the application write.
The successful application readback used 115200 baud.

USB Wi-Fi/password provisioning succeeded in the initial diagnostic build using
approved private settings. Credentials, saved prayer settings, brightness and
12-hour time format survived the application update. The unit reports VALID
application state and updates disabled. Public update discovery remained idle.

The user confirmed upright orientation with the USB cable at the back, correct
red/green/blue test bars and return to status, and visibly increased brightness
from 35% to 70% without interrupting clear audio. Normal playback was inaudible
at 30%, then clear at 80% within the retained output cap. The user confirmed
phone Stop ended audible playback. After resuming, the user confirmed that an
80%→90% volume change became noticeably louder while remaining clear. The entire
normal recording and the entire Fajr recording completed, with the user confirming
clear sound and correct Fajr-specific wording. These full listening checks used
the LIGHT Wi-Fi configuration; the audio adapter and recordings are unchanged
in the subsequent NONE comparison.

Fresh USB status and update-info queries passed after resuming, without rewriting
credentials. The final diagnostic dispatcher reported startup confirmed and
updates unsupported; a USB begin-update command was rejected without writing.

The Wi-Fi NONE comparison was installed once at `0x10000`, then independently
read back as SHA-256
`4314ffb0d8a76a9ad0f910f10a6e9c8c709c4d5fd1e4fc9f857253ef86199128`.
Control/NVS, inactive application and shared audio bytes remained identical.
The retained inactive slot still contains vendor bytes and is not a qualified
fallback image; compatible application-only USB recovery is retained privately.

Real-clock scheduled Asr started at 15:54 under the temporary −17-minute offset;
the user confirmed clear audio and “Athan / Playing / Use phone” on the screen.
The user disconnected USB during playback, confirmed the screen went dark,
waited ten seconds and reconnected. Exact settings/revision, active setup,
consumption, brightness and time format survived; synchronization returned and
playback remained idle. Telemetry reported reset reason 1 (power-on), separately
from prior software/USB resets.

After that cold boot, the same consumed Asr was moved to 16:00 using a −11-minute
offset. Fourteen status polls and seven encrypted memory samples across the due
time reported synchronized automatic readiness and no replay. The original full
settings and zero offsets were then restored at revision 10; Asr consumption
was retained. Both recordings subsequently completed under Wi-Fi NONE, with
poll-observed completion intervals of 238.5 seconds (normal) and 270.7 seconds
(Fajr). These include command/poll latency; source durations are 233.55 and
268.07 seconds. These load plays supplement the earlier listening confirmations.

The installed UI passed a read-only phone-width Chromium check: installed version,
unavailable update controls, volume 80% and brightness 35% were correct. Twelve
status, nineteen firmware-state and one timezone response completed without a
failed browser request, script error or POST. A separate ten-minute observation
through authentication renewals completed 320 API responses, all HTTP 200, with
no failed requests, script errors or POSTs. This does not erase the separately
observed Python status-poll timeouts.

## Partial runtime observation

The bounded session was stopped at the user's request after approximately seven
minutes, from 18:15:50 to 18:22:47 UTC. It is **not a 60-minute soak pass**.
It recorded 63 successful authenticated status polls and four HTTP timeouts;
the timeout cause remains unresolved. Encrypted telemetry continued, with no
observed spontaneous reset or codec/display failure. One API operation exceeded
50 ms; no decoder or speaker error was observed in this short session.

Minimum sampled internal free heap was 217,232 bytes, the reported internal
low-water mark was 209,252 bytes, and the smallest sampled largest internal
block was 172,032 bytes. PSRAM free space ranged from 6,966,712 to 8,109,564 bytes;
its largest block ranged from 6,815,744 to 7,995,392 bytes. These short-session
measurements do not establish long-session fragmentation or recovery.

At pause, authenticated status confirmed idle playback, incomplete setup,
automatic readiness false, no scheduler fault, volume 80%, brightness 35%,
12-hour display and unchanged empty consumption history. The observer was stopped.

## Completed 60-minute observation

The final Wi-Fi NONE session ran from 19:48:12 to 20:48:12 UTC (3,600.1 seconds),
with 240 encrypted telemetry samples and intermittent audio, network and display
activity. It included the attended scheduled Asr and one planned cold-power cut,
then full normal/Fajr load plays and a final full normal play. It was not an hour
of continuous playback or uninterrupted uptime. The final normal load completed
in a poll-observed 236.8 seconds, with consumption unchanged.

There were 656 successful authenticated HTTP status polls and eight timeouts:
two during the planned power cut and six outside it. All successful responses
were HTTP 200. Encrypted telemetry reported only the expected power-on reset;
no unexpected reset, scheduler fault, codec/display initialization failure or
decoder/speaker error was observed. Three main-loop operations exceeded their
warning thresholds (173, 54 and 109 ms) during activation/startup/settings work.
Public update checks remained disabled and the last-check timestamp remained zero.

| Runtime measurement (bytes) | Minimum | Final idle |
| --- | ---: | ---: |
| Sampled internal free heap | 215,892 | 241,140 |
| Sampled largest internal block | 172,032 | 192,512 |
| Reported internal low-water mark | 169,464 | 169,464 |
| Sampled free PSRAM | 6,963,744 | 8,109,116 |
| Sampled largest PSRAM block | 6,815,744 | 7,995,392 |

PSRAM recovered after playback, and the largest PSRAM block returned to its idle
size. These samples establish this bounded run's observed headroom; they do not
establish indefinite operation or behavior under other network/load conditions.

## Network finding and remaining scope

The resumed LIGHT observation also had HTTP timeouts in both connection-reuse
and connection-close phases, while encrypted telemetry continued. A ten-packet
device ping lost 30%; the gateway control lost 0% of ten. With Wi-Fi power saving
disabled, the initial 20-packet device ping lost 0%, but an HTTP timeout still
occurred. This comparison does not establish the timeout cause or a complete
network fix. A separate 60-packet NONE ping during playback lost 0%, while the
completed hour still had the six unplanned HTTP timeouts described above. The
final board profile uses NONE and all three affected builds and the profile checks
passed again. The remaining timeout cause needs investigation before network
reliability can be qualified.

The scheduled-playback, cold-boot persistence and moved-occurrence replay checks
are complete. Original settings are restored and setup remains active; the next
unconsumed prayer is Maghrib. Do not repeat acknowledged provisioning or restore
historical NVS.

Physical display-failure injection, other panel lots, touch, microphones, RTC and
SD are unverified; the latter four capabilities remain deferred. Backlight
failure handling is covered by host tests; physical scheduler continuity during
a screen failure has not been tested. The installed diagnostic overlay includes
telemetry and explicit test buttons; the smaller development entry point was
compiled, not separately installed. The retained vendor bytes in the inactive
slot are not a qualified rollback target. Public installer changes, signed
Waveshare releases and update/recovery qualification remain separate work.
