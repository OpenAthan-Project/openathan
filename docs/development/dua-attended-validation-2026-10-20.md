# Dua attended validation — 19–20 October 2026

The private qualification firmware from
`5f701d27caf94efb914b5a88e1ad971929d8978e` was exercised on the reference
AtomS3R C126 + Voice Pyramid A167 and Waveshare Box V2. Both used isolated
settings, credentials, activation, consumption and dua intent, with the real
clock and calculated prayer events. Normal/Fajr recordings and the approved
97,700-byte, 21.446531-second dua were unchanged. No synthetic media was installed
or played. This supplements the earlier
[host preparation](dua-qualification-validation-2026-10-19.md).

## Observed results

| Case | Atom | Waveshare |
| --- | --- | --- |
| Complete scheduled normal Athan, pause and one dua | Owner-confirmed clear and complete; sequence trace passed | Owner-confirmed clear and complete; sequence trace passed |
| Stop during Athan cancels the later dua | Owner-confirmed immediate silence; remained silent through the canceled window | Not repeated in this session |
| Stop during dua | Owner-confirmed immediate silence; trace ended idle and stayed silent | Owner-confirmed immediate silence; trace ended idle and stayed silent |
| Quran request during Athan and the silent pause | Both rejected with HTTP 409; no queued Quran | Both rejected with HTTP 409; no queued Quran |
| Invalid signature and corrupted downloaded payload | Rejected without making media ready | Rejected without making media ready |
| Disable while network work is outstanding | Canceled and settled off | Canceled and settled off |
| Power loss during partial Wi-Fi installation | Enabled intent survived; incomplete media remained unavailable | Enabled intent survived; incomplete media remained unavailable |
| Transient failure after reboot | Automatic retry after about 301 seconds; verified media ready | Automatic retry after about 301 seconds; verified media ready |
| Offline shared Isha/Fajr occurrence | Owner heard full Fajr and one complete, clear dua | Owner heard full Fajr and one complete, clear dua |
| Shared consumption | Both identities advanced; unrelated consumption unchanged | Both identities advanced; unrelated consumption unchanged |
| Prayer ring | Not separately observed here | Owner confirmed visible during normal Athan, dua and afterward |
| Production restoration | Original application/audio, preferences and current history verified; healthy scheduling | Original application/audio, preferences and current history verified; healthy scheduling |

The online Stop traces sampled pauses of about 1.95 seconds on Atom and 2.10
seconds on Waveshare; status polling and request latency limit these observations.
Owner listening is separate evidence of the audible recordings and pause.

Waveshare stayed disconnected from Wi-Fi throughout its full offline event and
completion margin, verified through a continuously open USB status connection.
Atom requires bottom-only power for audio; its offline result combines the
owner's hotspot-off confirmation, listening and pre/post device state. There was
no simultaneous Atom USB phase trace. Both devices consumed the shared Fajr and
Isha identities exactly once and retained valid media after reboot.

The first Atom interruption attempt completed before the power-cut cue and is
not a pass. Only its later, observed partial-transfer cut is counted. The first
Waveshare hotspot credential attempt explicitly failed and retained prior test
credentials; a later attempt succeeded after the operator adjusted hotspot
availability. Opening native USB can restart the application. The accepted
offline test revalidated the clock after opening and then held USB open through
the listening window. Missed and failed attempts remain in private evidence.

## Preservation

Each installation and restoration used fresh paired flash reads, explicit
hardware identity and compatible application recovery. Normal/Fajr bytes,
bootloader, control regions and the inactive application were preserved.
Post-test banks contained the valid signed generation-1 approved recording;
restoration erased only the bank owned by this test and restored the selected
application. Complete readback verified the original shared-audio partition and
preserved the latest NVS bytes. Production preferences and consumption matched
the fresh same-day baseline after restoration; historical NVS was not restored.

Both speakers returned to their previous production source
`9d7e87ce2a438bb7805967059ed3264a46cf273b`, v0.7.0, with at least 20 seconds
of healthy automatic scheduling. No publication or website change occurred.
Credentials, media, full snapshots and raw observations remain outside Git.

## Reliability follow-up

A subsequent production-host regression reproduced firmware checks starting
without consulting shared playback ownership. The follow-up fix makes manual
checks/installation requests return busy, leaves automatic checks due until
ownership clears, and excludes USB provisioning and first-run enrollment from
conflicting work. It also preserves USB network repair while an update is merely
queued. These new admission paths require their own source-bound validation;
the earlier listening passes do not test the later fix.

The original dua qualification profiles disable both firmware update transports.
Their results cannot establish firmware-update concurrency. Raw instrumented
heap/PSRAM/worker-stack observations are retained, but a successful build or
listening sample does not establish clean production runtime headroom. Bounded
runtime measurements, scheduler responsiveness during flash work and applicable
update/provisioning exclusion remain separate follow-up checks. Use the
[qualification guide](dua-qualification.md) and relevant hardware runbooks, with
fresh live state and preservation before any further installation.
