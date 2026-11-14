# Attended automatic-update qualification, 2026-11-13

The connected Waveshare Box V2 completed the bounded qualification in the
[upgrade runbook](../../firmware/esphome/upgrades/VALIDATION.md). Results apply
to this unit and source; they do not establish other-board or overnight acceptance.

## Source and preservation

Updater source: `fd46b9b289aa9c86540db21cfbd9dab1c248e717`.
The [exact-main CI run](https://github.com/OpenAthan-Project/openathan/actions/runs/43420980385)
passed before device access. Immutable private signed candidates v0.0.1–v0.0.4
used isolated test storage and qualification instrumentation. Production trust
and normal records remained separate. Final restoration used the merged
production profile; the v0.11.0 version-only release change was not installed.

Identity, disabled secure boot/encryption, 16 MiB flash, partition layout and the
allowlisted rollback bootloader were verified. Paired full checkpoints and fresh
paired scoped reads bound application-only writes to the stopped device.
Independent readback preserved normal settings, credentials, prayer history,
preferences, bootloader, control data, inactive slot, complete shared audio and
unused flash. Audio was never written, and no historical NVS or full-flash image
was restored.
Private raw observations and compatible application recovery were retained
outside Git.

## Update and recovery results

Eighteen device case receipts passed, including:

- Invalid-signature and corrupt-image rejection without boot selection.
- Three partial transfers retaining a retryable request, and three active-download
  cancellations without selection.
- Safe-window queuing and a safety check after delayed response headers.
- Power interruption before selection, after selection and during pending boot;
  selected control requests were denied and selection was not repeated.
- Forced unhealthy startup returning to the prior VALID application.
- Durable suppression, opt-out during automatic download and after restart, and
  a healthy automatic v0.0.4 update without an Install action.

Recorded interrupted attempts exceeded the runbook minimum ten; this does not
mean ten manual power cuts. An older VALID application remained available.
Missing automatic policy defaulted enabled. Saved opt-out and rejection skip
survived observed restarts.

The brief pending boot was not reachable over HTTP. The physical intervals,
saved ABORTED/VALID metadata and durable rollback result establish recovery,
not an exact instruction-boundary cut. The forced-startup candidate was already
suppressed by an earlier cancellation; hardware proves preservation of that skip.
Fresh checksum suppression was created on hardware and survived cold boots.
New rollback-suppression writes and exact NVS/selection faults retain the
[host-test coverage](automatic-update-recovery-2026-11-13.md).

## Continuous hour and playback

The uninterrupted run lasted 3,600.108 seconds, 3:33:53–4:33:53 PM EST, with
1,704 successful polls, zero network errors and zero unexpected resets.
Eleven permitted update checks succeeded; a check during Athan returned 409.
No erase, write, finalization or selection attempts occurred during the hour.
USB observations recorded no disappearance within that interval.

Full retained normal and Fajr recordings completed at 44% volume in 233.82 and
267.95 observed seconds. Earlier same-image listening confirmed both clear and
complete, including the correct Fajr wording; the repeats add device-state
evidence. Two earlier runs interrupted by owner-confirmed cable/hub disconnections
were retained separately and were not combined into this hour.

| Runtime measurement | Bytes |
| --- | ---: |
| Lowest sampled internal free heap | 195,712 |
| Device internal low-water mark | 184,464 |
| Smallest sampled largest internal block | 151,552 |
| Lowest sampled free PSRAM | 6,966,172 |
| Smallest sampled largest PSRAM block | 6,946,816 |
| Final idle internal free heap | 238,840 |
| Final idle largest internal block | 192,512 |
| Final idle free PSRAM | 8,110,372 |
| Minimum unused completed-worker stack | 3,920 |

After initial activity, five-minute idle medians held near 238,000 bytes of free
internal heap, with a 192,512-byte largest block and about 8.11 MB free PSRAM.
The initial largest block was 196,608 bytes; it settled to 192,512 without
progressive loss across the remaining samples. Transient TLS and playback
allocations recovered. These are qualification-image observations, not production
instrumentation or worst-case public TLS/redirect stack proof.

## Normal operation and remaining coverage

Normal firmware was authenticated healthy at 4:37:24 PM EST, more than fifteen
minutes before 4:53 PM Maghrib. Original settings, preferences and consumption
were retained, no catch-up playback occurred, and automatic updates were enabled
by default at revision 0. A fresh public signed-descriptor check reported current.
The earlier real 12:04 PM Dhuhr completed and was heard by the owner. The saved
2:31 PM Asr Skip was correctly consumed and cleared without playback.

The public descriptor's two HTTPS redirects were verified, but full public
application OTA delivery was not exercised. Exact v0.11.0 installation, Atom and
AUDIO-Board automatic-update hardware tests, overnight reliability and broader
network/peripheral conditions remain untested. The prior AUDIO-Board startup
time-synchronization issue is not resolved by this qualification.
