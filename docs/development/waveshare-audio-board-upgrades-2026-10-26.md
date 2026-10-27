# AUDIO-Board private upgrade qualification — October 26, 2026

These checks use the same USB-powered AUDIO-Board as the
[October 21 hardware report](waveshare-audio-board-hardware-2026-10-21.md).
Its PCB revision marking remains unknown. Private qualification applications
`v0.0.1` through `v0.0.4` were built from
`32bc971d2f6e6d63f2fcb1cc0bc31be5f6df8fcc`, with isolated settings and update
records, a private LAN HTTPS feed and separate test signing material. They are
excluded from release packaging. The retained approved real recordings and
repository production-trust configuration were unchanged. Synthetic CI audio
was never installed or played.

## Builds and automated checks

All 19 checks passed for that source, including 17 firmware configurations,
host/Python/configuration checks and the device UI browser job. The local UBSan
suite passed 23 tests. Qualification feed, hardware binding, capacity and USB
transition tests passed. Subsequent maintainer-tool fixes supply the required
same-origin header for qualification controls and reject factory/OTA applications
whose embedded hardware identity differs from their selected profile. The new
AUDIO-Board requires that identity; older markerless Atom/Box bundles remain
accepted. The complete pinned Python suite of 176 tests passed without skips,
and all three production binaries plus four private candidates passed the updated
capacity/identity checks. All six three-board legacy/phone-setup fixtures passed
the actual pinned website consumer. These fixes change no firmware source or
configuration.

Matched production builds before/after the qualification extension used the same
ESPHome 2026.9.0, ESP-IDF 5.5.5, dependency locks, SDK configuration and
`development` source identity. All three OTA byte deltas were zero:

| Profile | Before/after OTA bytes | Delta | Application budget remaining |
| --- | ---: | ---: | ---: |
| Atom | 1,401,440 | 0 | 171,424 |
| Box V2 | 1,405,312 | 0 | 167,552 |
| AUDIO-Board | 1,369,168 | 0 | 203,696 |

The [original support comparison](waveshare-audio-board-validation-2026-10-21.md)
records the overall feature deltas against main: Atom -16 bytes, Box V2 +64 bytes.
The retained regular AUDIO-Board application is 1,373,488 bytes, leaving 199,376
bytes under the application limit. All four private qualification applications
are 1,380,592 bytes, leaving 192,272 bytes under the limit and 716,560 bytes
inside each 2 MiB slot. Their static RAM is 117,539 bytes. Both application slots
and the separate 3.5 MiB shared audio partition remain unchanged.

## Observed upgrade behavior

| Check | Observed result |
| --- | --- |
| Invalid signature | Rejected with the verification/incompatibility error; no erase, write, finalization or selection attempt. |
| Correct signature for the wrong board | Rejected with the same verification/incompatibility error and no flash operation. |
| Ten interrupted attempts | Two corrupt images, two short responses, two disconnected responses, three download cancellations and one cancellation during delayed headers retained the original VALID application. Each queue was explicitly cleared; no candidate was selected. |
| Actual server interruption | Stopping the private server after a partial download retained the VALID application and a cancellable request; no boot selection occurred. |
| Power loss before selection | A roughly 11-second measured USB outage at the verified pre-selection hold booted the old VALID application and retained its signed request. |
| Power loss after selection | The selected candidate booted PENDING and became VALID; the request cleared with `success`. This cut was owner-reported; the passive USB timing window had expired before the action. |
| Startup health failure | The intentionally failing `v0.0.3` remained PENDING and automatically rolled back to VALID `v0.0.2`, clearing the request with `rolled_back`. The observed pending-to-recovered interval includes reconnection and the old application's health confirmation. |
| Power loss while PENDING | A recent PENDING observation followed by a measured 11-second USB outage returned to the previous VALID application, cleared the request and preserved test settings/history. |
| Signed USB update | Every application chunk was acknowledged; FINISH verified and selected `v0.0.4`. The connection remained open until physical disconnection. A measured 12-second USB outage was followed by VALID `v0.0.4`, a cleared request and preserved test settings. |

