# Saved screen brightness — development validation, 2026-10-04

This unreleased addition provides a separate **Screen** card on the device-local
settings page, with a 1–100% slider, a 10% default, and explicit save/reload
controls. Saved brightness applies to the optional backlight and survives restart.
Devices without an adjustable backlight hide the card. The screen stays on;
automatic dimming and screen-off are outside this change.

A separate checksummed NVS record preserves the existing prayer settings,
consumption history, time format and lights. Missing records use a virtual
10% default without writing flash; corrupt records are retained. Revision checks
and readback of uncertain saves protect concurrent edits and lost acknowledgements.
The [local API](../../firmware/esphome/provisioning/README.md#screen-brightness-unreleased)
distinguishes a saved preference from failed backlight application. Runtime changes
write only the Atom's white-channel PWM register; its current and other outputs
retain their existing configuration. Optional display faults never gate audio,
scheduling or update health.

The default was 10% for this implementation. The
[2026-10-05 follow-up](screen-brightness-default-validation-2026-10-05.md) changes
it to 50% while retaining existing saved preferences.

## Automated and browser validation

- All 16 UBSan CTests passed. Coverage includes production/isolated preference
  storage, interrupted write and commit outcomes, corruption, revision exhaustion,
  existing-record preservation and isolated cleanup. Backlight tests verify
  bounded PWM-only adjustments, unchanged-value suppression and runtime retry.
- All 103 pinned-environment Python tests passed, including production/isolated
  C++ local-API tests against resolved ArduinoJson 7.4.3. Adapter checks cover
  startup restoration, application failure/recovery, saves during simulated
  playback, maintenance write denial and unchanged scheduler readiness/history.
- All nine CI configuration checks passed.
- All 54 Chromium/WebKit browser cases passed, including authenticated brightness
  reads/writes, keyboard bounds, persistence after page reload, unsaved edits,
  revision conflicts, dropped save responses, storage/application failures and
  hiding unsupported controls. Consecutive brightness and time-format actions
  retain the latest brightness and revision after a save, reload or lost-response
  reconciliation.
- Desktop and 390-pixel-wide screen-card captures were reviewed. The added control
  reuses the existing page styling and fits without horizontal overflow. The UI
  detector reported existing header typography/decorative-label findings;
  this focused addition retains the existing design.
- Final whitespace, relative documentation links and source diff review passed.

## Matched firmware measurements

Baseline is `a7fbef5`. Before/after builds reuse the same disposable source
directory, configuration, public compile-only identity, Python 3.13.0, ESPHome
2026.9.0, ESP-IDF 5.5.5 and `esp-14.2.0_20260121` compiler. CI audio fixtures are
synthetic, compile-only bytes. These images are not installation/release artifacts.

| Profile | Before OTA bytes | After OTA bytes | Delta | Remaining 1.5 MiB budget |
| --- | ---: | ---: | ---: | ---: |
| Reference | 1,253,008 | 1,256,192 | +3,184 | 316,672 |
| Isolated provisioning | 1,254,752 | 1,257,952 | +3,200 | 314,912 |
| Scheduler device | 1,120,032 | 1,121,104 | +1,072 | 451,760 |
| Scheduler validation | 1,137,552 | 1,138,640 | +1,088 | 434,224 |
| Upgrade qualification | 1,260,208 | 1,263,328 | +3,120 | 309,536 |
| Forced startup failure | 1,260,208 | 1,263,328 | +3,120 | 309,536 |

Reference growth is **3,184 bytes**, with **316,672 bytes** of application budget
remaining. Reference static RAM grows by 56 bytes to 115,219. The
[measurement data](screen-brightness-build-2026-10-04.json) records all six profiles
and image hashes. Existing capacity checks passed for every profile before and
after, including dependency pins, partition layout, factory/OTA consistency,
profile separation and absence of audio payloads from application images.
Both 2 MiB application slots and the separate 3.5 MiB shared-audio partition remain
unchanged. No dependency, recording or release-version change was introduced.

## Review follow-up — 2026-10-05

A failed I2C acknowledgement can leave the physical PWM value uncertain. The
adapter now invalidates its cached brightness before a changed-value write, so
restoring the previous preference after a failure sends PWM again. A register
regression covers that reversal and subsequent unchanged-value suppression.
All 16 UBSan CTests passed again. The four profiles containing the adapter were
rebuilt with the same pinned dependencies and configuration; the table above
reflects the reviewed implementation. The two scheduler-only profiles are
unaffected by this adapter change.

A brightness response previously left the shared page snapshot stale, allowing
time-format save/reload to restore an older brightness and revision before the
next status poll. Brightness rendering now synchronizes that snapshot after
save, reload and uncertain-save readback. Six new Chromium/WebKit regressions
reproduced the reset against `1dd8ad5`; all 54 browser cases pass after the fix.
The regressions also verify that the next brightness save uses the current
revision without a false conflict. All four profiles embedding the UI were rebuilt
with the same pinned configuration; their capacity checks passed. The matched
before/after images and deltas for this fix are recorded in the measurement data.

## Physical validation remaining

No device was accessed, flashed, reset or played during this implementation.
Physical low/intermediate/high brightness, cold-restart persistence and changing
brightness during approved audio playback remain untested for this source.
Runtime heap, fragmentation and PSRAM under those conditions are not established
by the static RAM figures or simulated playback test. Builds and browser checks
establish development behavior, not release readiness.

Use the existing [hardware runbook](../../firmware/esphome/provisioning/HARDWARE_TEST.md)
for a separately coordinated session. Recheck live identity/state and preserve
settings, history, shared audio and recovery; use isolated storage for diagnostics
and compatible application-only installation for any approved candidate.
