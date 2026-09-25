# Device validation

`validation.yaml` uses the real ESPHome clock, scheduler, NVS store and shared
audio adapter with a synthetic timetable. `device.yaml` uses the real location
and prayer settings with the same optional encrypted developer diagnostics.
`development.yaml` remains the minimal scheduler build without the diagnostic API.
None of these configurations requires Home Assistant.

Set the usual private secrets plus `diagnostic_api_key` (a random, nonzero,
base64-encoded 32-byte key). Keep secret files, logs, recordings, built images,
and recovery backups outside Git. Use Atom USB-C for USB flashing/recovery and
the Pyramid bottom USB-C for audio operation. With a compatible installed
partition layout, application-only OTA works over the network while bottom
power remains connected.

## Test controls

Use `tools/device_console.py --host HOST --secrets /private/path/secrets.yaml`
to collect encrypted local logs, or add `--press 'BUTTON NAME'` for a specific
action. Do not retry an uncertain action automatically. The console lists the
buttons present in the connected image. `--seconds` bounds each connection run.

- **Play normal / Play Fajr**: play the corresponding supplied recording.
- **Stop audio / Skip next / Cancel skip**: exercise the production actions.
- **Start test sequence**: explicitly replace the previous test run. Fajr is due
  in 180 seconds, followed by Dhuhr, Asr, Maghrib and Isha at 120-second intervals.
  The whole sequence must fit before local midnight. It does not advance time.
- **Offline for 90 seconds**: disable only the device's Wi-Fi, then restore it.
- **Diagnostic restart / Diagnostic status**: reboot or log memory/OTA status.
- **Verify audio image**: while idle, use ESP-IDF to hash the complete data
  partition on device. Compare this SHA-256 with the original image after OTA.

The timetable and consumption/skip record persist in `oa_validation`; the
production namespace remains `openathan`. Reboot and OTA retain the timetable
and never start another run. Explicit reset invalidates the old timetable before
clearing consumption, preventing replay if power fails between those writes.
Storage errors block the test scheduler. Synthetic sunrise exists only to
preserve event ordering and never plays audio.

The encrypted API's `Scheduler status` text sensor publishes the exact next
event and saved skip even after a reconnect. Use it to verify persistence, rather
than inferring a saved skip merely from silence after a restart.

The test image starts with Wi-Fi disabled for 30 seconds on every boot. Once a
second it records whether the clock and audio stayed inactive; the result is
logged after connectivity returns. A physical power cycle is necessary to count
this as a cold-boot test. The real-schedule image has no artificial Wi-Fi delay.

## Acceptance procedure

Steps 2–3 describe first installation or a deliberate partition migration.
The tested reference unit already has the compatible dual-slot/shared-audio
layout. For ordinary application updates, retain its recovery archive and use
application-only OTA without rewriting factory firmware, audio or NVS. A full
flash restore also restores historical consumption; it is not normal settings
or Wi-Fi recovery. The dated [validation report](../../../docs/development/feasibility-report.md)
records what was actually observed for each firmware milestone.

1. Recheck stable compatible dependencies; run host tests and capacity checks.
   Record exact firmware hashes and dependency locks. ESPHome 2026.9.0 pins the
   diagnostic encryption libraries noise-c 0.1.30 and libsodium 1.10021.11.
2. Read all 8 MiB of current flash twice while held in the bootloader. Require
   byte equality, archive privately with SHA-256, and record the exact restore
   command before migrating partitions. The old single-slot image needs USB
   installation; ordinary application OTA cannot migrate it.
3. Write factory firmware at 0 and the audio image at `0x410000`; require upload
   verification and an audio readback hash match. Inspect boot logs and confirm
   that both recordings validate. Keep the verified backup available.
4. Listen to both entire recordings. Record duration, player transitions, errors,
   reset reason, idle/playback/recovered memory, and the listener's quality and
   Fajr-content assessment. Queued playback alone is not a sound-quality pass.
5. Start a test run; skip Fajr, power-cycle, and verify that the same Fajr skip
   survives. Cancel it before Fajr, let Fajr start, then stop after ten seconds.
   Skip Dhuhr twice and verify one event is skipped. Let Asr play and stop it
   with the front button. Start Fajr manually 30 seconds before Maghrib and verify
   that Maghrib replaces it with the normal track. Stop, restart without replay,
   then disable Wi-Fi across Isha. Reset only test state for additional runs.
   Record physical power cycling separately from software restart.
6. Confirm cold boot without valid time cannot play, then synchronization arms
   only future events. Do not advance the system clock to simulate this test.
7. Upload two application-only OTA images, recording slot changes and successful
   boots. Finish with the real schedule (`device.yaml`), which omits the test
   component and buttons. Hash the complete audio region again to establish that OTA
   preserved it. Dual-slot switching does not itself prove automatic rollback.
8. Verify the real settings and next prayer, then observe a real scheduled event
   when due. Mark that observation pending until it actually happens.

Keep measured results in a private dated report, with failures and untested
checks explicit. A full 8 MiB restore returns to the pre-test layout and state.

## Saved settings acceptance

Use the [settings console](SETTINGS.md) and keep exports, logs and firmware hashes
outside Git. Build/test passes do not count as the following physical checks.

1. Inspect live firmware, volume, next prayer and consumption. Retain the verified
   recovery archive. Use application-only OTA with the unchanged partition table;
   do not restore historical NVS during an ordinary upgrade. Record the audio
   digest before and after.
2. In the validation image, export settings. Change every field using valid
   coordinates/timezone, non-colliding offsets and a comfortable volume. Read
   back the complete snapshot and wait for `application=applied`. Disconnect
   bottom power for ten seconds, reconnect, and compare every field and revision.
   The boot log records settings revision/CRC with the clock still waiting;
   the existing 30-second offline boot gate must also pass.
3. Interrupt an update by removing power. Require either the complete old or new
   snapshot after reconnect, never mixed fields. Record acknowledgment and
   whether the cut actually overlapped the write. Host injection covers exact
   write/commit boundaries; a cut after completion is not a mid-write test.
4. Restore zero offsets, all prayers enabled and intended timezone. Start one
   synthetic sequence. After Fajr starts, cut power during playback. Reconnect
   without starting another sequence, confirm consumed state, then move the
   whole timetable three minutes later with +3-minute offsets. This preserves
   event ordering and leaves time for cold-boot synchronization. Confirm valid
   time returns before moved Fajr is due and that there is no replay then.
   Repeat with a new sequence if reconnection or synchronization was too slow.
5. Verify a future unconsumed prayer plays. During playback change volume and
   another valid setting; audio continues at the new level. Invalid settings
   and stale revisions must leave active settings unchanged. Check disabled/
   re-enabled and skipped prayers as well.
6. Finish on the real image through application-only OTA. Export its independent
   production settings; verify intended values, next prayer and unchanged audio
   digest. Record physical cuts, audible observations and unavailable checks
   separately. Software restarts alone are not a physical power-loss pass.
Do not publish private configuration or claim touch/microphone validation here.