The initial offline-server checks did not establish signature or hardware
rejection and were excluded. Test settings/history and light/dua preferences
were compared separately from protected production records.

## Preservation, stability and restoration

Fresh matching complete 16 MiB reads preserved the regular application's recovery
image before testing. A second complete matching pair after the ten interrupted
downloads confirmed unchanged production namespace payloads, approved shared
audio, bootloader and VALID selection. NVS CRCs and durable records were validated.
Historical full-flash backups are not routine recovery: restoring them would also
restore historical prayer consumption.

The final private `v0.0.4` application completed a full 3,600-second monotonic
stability run, from approximately 20:44 to 21:44 Toronto time on October 26.
There were 1,602 status samples, normally about two seconds apart, without a
collection error, reported scheduler fault or observed reset. Reset reason stayed
constant and the minimum-heap watermark never rose; the interface provides no
continuous uptime counter, so this is bounded observation rather than proof
against every possible reset.

Two real calculated prayer events were configured through ordinary isolated
settings and offsets, without changing the clock or production records. Fajr at
20:51 and normal Athan at 21:04 each advanced the corresponding isolated
consumption watermark and returned to ready. The owner heard both full recordings
clearly, including Fajr's morning phrase, and confirmed all seven LEDs pulsed
together. Dua was off during this run; the earlier attended dua result remains
separate. Sampled playing spans were 266.292 and 229.517 seconds respectively;
sampling boundaries are not exact decoder durations.

The device accepted 26 private signed-feed checks; four checks during playback
returned the expected busy response. Twenty-seven distinct completed-check
timestamps were observed, including the state at the start of collection.

| Metric | Minimum / maximum observed |
| --- | ---: |
| Sampled internal free bytes | 216,248 minimum |
| Sampled largest internal free block | 172,032 bytes minimum |
| Internal free watermark | 195,520 bytes minimum |
| Sampled PSRAM free bytes | 7,234,936 minimum |
| Sampled largest PSRAM free block | 7,208,960 bytes minimum |
| Internal fragmentation indicator | 20.65% maximum |
| Update worker stack free bytes | 3,952 minimum |

The fragmentation indicator is `100 × (1 − largest_free_block / total_free_internal_bytes)`
per sample. Initial/final idle medians were 239,672/239,304 internal free bytes,
with the same 196,608-byte largest block; PSRAM free medians were
8,372,756/8,372,568, with the same 8,257,536-byte largest block. These measurements
describe the private qualification image and this bounded session, not an exact
production release or an overnight soak.

After the soak, another fresh matching complete 16 MiB pair confirmed unchanged
production settings, password/activation records, prayer history, bootloader and
approved shared audio. The original regular application was then restored by
writing only its application region. Independent application and complete-flash
readback matched every expected byte, including unchanged partition table, OTA
metadata, inactive slot and unused flash. No historical NVS or full-flash image
was restored. The retained regular settings are revision 24 and 30% volume;
the isolated tests used 20%.

After the owner's physical RESET, read-only USB status confirmed the original
regular source/hardware identity, active setup, ready password storage, Wi-Fi
reconnection and confirmed boot health. Both its regular hostname and LAN page
returned their expected Digest authentication challenge after USB closed. The
final owner phone-page handoff remains pending at this checkpoint.

## Limits and remaining release work

The private first-run Wi-Fi wizard returned to setup for the owner. USB
provisioning subsequently joined home Wi-Fi and retained the connection across
restart; it does not establish a pass for that failed phone handoff. Earlier
phone setup observations retain their original source and date.

One esptool RTS restart after a read-only flash checkpoint did not regain LAN
access; physical RESET recovered it. The reset method's underlying cause remains
unresolved. Power-cut clock recovery is not evidence of battery-backed RTC
retention. Battery operation, microphones, SD storage, external displays and
maximum output remain outside these checks.

These results qualify the recorded private candidates on one unit. They do not
qualify an intended production release's signing/feed/redirect path, physical
public-website fresh-install/retry/recovery, other PCB revisions or an extended
overnight soak. Public availability still requires recorded release qualification
and explicit publication. Follow the
[upgrade runbook](../../firmware/esphome/upgrades/VALIDATION.md).
