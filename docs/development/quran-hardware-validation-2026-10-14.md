# Quran hardware validation — 2026-10-14

MP3Quran playback, cancellation, controlled failure/recovery and scheduled Athan
preemption have been observed on Waveshare Box V2. This report consolidates the
completed development test sessions and their evidence limits for separate
release review. It adds no firmware, media, installation or publication change.

## Dated source and profile boundaries

| Date | Source | Installed test profile and coverage |
| --- | --- | --- |
| 2026-10-11 | `2cea368` ([PR #43](https://github.com/OpenAthan-Project/openathan/pull/43)) | Ordinary MP3Quran transport in isolated diagnostics, with a private stack observation: real-provider playback, Stop, Athan, Wi-Fi loss, software restart and complete Ya-Sin. |
| 2026-10-11 | `0aebd71` ([PR #45](https://github.com/OpenAthan-Project/openathan/pull/45)) | Private HTTPS qualification profile: certificate and metadata faults, blocked-metadata Stop and explicit recoveries. |
| 2026-10-14 | `2786ef7` | Private qualification profile: remaining metadata/audio faults. Invalid MP3 EOF reporting failed; the other nine cases passed. |
| 2026-10-14 | `38943c8` ([PR #47](https://github.com/OpenAthan-Project/openathan/pull/47)) | Fixed invalid-MP3 error/recovery and a new scheduled Athan preemption/admission-rejection session. |

The earlier results were not all repeated on `38943c8`. Private routing preserves
logical provider URL/redirect policy but verifies the private origin's certificate;
those sessions do not establish public-provider trust. The October 11 ordinary
transport session used the real provider's HTTPS path. All installed test profiles
used isolated storage and disabled firmware updates. Normal playback and recovery
used approved real recordings.
Synthetic CI/host fixtures were never installed or played.

## Provider playback and interruption

On October 11, complete Al-Fatihah recordings from Alafasi and Hussary finished
normally and were confirmed clear. Abdulbasit/Warsh playback was also heard clearly
before a scheduled Athan. Observed MP3 output was stereo at 44,100 and 11,025 Hz.
Catalog paging, search, edition selection and the 114-surah selection were exercised
alongside playback, authenticated status requests and the optional display.

- Complete Alafasi/Hafs Ya-Sin lasted about 17.5 minutes between playing/idle polls.
  The listener confirmed clear audio throughout and a normal finish; at least
  35 seconds of idle followed. This includes 70 runtime samples.
- Physical BOOT Stop during Ya-Sin stopped promptly and stayed stopped, as confirmed
  by the listener. Stop during metadata resolution invalidated the pending result.
  The later controlled blocked-metadata test retained idle/token state for 60.7
  seconds after the worker finished; its listening observation was unconfirmed.
- Wi-Fi interruption/reconnection retained the same boot identity. Device traces
  showed stopped/error playback, preserved settings/history and no automatic resume
  for 40 seconds. The listener reported being absent; audible confirmation is absent.
- Software restart during Quran produced a new boot identity, restored clock/scheduler
  readiness and preserved settings/history/preferences without resuming playback.
  At least 35 seconds of idle were observed; listening was unconfirmed.
- Manual Fajr replaced Quran, completed and stayed idle. The listener confirmed clear
  audio, Fajr-specific wording and a normal finish.

Physical power removal was already tested on Waveshare in the earlier Athan work:
power was cut during scheduled Asr, settings/history survived and the consumed
occurrence did not replay. The [October 10 volume validation](waveshare-volume-build-2026-10-10.md)
also verified a ten-second removal of all power and preserved settings/history.
These are prior physical power results; the Quran-specific restart above was a
software restart, not a newly observed power cut during streaming.

## Controlled faults and the EOF correction

| Fault group | Source | Observed result and listening boundary |
| --- | --- | --- |
| Untrusted and wrong-host certificates | `0aebd71` | Error before any HTTP request, released ownership, at least 30 seconds without resumption, explicit complete recovery. Both fault/recovery listening checks were confirmed. |
| Metadata stall, oversize, truncation and HTTP/foreign-host/loop redirects | `0aebd71` | Error, released ownership, at least 30 seconds without resumption and complete explicit recovery. Listening was unconfirmed. |
| Metadata 503/malformed JSON; audio 503/wrong MIME and HTTP/foreign-host/loop redirects | `2786ef7` | Error, released ownership, at least 30 seconds without resumption and clear complete explicit recovery, confirmed by the listener. |
| Truncated and stalled audio | `2786ef7` | Errors after 3.76 and 38.03 seconds respectively; no automatic resumption. Partial samples ended and both explicit full recoveries were confirmed clear and complete. Partial input is not a normal full-recording finish. |
| Invalid MP3 body | `2786ef7` → `38943c8` | The original silent `idle` result failed the error requirement. After the EOF fix, the same no-frame input reported an error in 4.49 seconds, released ownership and stayed stopped for 30.15 seconds with no new requests. Fresh full Al-Fatihah recovery completed in 55.00 seconds; silence and clear complete recovery were confirmed. |

Redirect rejection prevented a second request for HTTP downgrade/foreign hosts;
loops were bounded to five requests in the affected phase. Recovery always required
a distinct owner Play. The initial invalid-MP3 failure remains a failed observation;
its closure is the later source-bound retest, supported by the
[actual-decoder regression and matched builds](quran-mp3-eof-validation-2026-10-14.md).

## Scheduled Athan on the fixed source

On October 14, `38943c8` played Quran before a genuine-clock Dhuhr occurrence arranged
with an isolated −120-minute offset. One full normal Athan replaced it. The first
poll showing Athan ownership was 1.217 seconds after the scheduled time; this bounds
observation timing, not the exact physical start. The observed playing-to-finished
span was 233.15 seconds for the approved 233.55-second recording.

One current-token Quran Play during Athan returned HTTP 409. Its token still matched
afterward, Athan retained ownership and no Quran HTTP/fixture request started.
Completion was followed by 31.04 seconds of silence/no resumption across 28 samples.
Only the selected isolated Dhuhr consumption watermark advanced. The listener kept
the device page closed and confirmed clear Quran, one full Athan, then silence.
Original isolated offsets were restored without clearing the consumption record.

## Runtime and preservation evidence

| Session | Runtime observations |
| --- | --- |
| October 11 ordinary provider, complete Ya-Sin | Minimum sampled internal free/largest block 128,488/86,016 bytes; PSRAM free 6,834,760 bytes. Idle free heap changed 239,564 → 235,672 and largest block 147,456 → 124,928 bytes around playback/catalog/HTTP activity. |
| October 14 remaining faults | 484 status and 87 native samples; zero read errors and no observed codec/display faults or uptime regressions. Sampled internal free/largest 196,416/139,264 bytes; metadata minimum unused stack 3,136 bytes. |
| October 14 fixed EOF retest | 44 fault/recovery status and 18 native samples; zero read errors and no observed codec/display faults or uptime regressions. Sampled internal free/largest 198,012/143,360 bytes; metadata minimum unused stack 3,136 bytes. |
| October 14 scheduled preemption | 380 status and 55 native samples; zero read errors and no observed codec/display faults or uptime regressions. Sampled internal free/largest 216,300/143,360 bytes; low-water 174,320 bytes; PSRAM free/largest 6,972,520/6,815,744 bytes; metadata minimum unused stack 3,144 bytes. |

These are bounded diagnostic observations. October 11 had connectivity gaps and
planned resets across its wider collection; it was not a continuous reset-free soak.
The Ya-Sin idle heap difference is an observation, not a leak finding or a leak-free
acceptance claim. Sampling can miss transient minima and other task stacks were
not measured. Clean production runtime headroom and longer repeated-start behavior
are not established by these measurements.

The fixed-source private qualification image was 1,377,536 OTA bytes with 195,328
bytes remaining against the 1.5 MiB application budget. It passed the pinned capacity
checker; this is an installed test-image measurement, not a publishable artifact.
The separate matched comparison records the fix's +32-byte qualification delta and
zero static RAM delta. No new builds or size changes accompany this documentation.

Each installation/restoration used fresh paired full-flash snapshots, a compatible
application-only recovery and independent complete readback. Protected production
settings, credentials, current consumption history, shared audio and unaffected
flash were preserved. On October 14 the entire original selected 2 MiB application
partition was restored, including original trailing bytes in the erase extent.
SDK and isolated-test records could change normally; no historical full-flash
restore was used. Final production observations were active, synchronized,
automatic-ready and idle with original settings/history/preferences preserved.
The installed application returned to the original v0.5.0; these tests did not
publish or install a new production release.

## Handoff to separate release review

Completed passes retain their date, source and profile. Mono/other channel counts,
Quran interactions with firmware maintenance and other reference hardware are not
covered by this Waveshare record. Scheduled handovers used isolated offsets; an
unshifted production occurrence was not newly observed. Wi-Fi/software-restart
listening and power removal specifically during Quran remain unconfirmed, with
their existing device/power observations recorded above.

Release review can assess those limits against its candidate and other changes.
This report schedules no additional hardware session. Raw source/build identities,
paired backups/readbacks, request traces, measurements and listener replies are
retained in private maintainer archives outside Git; credentials, local paths,
device identifiers and recordings are not included here.
