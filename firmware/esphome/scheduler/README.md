# Standalone scheduler development build

The scheduler calculates prayers locally, plays the offline Fajr/normal tracks,
and needs neither Home Assistant nor internet once the system clock is valid.
This is developer firmware, not a release image. It has been compiled,
host-tested and exercised on the AtomS3R/Voice Pyramid reference unit. The device
checks covered full playback of both supplied recordings, cold-start time gating,
scheduled track selection, stop, persistent skip/cancel, playback replacement,
offline scheduling, restart without replay, and OTA preservation of audio.
An actual prayer-time observation remains separate from the synthetic tests.

## Configuration and build

Use the [pinned ESPHome environment](../feasibility/README.md). The scheduler
adds no firmware dependencies. Create an ignored `secrets.yaml` alongside
`development.yaml` containing these keys:

```yaml
# Illustrative compile-only values; replace with the intended device settings.
wifi_ssid: OpenAthan-compile-only
wifi_password: compile-only-no-network
ota_password: compile-only-do-not-deploy
athan_latitude: 43.6532
athan_longitude: -79.3832
athan_timezone: America/Toronto
athan_method: north_america
```

The provided development file requires explicit latitude, longitude, timezone,
and method. Supported presets are `muslim_world_league`, `egyptian`, `karachi`,
`umm_al_qura`, `dubai`, `moonsighting_committee`, `north_america`, `kuwait`, `qatar`,
`singapore`, `tehran`, and `turkey`. `hanafi` defaults to false (Standard Asr).
High-latitude choices are `middle_of_night` (default), `seventh_of_night`, and
`twilight_angle`. There are no custom angles.

`offsets` accepts integer minutes from -120 to +120 for fajr, sunrise, dhuhr,
asr, maghrib and isha. Defaults are zero. `enabled` accepts booleans for the
five prayers, all true by default; sunrise is informational. The calculation
wrapper rejects collisions/reversed order, including disabled events and
across the three-day window. Such an invalid schedule disables automatic
playback and reports a fault. Polar events that cannot be calculated stay absent.

ESPHome's timezone is global. Every configured time source must specify the
same timezone; the schema rejects missing or conflicting values. IANA names
are resolved into ESPHome's timezone rules during configuration. Clock sources
update the system UTC clock; the bridge derives the civil date from the same
timestamp using those timezone rules.

```sh
export ESPHOME_BUILD_PATH=/tmp/openathan-scheduler
esphome compile firmware/esphome/scheduler/development.yaml > /tmp/openathan-scheduler.log 2>&1
python tools/check_feasibility.py \
  --build-dir /tmp/openathan-scheduler/openathan-feasibility \
  --log /tmp/openathan-scheduler.log \
  --audio-image /tmp/openathan-audio/athan-audio.bin
```

Build the separate audio image using the [audio-image instructions](../feasibility/README.md).
The existing manual audio feasibility configuration remains available separately.
These commands do not upload. Devices running the earlier single-slot diagnostic
firmware require a USB partition migration before installation. The tested
reference unit completed that migration; ordinary application OTA cannot change
the old layout.

## Scheduling and persistence

The core consumes an injected clock (UTC, milliseconds, monotonic time and local
date), calculator, durable store, and playback capability. It maintains yesterday,
today and tomorrow; event identity is the calculation date plus prayer, even
when an offset moves playback across midnight. Core `configure()` re-arms with
future events; the ESPHome developer settings are currently supplied at boot.

Normal polls run once per second, with at most 2,000ms of lateness allowed.
The core compares precise UTC elapsed time with monotonic elapsed time, allowing
250ms of sampling/slew tolerance. Larger corrections, backward time, polling
gaps longer than three seconds, first valid time, restart and configuration
changes re-arm without catch-up. Time-sync callbacks also evaluate the clock;
an unchanged sync does not force a re-arm. Normal timezone/DST transitions use
UTC instants and do not duplicate repeated local-clock times.

Before requesting playback, the event is consumed in durable storage. Missing
or failed audio does not retry the event. A crash after the save can miss an
Athan, but does not replay it. Older dates are covered by per-prayer monotonic
date watermarks; changing settings or moving time backward never clears them.
A mistakenly advanced clock can therefore suppress older dates after correction.
There is no automatic state reset, so recovery from that situation must be
deliberate rather than silently restoring potentially consumed events.

The ESP32 store owns NVS namespace `openathan`, key `scheduler`. Its versioned
36-byte record encodes five watermarks and an optional skip, with CRC-32, explicit
byte order, and no compiler-struct padding. It checks `nvs_set_blob` and
`nvs_commit` before acknowledging success. Missing initial state starts fresh
and skips past events; wrong-size/version/checksum or read errors block automatic
playback. A write failure latches the storage fault for the current boot. This
adapter never erases NVS, and does not use ESPHome's deferred preference queue.

The component exposes `status()` with clock readiness, automatic readiness,
playback activity, next event, skip key and fault. State changes are logged;
timestamps are UTC Unix seconds and skip dates are days since 1970-01-01.
Static build memory figures do not establish runtime headroom.

## Controls and reuse

Actions `openathan.stop`, `openathan.skip_next`, and `openathan.cancel_skip`
take `id: athan_scheduler`. The corresponding C++ skip/cancel methods return
whether the requested state is durable; failed actions log a warning.

- Stop interrupts current audio without undoing consumption.
- Skip next selects the next enabled, available prayer and saves that exact key.
  Repeated requests are idempotent. Restart retains it; passing the event clears
  it. Removing/disabling its event clears it when its date is evaluated. It never
  transfers to another prayer, including after clock corrections.
- Cancel skip durably clears the selected skip.
- Scheduled playback replaces existing audio: the adapter queues stops for both
  player pipelines, then queues the correct announcement file in FIFO order.
  Queue acceptance is not confirmation of audible playback or completion.

The reference front button uses a short click to stop, a 2–5 second click to skip
next, and a 6–10 second click to cancel skip. There is no automatic/manual test
play action on that button in this scheduler build.

The generic `packages/openathan.yaml` loads the component without board, Wi-Fi,
native API, or Voice Pyramid assumptions. Integrators supply `time_id`, settings,
and `playback_id` referencing an implementation of `openathan::Playback`.
Alternative adapters implement readiness, activity, start-with-replacement and
stop; register that base type in their ESPHome code generation.

## Verification

Run from the repository root with the pinned ESPHome Python environment:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python tools/run_tests.py
```

C++ tests exercise real prayer reference values, injected clock transitions,
DST, midnight/month/year rollovers, positive/negative offsets across midnight,
duplicate prevention, persisted skips, storage failure and playback priority.
The production NVS adapter is tested against injected NVS errors. Python tests
exercise the production audio adapter/parser and real ESPHome configuration
validation. The test runner rejects skipped tests, including when ESPHome is
missing. See the [CI guide](../../../.github/workflows/README.md) for clean-checkout
builds with public placeholder settings and synthetic audio-image fixtures.

The [capacity report](../../../docs/development/feasibility-report.md) records
build results and limits, including completed reference-device tests for SNTP
behavior, sound quality, runtime memory, power-loss persistence and OTA slot
switching. Actual prayer-time observation and deliberate rollback fault injection
remain outstanding; successful slot switching does not prove automatic rollback.
Provisioning UI, scheduler settings UI and automatic audio updates remain outside
this milestone.

For device checks, see [the validation procedure](VALIDATION.md). Its separate
synthetic timetable uses isolated NVS and the real clock; `device.yaml` returns
to the real prayer schedule while retaining optional encrypted diagnostics.
