# Atom / Voice Pyramid volume calibration — 2026-10-11

The AtomS3R / Voice Pyramid product profile previously sent the slider directly
to the ES8311 with a 60% ceiling. Under the pinned ESPHome mapping, even 100%
selected register 153, or -19 dB. The calibrated profile reaches register 191
(`0xBF`, 0 dB) at 100%, with software attenuation for everyday volume control.
This removes 19 dB of digital attenuation at the top; audible output and
distortion at the new maximum remain untested on the physical Pyramid.

## Gain and startup contract

The curve is `-96.5 × (1 - percent / 100)` dB for percentages 1–100. Zero uses
both software mute and the codec mute. From 50% upwards the codec stays at
0 dB. Lower percentages add codec attenuation to preserve the old very quiet
settings beyond ESPHome's -49 dB software range. Neither stage amplifies samples
or sets positive codec gain. DAC dynamic-range compression is explicitly disabled.

The media player starts muted and the GPIO expander's amplifier enable starts
off. The adapter waits for the audio partition, clock and amplifier initialization.
For every volume change it disables the amplifier, requests software mute and
waits for readback. Only then does it apply codec mute, DRC and gain settings,
verify the gain/DRC registers, and request the final software level. Amplifier
enable and new playback remain gated until that level reads back. Codec write or
readback failure leaves the adapter failed with the amplifier disabled; dropped
media-player commands are retried while gated. A live adjustment briefly mutes
the output without stopping or replaying the recording.

