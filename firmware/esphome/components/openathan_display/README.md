# Optional status display

The reference candidate includes a dim, always-on 128×128 status screen through
[the GC9107 board profile](../../boards/atom-s3r-gc9107-display.yaml).
This is development work after v0.2.1, not a capability in the published v0.2.1
bundle. Hardware acceptance and a successor release remain separate.

The board profile keeps the screen on at a build-time default of 10% backlight
brightness (`display_brightness_percent`) and rotation 180. Physical dimness and
orientation still require acceptance on the integrated device. This screen has
its own small-device layout; the device-local web UI retains its existing design.

The presenter is plain C++ without ESPHome dependencies. Its firmware adapter
reads the existing scheduler, activation, settings and Wi-Fi state; it does not
calculate prayer times, write settings/history or issue playback/update commands.
Saved timezone rules supply both displayed times, including recurring DST.
The display is optional: its absence, allocation failure or a failed backlight
must not gate the scheduler, local controls or application-update health.

The adapter polls once a second and sends a framebuffer only when visible
content changes. The clock displays minutes, not seconds. The driver uses an
8-bit framebuffer (16 KiB); allocator placement, SPI transfer cost, runtime heap
and PSRAM headroom must be observed on the integrated image during hardware
acceptance. Static RAM and a successful build are not those measurements.

## Layout and states

Copy is English and both local times use the 24-hour `HH:MM` format.
On a black background, the local clock starts at y=6, heading at y=32,
main value at y=58, explanation at y=96 and connection status at y=116.
The 8×8 bitmap font is scaled 1× for small text, 2× for headings/messages
and 3× for the prayer time. Text is centered and bounded to the screen.
Primary text is white, secondary text `#D0D8D8`, and fault guidance `#FFB8A8`.
There are no animation, scrolling, remote fonts or graphical UI framework.

| Condition | Center content | Explanation |
| --- | --- | --- |
| Ready with a next event | Prayer name and local HH:MM | Next Athan |
| Skip matches next/shared event | Prayer name and local HH:MM | Will be skipped |
| Temporarily not ready | Prayer name and local HH:MM | Not ready yet |
| Playback active | Athan / Playing | Button to stop |
| Setup incomplete | Setup / Needed | Use phone setup |
| Clock invalid, online | Time / Waiting | Syncing clock |
| Clock invalid, offline | Time / Waiting | Connect Wi-Fi |
| All prayers disabled | Athan / Off | All prayers off |
| No next event/time available | Athan / Waiting | No time set |
| Storage/audio/schedule fault | Error / affected capability | Use device page |

Faults take precedence over setup, then playback, clock readiness, disabled
prayers and next-event status. Playback remains visible if time becomes invalid;
the clock then displays `--:--`. Wi-Fi loss alone only changes the footer to
Offline: valid-clock standalone scheduling continues. An unrelated/stale skip
does not label a different event. Front-button stop/skip/cancel behavior is
unchanged, and the display introduces no additional controls.

## Font attribution

[font.h](font.h) contains the printable ASCII subset of Daniel Hepper's
[public-domain font8x8 basic Latin font](https://github.com/dhepper/font8x8/blob/8e279d2d864e79128e96188a6b9526cfa3fbfef9/font8x8_basic.h),
based on Marcel Sondaar's public-domain IBM VGA font work. The original author
and public-domain notice are retained in the file. No system font is bundled.

## Validation

`display_status_and_backlight` checks rendered bounds, state transitions,
shared/stale skips, refresh deduplication, PWM limits and each failed I2C write.
The production and isolated settings-adapter tests exercise the real presenter
adapter with pinned ESPHome timezone conversion: both DST boundaries, midnight,
unchanged polls, network loss, and scheduled playback with a failed display.

To inspect all fixture screens using the actual shipping renderer:

```sh
./build/display_tests /tmp/openathan-display-previews
```

The resulting PPM images are 128×128. They contain illustrative prayer/time
values, not device observations. See the [dated build report](../../../../docs/development/display-validation-2026-10-03.md)
and the [hardware runbook](../../provisioning/HARDWARE_TEST.md).
