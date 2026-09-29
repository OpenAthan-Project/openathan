# Prayer lights

The reference firmware optionally drives both Voice Pyramid A167 LED groups.
Prayer calculations and audio remain independent of the light hardware. No Home
Assistant, public website or cloud connection is needed for the countdown once
the device clock is valid.

## Behavior

Both groups use the same color and saved brightness. The countdown includes all
five calculated prayers, even when their announcements are disabled or skipped.
Sunrise is excluded. Saved location, calculation rules, offsets and timezone
apply. The next strictly future calculated time is selected, including tomorrow
and adjusted times spanning midnight; coincident times share one transition.
The visual timetable does not consume or change any announcement history.

| State | Output |
| --- | --- |
| More than 30 minutes until prayer | Steady green |
| More than 10 and at most 30 minutes | Steady orange |
| At most 10 minutes | Steady red |
| Audio playing | Blue pulse |
| Setup incomplete or time unavailable | White pulse |
| Blocking settings, schedule, storage or audio fault | Red pulse |
| Disabled, zero brightness, maintenance, or no future calculated prayer | Off |

Precedence is disabled/maintenance, blocking fault, setup/time wait, playback,
then countdown. At prayer time, the countdown advances to the next future time;
actual playback overrides it. Wi-Fi loss alone does not change the indication.
Pulses smoothly rise and fall over three seconds, between roughly 10% and 100%
of saved brightness (quantized to integer percentages, with a 1% minimum while
enabled). At 1% brightness, this quantization produces a steady status light.
Defaults are enabled and 20% brightness. There are no quiet hours in this version.

## Settings and local API

The device UI's **Lights** section saves independently from prayer settings.
Controls appear only on builds configured with a light output. A light failure
is reported in text and never disables scheduling or audio.

Authenticated `GET /api/lights` returns:

```json
{"schema":1,"supported":true,"revision":1,"settings":{"enabled":true,"brightness_percent":20},"application":"applied","mode":"green"}
```

`POST /api/lights` uses the existing same-origin JSON and Digest protection:

```json
{"schema":1,"expected_revision":1,"settings":{"enabled":true,"brightness_percent":35}}
```

Successful saves return the same shape as GET. Brightness must be an integer
from 0 through 100; enabled must be boolean. Unknown fields are rejected.
Invalid payloads return 400, stale revisions 409, unavailable storage/maintenance
503, and writes without a configured capability 404. Reads without a capability
return `supported: false`. `/api/status` also includes this snapshot as `lights`.

Application states are `applied`, `output_unavailable`, `storage_fault`,
`save_failed`, and `unsupported`. `applied` means the requested I2C writes were
acknowledged, not that emitted light was physically observed. Mode is `off`,
`green`, `orange`, `red`, `playing`, `waiting`, or `fault`.
A lost save response requires readback before retrying; the UI never automatically
repeats a mutation. Revisions are independent of prayer-settings revisions.

The versioned, CRC-protected 16-byte `lights` record shares the existing
production `openathan` namespace (isolated test: `oa_test`), but has its own key.
Existing `settings` and `scheduler` records retain their exact formats. Absence
uses defaults with virtual revision 1 without writing flash. Corruption preserves
the record, turns lights off and blocks light writes for that boot. Failed saves
retain the last acknowledged values in RAM and block further light writes until
restart. An interrupted/unacknowledged save may restore either complete snapshot.
Older firmware ignores this additional record. Test-storage cleanup removes it
through the existing namespace allowlist.

## Adapters and cost

Custom ESPHome configurations may supply `openathan.light_output_id`, implementing
the portable `LightOutput` interface; omit it to retain operation without LEDs.
Only the official reference configuration opts into the A167 lights by default.
Policy and timetable selection live in the portable core; the ESPHome bridge
owns persistence, clock/state input and bounded retries. Board register behavior
stays in `voice_pyramid`.

The adapter uses STM32 address 0x1A on the existing board I2C bus, 14 pixels per
group, BGR0 pixel records at 0x20/0x60 and brightness registers 0x10/0x11.
Brightness is 0–100, as implemented by the [manufacturer source](https://github.com/m5stack/M5Echo-Pyramid/blob/main/src/STM32Ctrl.cpp),
although its header's parameter comment says 0–255. Use per-pixel writes with
checked acknowledgements, not the vendor bulk helper. No speaker reset,
STM32 configuration changes or flash-writeback commands are issued.

Initialization writes dark output first. Color changes darken both groups while
updating their pixels; pulses update only two brightness registers. Unchanged
frames do not write the bus. Updates are capped at 20 Hz; failures retry at most
once per second and force a full frame repair. The frame path uses fixed-size
state without animation heap allocation. A broken bus can leave previously
emitted light visible; software cannot guarantee darkness without communication.

## Focused hardware acceptance

Coordinate an attended session using the [provisioning runbook](../../firmware/esphome/provisioning/HARDWARE_TEST.md)
and its current-device preflight and compatible application-only recovery.
Keep the established audio, production settings, credentials and prayer history.
Use isolated test storage for accelerated schedule scenarios; never install or
play the synthetic CI audio image. Do not repeat unrelated completed acceptance.

1. Observe both complete LED groups, green/orange/red boundaries and blue/white/red
   pulses. Verify the intended colors on the physical diffuser.
2. Save disabled, zero brightness and a comfortable intermediate brightness;
   verify restart persistence and no change in prayer-settings revision/history.
3. Check skipped/muted prayers, overnight rollover and loss of Wi-Fi with a valid
   clock. Compare visual targets with the calculated timetable.
4. During one approved recording, use the phone UI and vary brightness. Observe
   uninterrupted audio, bounded main-loop latency, minimum free heap, largest
   free block/fragmentation, and PSRAM. Static build RAM is not runtime evidence.
5. Verify the isolated application can be replaced by the exact compatible
   production application with preserved production records and shared audio.

Automated tests establish policy, persistence, register traffic and UI behavior.
The historical LED color-cycle pass does not establish these integrated results.
Record actual firmware sizes and physical outcomes in the dated validation
report; do not infer release readiness from compilation.
