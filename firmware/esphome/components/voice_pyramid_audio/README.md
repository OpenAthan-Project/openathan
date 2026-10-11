# Voice Pyramid product audio

This adapter implements OpenAthan's portable `Playback` interface using the
existing shared-partition audio player and A167 hardware adapter. It owns the
product volume curve, muted gain transitions and compatible settings migration.
Prayer calculations and scheduling remain in the reusable core.

Select `packages/hardware/voice-pyramid-volume.yaml` alongside the reference
hardware and product/scheduler packages. It removes the direct speaker-to-DAC
volume route, starts the amplifier with PA disabled, owns software gain through
the vendored I2S DMA fence, and binds the calibrated playback and settings store
to the scheduler. The optional `validation_id` selects
`oa_validation` and supplies that same store to the synthetic timetable harness.
Codec and PA readbacks plus a completed current-generation DMA buffer gate output.
Two seconds without completion faults playback; it never permits unmute.
Media-player volume/mute preferences cannot override calibrated gain. Zero is a
valid applied volume with both gates closed. Athan and supported Quran streams
share this output control; stream capability, ownership, state and playback epoch
are delegated to the existing partition player. Stop revokes that ownership.
Integrations using different player/scheduler IDs can retain the generic hardware
package without selecting calibration.

See the [calibration contract and dated build evidence](../../../../docs/development/atom-volume-calibration-2026-10-11.md)
for the gain curve, settings compatibility, amplifier gating and outstanding
physical acceptance.