The mappings are checked against the [ES8311 register guide](https://files.waveshare.com/wiki/common/ES8311.user.Guide.pdf)
and pinned ESPHome 2026.9.0 sources for the
[ES8311](https://github.com/esphome/esphome/blob/2026.9.0/esphome/components/es8311/es8311.cpp),
[I2S speaker](https://github.com/esphome/esphome/blob/2026.9.0/esphome/components/i2s_audio/speaker/i2s_audio_speaker.cpp)
and [media player](https://github.com/esphome/esphome/blob/2026.9.0/esphome/components/speaker/media_player/speaker_media_player.cpp).
[M5Stack's A167 documentation](https://docs.m5stack.com/en/atom/Voice_Pyramid)
identifies its ES8311 and AW87559. The verified clock and amplifier register
initialization are retained; this change does not introduce an amplifier boost.

## Saved levels and older firmware

On upgrade, each saved legacy percentage is converted to the greatest integer
on the new curve that is no louder than its old register gain. The rounding
difference is less than 0.966 dB. Zero remains muted; a positive legacy setting
remains positive. Fresh devices start at 56%, equivalent to or slightly quieter
than the old 70% default.

| Legacy setting | Calibrated setting | Legacy gain | Calibrated gain |
| --- | ---: | ---: | ---: |
| 20% | 16% | -80.5 dB | -81.06 dB |
| 70% | 56% | -42 dB | -42.46 dB |
| 80% | 64% | -34.5 dB | -34.74 dB |
| 100% | 80% | -19 dB | -19.3 dB |

The existing CRC-protected v1 `settings` record remains readable by older
firmware. A second v1 record at `pyr_volume` stores the calibrated snapshot.
It is authoritative only when its complete derived legacy mirror, including
revision and all other settings, matches `settings`. Saves commit the quiet
legacy mirror first, then the calibrated record. A power cut can retain the old
snapshot, the complete new snapshot, or the new snapshot with a quieter mirrored
volume; it cannot assemble unrelated fields from the two records.

Returning to older firmware loads the mirror, capped at that firmware's existing
maximum. If older firmware saves settings, the next calibrated boot recognizes
the mismatch and migrates its current snapshot again. Boot migration writes
neither settings nor prayer history. It advances the exposed revision once to
reject an old browser's stale save. Corrupt companion records and exhausted
migration revisions block settings and automatic playback while retaining NVS.

The scheduler's synthetic validation profile uses this same store in
`oa_validation`; product and provisioning profiles retain their existing
production or isolated namespaces. Consumption, skips, shared audio and other
preferences are independent of the companion record. The generic hardware
package retains its ordinary ESPHome DAC binding; calibration is selected by the
separate product package. Core prayer logic and the public website do not acquire
a hardware dependency.

## Automated validation

All 20 CTests passed with UndefinedBehaviorSanitizer, including production and
isolated calibration fixtures. They cover every legacy percentage, effective
two-stage gain, stale revisions, companion/mirror matching, older-firmware saves,
power cuts at both writes and commits, early persistence, storage corruption,
namespace separation, dropped volume commands and codec mute/gain/DRC failures.
The full pinned Python suite passed all 113 tests, including product volume
routing, synthetic validation storage and generic custom hardware configuration.
All 488 Chromium/WebKit cases passed with Node 24.19.0 and Playwright 1.62.1.
All 13 ESPHome schemas, the pinned website installer contract and the production
upgrade runtime/resolved HTTP regressions passed. Browser endpoints and NVS/I2C
fixtures are simulated; these checks do not establish physical output quality.

Matched OTA measurements use main revision `e4304465` before this change and
the calibrated source afterwards. Both snapshots use identical compile-only
fixtures, ESPHome 2026.9.0, ESP-IDF 5.5.5 and the pinned compiler. These source
snapshots do not carry a release Git identity; the measurements compare the
same generated identity in both builds and are not published-release sizes.

| Variant | Before OTA bytes | After OTA bytes | Delta | Remaining 1.5 MiB budget |
| --- | ---: | ---: | ---: | ---: |
| Atom reference | 1,277,360 | 1,281,568 | +4,208 | 291,296 |
| Atom real schedule | 1,121,296 | 1,125,488 | +4,192 | 447,376 |
| Atom synthetic timetable | 1,138,832 | 1,143,040 | +4,208 | 429,824 |
| Atom provisioning validation | 1,278,800 | 1,283,056 | +4,256 | 289,808 |
| Atom upgrade qualification | 1,284,336 | 1,288,656 | +4,320 | 284,208 |
| Atom startup-failure qualification | 1,284,336 | 1,288,656 | +4,320 | 284,208 |
| Waveshare production | 1,284,752 | 1,284,912 | +160 | 287,952 |
| Waveshare audio | 1,213,344 | 1,213,504 | +160 | 359,360 |
| Waveshare display | 1,263,312 | 1,263,472 | +160 | 309,392 |
| Waveshare diagnostics | 1,350,224 | 1,350,384 | +160 | 222,480 |

All ten variants passed dependency, partition, image and capacity checks.
The Waveshare mapping is unchanged; its 160-byte delta comes from the shared
audio/default-volume bridge changes.

The 1.5 MiB application limit, both 2 MiB OTA slots and the separate 3.5 MiB
shared audio partition are retained. Compile-only fixtures must never be
installed or played. The adapter adds no audio task, sample buffer or dependency.
Reference static RAM increases by 152 bytes; runtime heap, fragmentation and
PSRAM during networking and playback remain unmeasured.

## Physical acceptance still pending

No firmware was installed and no audio played during this implementation.
Earlier Atom listening results apply to the old mapping; the
[Waveshare results](waveshare-volume-build-2026-10-10.md) apply to different hardware.

Use the [scheduler runbook](../../firmware/esphome/scheduler/VALIDATION.md) or
[provisioning runbook](../../firmware/esphome/provisioning/HARDWARE_TEST.md)
for an attended application-only update with approved real recordings. First
recheck the live identity, settings, history, audio digest and compatible recovery.
Verify the migrated level before playback. Start at that level, increase in
bounded steps, and stop if the listener reports distortion or uncomfortable
output. Check zero mute, live adjustment, Stop, both recordings and a cold restart
with unchanged settings/history/audio. Record register/output observations and
runtime memory separately. This build evidence does not qualify a release or
establish the Pyramid's usable maximum loudness.
