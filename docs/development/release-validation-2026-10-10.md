# v0.5.0 release qualification — 2026-10-10 Toronto

[v0.5.0](../releases/v0.5.0.md) is published as stable/latest from
`9e47867facd06164ede96a77859a77896cdb4123`. It adds Waveshare Box V2 alongside
AtomS3R C126 + Voice Pyramid A167. The [public installer](https://openathan.com/install/)
serves both hardware bundles. Qualification below covers one attended Waveshare
V2 unit with 16 MiB flash and 8 MiB PSRAM; earlier Atom evidence retains its
original scope.

## Production artifacts and automated checks

Exact-source [firmware CI](https://github.com/OpenAthan-Project/openathan/actions/runs/38067247571)
passed all 12 jobs. Both production builds passed pinned ESPHome 2026.9.0 /
ESP-IDF 5.5.5 dependency, image, partition, recording-exclusion and capacity
checks. Each actual OTA image remains below the 1,572,864-byte application limit.
Both 2 MiB slots and the separate 3.5 MiB shared audio partition are retained.

| Production board | OTA bytes | Application budget remaining | Free bytes per 2 MiB slot |
| --- | ---: | ---: | ---: |
| AtomS3R + Voice Pyramid | 1,277,392 | 295,472 | 819,760 |
| Waveshare Box V2 | 1,284,784 | 288,080 | 812,368 |

These exact version/source-identified images are each 32 bytes larger than the
corresponding [prepared production build](v0.5.0-preparation-2026-10-10.md#matched-ota-measurements).
The private qualification applications are separate images with isolated records,
private update trust and test controls; none enters the public release.

## Attended Waveshare results

| Check | Observed result |
| --- | --- |
| Fresh installation | Browser erase/install completed; complete 16 MiB readback verified production firmware and approved audio. Physical RESET showed setup needed. |
| Local setup and USB identity | Owner completed Wi-Fi, password and settings setup. Compatible OpenAthan reported Waveshare Box V2 and Chrome recognized it automatically. Unknown firmware required explicit model selection. |
| Invalid signature | Private-feed descriptor was rejected without queuing an installation or erasing/writing an application. |
| Signed Wi-Fi update | Healthy private v0.0.2 transferred, switched slots and moved from PENDING to VALID; the queue cleared. |
| Power cut before selection | After the full target was finalized but before selection, attended power removal/reconnection retained the old VALID image and durable queue. A controlled truncated retry remained unselected and cancellation cleared the queue. |
| Failed startup | Private v0.0.3 remained unconfirmed and automatically rolled back to the previous VALID v0.0.2; the rejected request cleared. |
| Production preservation and restoration | Fresh paired reads verified production records and complete shared audio. One application-only restoration installed the exact v0.5.0 application; independent readback protected the other flash regions. Production startup and saved-state reconciliation passed. |

After installation was verified, the browser could not reconnect automatically
for setup. Physical RESET and USB reconnection completed the setup flow without
reinstalling. BOOT-held download mode was used during installation preparation.

Fresh installation deliberately erased the previous device data. Subsequent
preservation checks use the newly configured production baseline: settings,
credentials, preferences and audio were preserved through isolated OTA testing.
Prayer consumption did not regress; the one naturally elapsed production prayer
advanced its watermark. Historical NVS/full-flash restoration was not used.

The bounded power observer did not capture the exact unplugged duration or screen
darkness. The owner response and fresh power-on reset state support the recovery
result. These cases are a focused Waveshare extension, not a repeat of the entire
earlier [Atom interruption matrix](upgrade-qualification-2026-10-01.md).

212 quiet isolated OTA samples recorded internal free/largest-block minima of
194,676 / 143,360 bytes, internal low-water 192,640 bytes, PSRAM free/largest-block
minima of 8,031,200 / 7,995,392 bytes and updater unused-stack minimum 3,952 bytes.
Announcements were disabled; these observations do not establish concurrent
audio/network/display headroom or clean-production runtime memory.

## Reused hardware evidence and limits

The release RTC adapter files match tested source `1378dc2`, and the audio adapter
files match tested source `96670c1`, byte for byte. Reuse covers
[powered offline RTC RESET](waveshare-rtc-validation-2026-10-10.md#attended-powered-reset-validation),
[volume/listening and cold restart](waveshare-volume-build-2026-10-10.md), and
the separately dated [BOOT controls](waveshare-buttons-validation-2026-10-09.md)
and [development playback/scheduling/soak](waveshare-validation-2026-10-09.md).
Those results retain their tested images and observation gaps; no new full
listening session, real scheduled announcement or hour-long soak is claimed here.

RTC battery-backed unplugged retention, long-term drift and audible scheduled
playback using RTC-only time remain unqualified. Other display-panel lots and
broader hardware/browser/network coverage need separate validation. Earlier
intermittent HTTP timeouts remain unresolved. Touch, microphones and SD storage
remain deferred. Preserving browser USB updates require separate transport
qualification; public USB application writes remain disabled.

## Public delivery

All 13 unauthenticated GitHub downloads matched the immutable validated bundle;
both board signatures and the combined artifact contract passed. The
[website deployment](https://github.com/OpenAthan-Project/website/actions/runs/38098999421)
at source `358079bdac8b9d045c2dd50b6d58021548a3fd56` passed 180 unit tests,
369 development browser tests, 171 built-site browser tests and the release
freshness gate. Live metadata selected the correct manifests for both boards,
and all nine website-imported assets matched the release bytes.

The restored production Waveshare checked the real public signed descriptor and
reported v0.5.0 current, with no error or installation queued. Settings, history
and preferences matched the before-check state; clock and automatic readiness
were true. This establishes the device's public descriptor path, not a new
public-feed application-transfer test. Raw diagnostics, snapshots, credentials
and private qualification material remain outside Git.
