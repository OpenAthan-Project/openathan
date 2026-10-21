# Waveshare ESP32-S3-AUDIO-Board

The screenless AUDIO-Board has independent production and isolated development
profiles. It reuses the standalone product layer, shared ES8311 playback
sequencing and PCF85063 RTC adapter. No Box V2 display or Voice Pyramid code is
required. See the [parts and setup guide](../../hardware/reference-builds/waveshare-audio-board/README.md).

## Hardware contract

The [official V1.1 schematic](https://files.waveshare.com/wiki/ESP32-S3-AUDIO-Board/ESP32-S3-AUDIO-Board_1.1.pdf)
and the [official example archive](https://files.waveshare.com/wiki/ESP32-S3-AUDIO-Board/ESP32-S3-AUDIO-Board-Demo.zip)
are the sources for these mappings. The inspected archive SHA-256 is
`e63823469cec26ac782b9e8a9f8af22d8c55015362fa8a4fb4e7212a17af149e`;
`I2C_Driver.h`, `I2S_Driver.h`, `Audio_ES8311.cpp`, `Button_Driver.cpp` and
`TCA9555PWR.h` agree with the schematic for the bus, audio, amplifier and keys. Confirm the actual PCB before hardware writes;
these are documented mappings, not an observation of the new physical unit.

| Function | Wiring |
| --- | --- |
| I2C | SDA GPIO11, SCL GPIO10 |
| ES8311 | 0x18; 44.1 kHz, 16-bit mono; MCLK 12, BCLK 13, LRCLK 14, DOUT 16 |
| TCA9555 | 0x20; amplifier P10 (zero-based bit 8), keys P11/P12/P13 (bits 9/10/11), active low |
| RGB | Seven WS2812 LEDs, GPIO38, RGB; colour order awaits physical confirmation |
| RTC | PCF85063, 0x51 on the same I2C bus |
| BOOT | GPIO0, active low with internal pull-up |
| Memory | 16 MiB flash, 8 MiB octal PSRAM |

The vendor factory driver's `LED_STRIP_COLOR_COMPONENT_FMT_RGB` selects RGB,
although its adjacent comment says GRB. The profile follows the executable
configuration; physical red/green/blue observation is required before release.

The board adapter is the sole owner of its TCA9555; do not instantiate a second
expander driver at that address. Initialization preserves unrelated bank-1 bits
and leaves bank 0 untouched. It verifies the low amplifier output latch before
selecting output direction. Amplifier transitions require acknowledged writes
and matching readback. Failure latches playback unavailable and attempts to
switch power off; a broken bus cannot guarantee the physical output state.
ES8311 fixed 0 dB output and software volume reuse the Box playback implementation.
No samples are amplified above the original recording. Begin attended listening
at a low setting before raising volume.

Key polling is bounded to 50 Hz with 50 ms stable debounce and no hold-repeat.
Held startup keys and keys held through failed reads require a stable release
before an action. KEY1/KEY3 adjust a complete settings snapshot through the
existing settings service, preserving revision checks and storage-fault handling.
KEY2 calls Stop directly. BOOT retains the normal release-triggered gestures;
without a display, idle Stop has no screen action.

The optional RGB adapter applies the existing portable light frames to all seven
pixels. It has no independent effects, persistence or light policy. Failed output
never gates prayer calculation/audio. No new HTTP or serial protocol is introduced.

## Build and release interfaces

Use the [pinned CI prerequisites](../../.github/workflows/README.md):

```sh
python -m esphome compile firmware/esphome/waveshare-audio-board/production.yaml
python -m esphome compile firmware/esphome/waveshare-audio-board/development.yaml
```

Production uses hardware identity `waveshare-esp32-s3-audio-board`, production
storage and signed updates. Development uses `openathan-test`, isolated namespaces
and disabled public/USB firmware updates. Release validation rejects isolated images.
Neither configuration includes display, microphone, camera or SD components.

Both app slots remain 2 MiB; actual OTA payloads must remain at most 1,572,864
bytes. Shared audio remains 3.5 MiB at 0x410000. Extra flash is unused. Artifact
names use `waveshare-audio-board.`; `athan-audio.bin` and optional dua assets stay
shared. A combined package can include `--waveshare-build-dir` and
`--audio-board-build-dir`; every supplied profile must match the source/version.
Older single-board and two-board bundles remain valid.

## Hardware qualification

No installation, playback or recovery result is established by this change.
Use the [provisioning runbook](../../firmware/esphome/provisioning/HARDWARE_TEST.md)
with a new AUDIO-Board-specific preflight: identify USB port/MAC/security/flash,
verify PCB revision/mappings, retain matching private original-flash reads, and
plan recovery before first installation. A vendor partition layout requires a
reviewed first-install transition; do not assume application-only compatibility.

Use approved real recordings, never CI fixtures. Qualify normal/Fajr listening,
buttons/volume, physical colours/brightness, phone setup without a screen,
real scheduled playback and no replay, Quran/dua priority, RTC restart, preserving
signed Wi-Fi/USB updates and rollback/recovery. Measure internal heap, largest
free block/fragmentation and PSRAM during audio/network/light activity. Keep
original backups and credentials outside Git. Record observed and untested
outcomes separately before public release.

See the [dated software measurements](waveshare-audio-board-validation-2026-10-21.md).
