# Waveshare Box V2 public-support plan

This is a proposed follow-up to the [isolated development profile](waveshare-box-v2.md),
not an implementation or release qualification. Public updates remain disabled.
The [dated hardware evidence](waveshare-validation-2026-10-09.md) applies to one
ESP32-S3-Touch-LCD-1.85C-BOX V2 unit. V1, other panel lots, touch, RTC, microphones
and SD are outside this proposal.

## Production storage and preserving migration

Add a dedicated production Waveshare entry point, with the same standalone
services and board adapters, after separately approving its migration and
qualification. Keep development storage isolated. Removing the test-storage
define alone would select different records and would not transfer the current
settings or consumed-prayer history.

The first migration should be an attended maintainer operation, not an automatic
behavior on every boot or an ordinary browser settings save. It must:

- Reidentify the device, validate the current image/layout and retain current
  settings, history, credentials, OTA metadata and shared-audio recovery evidence.
- Decode and validate the complete source snapshot. Transfer prayer settings and
  their revision, consumption watermarks, occurrence-bound skip, display/light
  preferences and time format from `oa_test` to `openathan`; transfer enrollment
  and activation from `oa_network_test` and `oa_setup_test` to their production
  counterparts only after explicit approval to reuse those credentials.
- Refuse automatic replacement when any production records already exist. Adopt
  a complete valid production installation unchanged; incomplete, conflicting or
  corrupt records require a reviewed recovery/migration decision. Never combine
  independently selected settings and history or overwrite a newer watermark.
- Persist a migration-in-progress gate before writing destination records. It
  must take precedence over the existing legacy-settings adoption path, which
  can activate recognized settings when an activation record is absent.
- Keep destination scheduling inactive while staging and verifying all records.
  Record source identity and snapshot hashes in a versioned migration journal;
  retry only the same verified snapshot. Commit activation last, after history,
  preferences and credentials have been read back successfully.
- Preserve source records until production acceptance completes. Leave any
  development update request isolated; do not copy `oa_upgrade_test/request`
  into the production updater queue.

After a valid clock returns, arm only future unconsumed occurrences. Never replay
a consumed prayer, perform catch-up playback, restore historical NVS, or clear
history to complete migration. Host fault injection must cover every staging,
commit and activation boundary, journal corruption and repeated migration.
Attended acceptance must cover a cold restart and moving a consumed occurrence
without replay, with the shared-audio hash unchanged.

## Compatible recovery and failed-startup rollback

Retain hardware-specific application recovery artifacts for every qualified
production revision. A factory backup is historical recovery material, not a
routine settings/credentials restore. The development unit's vendor image in
the inactive slot is not a usable OpenAthan rollback target.

Before enabling any public update transport, separately qualify the 16 MiB
rollback-capable bootloader and a compatible baseline application in both slots.
Any bootloader, inactive-slot or OTA-metadata transition needs its own preserving
checkpoint and approval; application-only installation cannot retrofit an
unqualified bootloader. Preserve all current durable records and recordings.

Use a Waveshare-specific isolated qualification profile and the existing updater
engine. Prove healthy pending-startup confirmation and a deliberately failed
startup returning to the known compatible image, in both slot directions.
Check image identity, settings/history, credentials and audio after each result.
Exercise interrupted download, safe-window cancellation, playback handoff and
interrupted boot selection. Do not infer rollback from a successful reboot or
the existence of two slots.

## Optional screen failure

Use the board adapter to inject LCD initialization and backlight failures in a
qualification image. Separately perform an attended physical fault test using a
reviewed safe disconnection procedure for this board; power down before changing
connections. Retain observed results separately from injected failures.

Require functioning phone access, real-clock readiness and approved audio while
the screen is unavailable. Stop and volume must remain usable, and a screen
fault must not veto otherwise healthy application startup confirmation. Test
reconnection and recovery without resetting settings. A fault that electrically
blocks the shared I²C bus may also affect the codec and needs separate evidence;
software failure injection does not qualify that physical condition. Other panel
lots require their own register-sequence and orientation validation.

## Hardware-specific release and installer contract

Keep the existing lower-8-MiB layout, two 2 MiB slots, 3.5 MiB audio partition and
1,572,864-byte application ceiling for both targets. The extra Waveshare flash
remains unused. Introduce an explicit reviewed hardware-profile table instead of
globally allowing 16 MiB images: Atom retains 8 MiB headers; Waveshare requires
16 MiB headers, its exact hardware identity and production storage. Reject
development, diagnostic and qualification images from every public bundle.

Preserve Atom's schema-1 manifest, ten-field signed descriptor, existing asset
names, bootloader allowlist and update endpoints. Publish Waveshare artifacts
under distinct names in the same qualified release, such as
`manifest-waveshare-v2.json`, `firmware-waveshare-v2.factory.bin`,
`firmware-waveshare-v2.ota.bin` and `upgrade-waveshare-v2.json`, with a separate
build report and explicit checksum entries. Share an audio asset only when its
exact bytes, format and redistribution approval match both targets.

The Waveshare schema-1 signed descriptor can retain the existing fields: exact
hardware identity selects the reviewed 16 MiB image contract; the application
hash binds the image header. Bind source, version, layout, storage/audio formats,
rollback requirement, byte count and SHA-256 as today. Select descriptor and
application asset names from the compiled hardware profile, never caller-supplied
URLs. Verify target/header/layout again before staging or boot selection.

Plan a separately reviewed website change to present explicit Atom and Waveshare
V2 selection. Chip and flash size alone cannot identify the attached peripheral
board. Require exact installed hardware identity for preserving updates; fresh
installation requires an explicit model selection and clear erase disclosure.
Reject V1, mismatched images and ambiguous preserving-update targets. The website
continues to consume published artifacts and never builds firmware.

Acceptance must include unchanged Atom bundle/import/update tests, cross-target
rejection, incorrect header/layout/signature/hash tests, 16 MiB fresh installation
and preserving Waveshare updates. Measure OTA bytes, runtime internal heap,
largest blocks and PSRAM recovery under comparable concurrent workloads; compile
success is not runtime qualification.

## Approval and qualification sequence

1. Review production storage/migration implementation and host fault-injection
   coverage in a focused draft PR. Approve the exact attended migration candidate.
2. Approve and verify the preserving bootloader/baseline transition and actual
   failed-startup rollback. Keep public update transports disabled until it passes.
3. Complete source-bound Waveshare listening, scheduled playback, phone controls,
   screen-failure and bounded network/resource qualification. Reuse earlier
   evidence only where the source, recording and hardware behavior remain valid.
   Resolve the intermittent HTTP failures or explicitly review remaining limits
   before public qualification.
4. Review hardware-aware release tooling and website selection independently,
   including negative Atom compatibility tests and signed bundle inspection.
5. Obtain separate approvals for release publication/latest selection and website
   deployment after reviewing the exact artifacts and qualification report.

Every installation checkpoint names the unit, running slot, security state,
source/image identity, preserved regions and matching recovery procedure.
Reconcile uncertain writes through reads before considering another attempt.
Development acceptance alone does not authorize any of these transitions.
