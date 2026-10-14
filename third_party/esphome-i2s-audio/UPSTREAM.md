# Pinned ESPHome I2S audio component

Source: [ESPHome 2026.9.0](https://github.com/esphome/esphome/tree/2026.9.0/esphome/components/i2s_audio),
commit `c6e4c87e525dd343e470d8dea368957a002f6505`.
The complete component is retained, including microphone and SPDIF files; the
calibrated Atom package alone selects this local component. Other configurations
continue to resolve the pinned installed ESPHome component.

`upstream.json` records the commit and license hash;
`upstream-sha256.json` records each original component file. `volume-fence.patch`
is the maintained patch against those files, including the added header.
The Python provenance regression reverses that patch and checks all hashes.
When updating the pinned version, replace the originals, regenerate the patch,
and rerun host transport tests, schema controls and matched firmware builds.
Do not edit the installed SDK cache.

The patch adds opt-in `volume_fence: true` for standard software-gain speakers.
It rejects SPDIF and a direct `audio_dac` binding. Restored or queued media-player
volume/mute setters cannot change owned gain in this mode. The adapter calls
`request_volume_with_fence(level)` and polls `volume_fence_status(token)`.
A token contains a session and generation; it is pending, complete, cancelled
or failed. Stop/restart cancels obsolete tokens, counter exhaustion fails closed,
and only a fully cleaned-up stopped speaker can acknowledge without DMA.

Each DMA descriptor uses one synchronized gain/generation snapshot, with gain
applied to consumed raw PCM before bit-depth narrowing. The existing FIFO record
carries its session/generation, including preloaded and padded silence. A complete
matching descriptor confirms that older FIFO buffers drained. Fenced playback
composes one complete descriptor in the existing silence scratch buffer and
submits it in one write, preventing partial-buffer abandonment under driver backlog. Completion batches
are validated before publishing an acknowledgement. Allocation, callback, DMA
layout, write, overflow and queue faults latch failure through task cleanup;
logical readback and elapsed time cannot complete a fence.
An ISR counter tracks completed descriptors awaiting a committed buffer write,
independently of drained application events. Exceeding the driver's N-1 queue
capacity fails before event publication; the driver overflow callback also fails
closed. This covers ESP-IDF 5.5.5 invoking on_sent before overflow handling.
ISR queue failures latch under the same ESP-IDF critical-section lock immediately,
without waiting for deferred event-group updates. Driver teardown rechecks faults
before clearing events or allowing a stopped speaker to complete a fence.

The opted-in standard speaker disables its I2S channel in the worker before
publishing `TASK_STOPPING`/`TASK_STOPPED`. DMA must not keep producing completions
after the worker stops servicing descriptors while main-loop cleanup is delayed.
The main loop still owns task deletion, channel deletion and bus release, and
skips a second disable after verified worker quiescence. Existing faults and a
failed disable remain latched; this does not relax active-stream overflow checks.

No audio task, PCM buffer or queue is added. For an opted-in speaker the existing
ten-entry record queue grows from 4 to 12 bytes per entry (+80 heap bytes).
The synchronized state and lock add fixed object storage. DMA descriptors, source
staging, ring buffer duration and the 4096-byte task stack retain upstream sizes;
changed task locals still require attended runtime headroom measurement.

## License

The unchanged upstream [LICENSE](LICENSE) includes ESPHome copyright, GPLv3
runtime/C++ terms and MIT Python terms. Modified upstream files identify the
OpenAthan changes dated 2026-10-11. The new C++ fence header uses GPL-3.0-only;
Python changes retain MIT. These files are exceptions to the repository's
Apache-2.0 software license. Preserve these notices with source and firmware
redistribution; existing third-party dependency terms remain applicable.
