# Phone setup, discovery and update validation — 2026-10-19

Attended checks completed the prayer-settings wizard, cold restart and webpage
rediscovery on one AtomS3R C126 with Voice Pyramid A167 and one Waveshare
ESP32-S3-Touch-LCD-1.85C-BOX V2. Both speakers were subsequently updated through
their Update pages and reported production v0.7.0 with their saved settings intact.

## Sources and scope

The setup checks used hardware-installable development images built from released
source [`9d7e87ce2a438bb7805967059ed3264a46cf273b`](https://github.com/OpenAthan-Project/openathan/tree/9d7e87ce2a438bb7805967059ed3264a46cf273b):
[Atom provisioning validation](../../firmware/esphome/provisioning/validation.yaml)
and [Waveshare development](../../firmware/esphome/waveshare/development.yaml).
These images used isolated test storage and unchanged approved shared audio.
Their complete flash readbacks establish preservation for those development
images; they do not establish installation of the exact published binaries.

The later production update observations concern the
[published v0.7.0 release](https://github.com/OpenAthan-Project/openathan/releases/tag/v0.7.0).
They consist of owner-reported updates and authenticated status comparisons,
as distinguished below. No firmware logic, dependencies, partition layout or
hardware mappings were changed for these checks.

## Setup and discovery results

| Check | Atom | Waveshare |
| --- | --- | --- |
| iPhone Location → Calculation → Review → Finish setup, then reload | Completed | Completed |
| Selected timezone independently matched the reported choice | Passed | Passed |
| All five prayers independently confirmed enabled | Passed | Not independently confirmed |
| Actual ten-second removal of all power, followed by a new boot identity | Passed on Pyramid bottom power with Atom USB disconnected | Passed |
| Active setup after cold restart | 25 successful authenticated reads over at least 120 seconds | 25 successful authenticated reads over at least 120 seconds |
| Webpage QR after restart, using the iPhone camera without a bookmark or existing tab | Front-button QR opened the page; setup stayed complete and refresh worked | BOOT-button QR opened the page; setup stayed complete and refresh worked |

Each restart observation retained one boot and zero steady-observation read
errors. Setup remained active, settings were applied, the clock and automatic
scheduler were ready, and a future prayer was scheduled. The complete frozen
post-Finish settings, revision and saved preferences persisted. Other wizard
choices were not independently transcribed before Finish; persistence was
checked against the full saved post-Finish snapshot.

The exact URLs opened from the webpage QRs were not reported. These results
therefore establish page rediscovery without adding a hostname-specific access
claim. Earlier setup Wi-Fi QR, captive-page and manual-entry checks retain their
dated scope in the [October 17 reconciliation](v0.7.0-preparation-2026-10-17.md#reconciled-attended-observations).

## Preservation and recovery during setup testing

Fresh paired complete flash captures, compatible application-only recovery and
independent complete readbacks protected production records and shared audio on
both speakers. The original applications were restored before the production
updates. Recovery preserved current post-test NVS and bytes outside the selected
application slot; no historical NVS or full-flash snapshot was replayed.

Test activation, settings and isolated consumption records changed as expected.
Waveshare also had framework system-state changes, so the testing does not claim
that all NVS bytes remained unchanged. Protected production records were compared
separately. Skip was not independently exercised, and no new listening result is
claimed. The [provisioning hardware runbook](../../firmware/esphome/provisioning/HARDWARE_TEST.md)
and [scheduler validation runbook](../../firmware/esphome/scheduler/VALIDATION.md)
describe the preservation boundaries.

## Production update observations

The owner updated both speakers through their device-local Update pages.
Subsequent authenticated read-only status checks reported production mode,
version v0.7.0 and the expected released commit above, with the updater current,
no error and no queued update. Both speakers had active setup, applied settings,
Wi-Fi, clock and automatic scheduling ready, idle playback and a future prayer.

Saved settings and revision, display preferences, time format, saved light
preferences and exposed prayer-consumption history matched each speaker's prior
production baseline. Existing authentication continued to work.

The update transfers were not independently observed, and post-update physical
flash contents and audio hashes were not read back. These status comparisons
support the reported production update and settings retention; they do not
extend the isolated development-image readback results to the exact published
application bytes or establish exact published-image recovery.

## Remaining limits

The earlier Galaxy S9 / Android 10 `.local` failures remain unresolved; opening
the speaker's displayed current IP address worked in those dated checks. Samsung
was not retested on October 19. See the
[October 15–16 observations](v0.7.0-preparation-2026-10-17.md#reconciled-attended-observations)
for the board-specific results.

Exact published factory-image fresh installation and recovery, interrupted
enrollment and configured-outage/no-setup-AP behavior are not new passes from
this session. Runtime heap, fragmentation, PSRAM, stack, simultaneous
audio/network/display activity and extended streaming remain unqualified by
these checks. No new audio-quality, scheduled listening or broader hardware
acceptance is inferred.
