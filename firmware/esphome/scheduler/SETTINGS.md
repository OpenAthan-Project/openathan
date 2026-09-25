# Saved developer settings

Runtime settings can change without rebuilding firmware or requiring Home
Assistant. `device.yaml` and `validation.yaml` expose encrypted `get_settings`
and `set_settings` actions. `development.yaml` has the same settings service
without a network interface. The phone UI remains separate work.

## Read, edit, save

Use the pinned ESPHome environment. Exports contain location information and
must stay outside the repository.

```sh
python tools/device_console.py --host openathan-feasibility.local \
  --secrets /private/path/secrets.yaml --seconds 5 \
  --get-settings /private/path/settings.json
# Edit settings in the export, retaining schema and revision.
python tools/device_console.py --host openathan-feasibility.local \
  --secrets /private/path/secrets.yaml --seconds 5 \
  --set-settings /private/path/settings.json
```

The response contains `schema`, `revision`, `settings`, `application`,
`scheduler_fault`, `automatic_ready`, and read-only `consumed_through` watermarks
(calculation days since 1970-01-01; −2147483648 means never). Success acknowledges a durable save.
Application status separately reports `applied`, `volume_pending`,
`volume_failed`, or `storage_fault`. Read back until applied before evaluating
volume. Export again before another edit: stale revisions are rejected.

| Setting | Values |
| --- | --- |
| `latitude`, `longitude` | Finite numbers in −90…90 and −180…180 |
| `timezone` | IANA name; console regenerates `timezone_rules` |
| `method` | Existing twelve calculation presets |
| `asr_method` | `standard` or `hanafi` |
| `high_latitude` | `auto`, `middle_of_night`, `seventh_of_night`, `twilight_angle` |
| `offsets` | Six named events, integer −120…120 minutes |
| `enabled` | Five named prayers, boolean; sunrise never plays |
| `volume` | Integer 0…100%; zero is silent |

The console resolves IANA names using pinned ESPHome/tzdata tooling. It also
accepts POSIX strings exported by builds without an IANA label. Stored recurring
DST rules work offline, with ESPHome's normal timezone limitations; they are not
a historical timezone database. Legislation changes require saving updated rules.

The reference player's 60% output ceiling remains: 70% control means 42% output.
It is internal in scheduler images to prevent native player commands bypassing
the settings service. Front-button controls remain. Current audio continues
across saves; volume affects it immediately, while schedule changes rearm future
events without catch-up. Volume-only and unchanged saves do not rearm scheduling.

## Persistence and interfaces

Framework-independent `DeviceSettings` and `SettingsService` use a separate
192-byte `OAC1` NVS record at `openathan/settings`, with explicit byte order,
revision and CRC-32. The existing `openathan/scheduler` record remains unchanged.
Settings saves never clear consumption. Both test records use `oa_validation`.

With no settings record, first upgrade commits configured prayer settings and
timezone plus restored player volume. Saved values subsequently override build
defaults, including across OTA. Initial `timezone_name` must resolve to the
configured clock rules. Runtime timezone changes affect only OpenAthan dates.

`set_settings` takes a JSON string with exactly `schema: 1`, `expected_revision`,
and the complete `settings` object, limited to 4096 bytes. The device validates
fields and preflights schedule changes with valid time. Without time, valid
settings can be saved and schedule validation waits. Rejections preserve active
settings. Identical current-revision saves perform no write. An uncertain
disconnect triggers readback, never an automatic repeat write.

Acknowledgment follows `nvs_commit`. Interrupted, unacknowledged writes may
restore either complete snapshot; acknowledged saves must restore the new one.
Write failures preserve active settings and latch saves and automatic playback
off until restart. Corrupt, unsupported or unreadable records block playback
without falling back to defaults or erasing NVS. Repair is explicit; there is no
factory-reset API.

Consumed identity remains calculation date plus prayer, including both members
of shared Isha/Fajr occurrences. Changes and restarts never clear watermarks.
Consumption precedes playback: a crash can omit an announcement but cannot
replay it. Deliberate erasure/restoration of historical NVS is outside that
guarantee. Older firmware ignores saved settings and uses its compiled values;
downgrade retains history but may change the timetable.

Custom playback adapters used by this bridge must implement actual readback in
`volume_percent()` and requests in `request_volume_percent()`. Pending/failed
readback gates new playback; dropped requests are retried without stopping audio.

## Phone setup integration

The developer actions use ESPHome's encrypted native API; they are not an HTTP
interface for a browser. The planned phone setup must add its own transport and
access controls while using this settings service. Provisioning, Wi-Fi recovery
and password recovery must preserve prayer settings and consumed-prayer history.
First-run confirmation is separate product work: current developer firmware uses
configured/restored settings and gates scheduling on valid time, storage and audio.

## Verification

Host tests cover serialization/corruption, upgrades, interrupted writes,
acknowledgments, revision conflicts, rejected schedules, consumed prayers moved
later, dropped volume requests and pinned ESPHome timezone conversion. Synthetic
events honor offsets and retain their calculation-day identity across changes.

See the dated [reference-device results](../../../docs/development/feasibility-report.md#saved-runtime-settings--2026-09-25)
and [settings acceptance procedure](VALIDATION.md#saved-settings-acceptance).
Observed hardware outcomes and remaining checks are recorded separately from
automated coverage.
