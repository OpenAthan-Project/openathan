# Device UI redesign validation — 2026-10-05

Unreleased development implementation. The user approved the revised browser
mockups and authorized embedded integration. This report covers production UI
assets, automated host/browser checks and matched compile-only firmware images.
No hardware was accessed, flashed or played; no release or deployment occurred.

## Resulting behavior

Today leads with the authoritative device next occurrence and local time,
readiness, volume and named Skip/Restore. Hero and the single amber timetable row
remain on Asr when skipped; both move only when the reported occurrence changes.
A future-day occurrence has a dated hero and leaves today's rows neutral.
Sunrise has no Athan. Playback identity, history and countdown are not inferred.
A disconnected board freezes observed information and labels it stale; playback
may still be active. Stop is available from both views and remains independent
of preference writes. Successful Stop responses that still report playing keep
pending feedback until authoritative playback completion.

Ordinary preferences save automatically, with immediate slider output and release/
keyboard coalescing. Prayer calculations remain drafts until preview/confirm.
Preference saves merge into the confirmed snapshot, retaining coordinate precision
and excluding unconfirmed calculation changes. Failed/conflicting edits survive;
uncertain outcomes require readback before further writes in their revision domain.
Discarding preference edits does not discard a separate prayer draft. Drafts
survive in-page navigation and warn before browser navigation.

Review corrections on 2026-10-06 invalidate ready previews when conflict recovery
reads changed calculation settings, before replacing the confirmed snapshot.
Delayed Skip/Restore responses and uncertain-action readbacks preserve playback
confirmed by a newer Stop. Successful status responses without settings, when
storage is explicitly faulty, remain connected and show the device fault.
Prayer controls and confirmation are disabled; drafts and pending edits survive,
with independent preferences and reported playback Stop still available. Recovery
requires confirmed saved settings, and no default prayer settings are invented.

First run follows location → calculation → timetable review → Finish setup.
The optional HTTPS helper precedes manual coordinates. Helper proposals require
ready preview; manual setup can finish while waiting for valid time with a warning.
Settings expose screen/lights only when supported and retain independent display,
lights and time-format persistence. Existing request payloads/revisions are retained.
The only additive API field is read-only `local_date` on valid-clock status,
derived from saved timezone and omitted while waiting; clients tolerate absence.

## Automated checks

- **96 Chromium/WebKit checks passed, zero failures or skips.** They exercise the real production HTML/CSS/JavaScript and C++
  Digest verifier against simulated settings endpoints. The final suite covers
  automatic saves, rapid/nested edits, domain conflicts, dropped committed replies,
  failed readback, applying/storage/output failures, Stop during saves and delayed
  Stop completion, occurrence-bound Skip and uncertain action reconciliation,
  draft retention, location handoff, stale previews, setup clock gates, capability
  visibility, coordinate precision, device-local time, update regressions and nonce
  expiry without a second Chromium sign-in prompt.
- The 20 added review regression checks cover preference conflict Retry/Use saved
  values with a ready prayer preview, delayed Skip/Restore responses and readbacks
  after Stop, initial unreadable prayer storage, fault/recovery draft retention,
  uncertain save readback during a storage fault and a preview arriving during
  that fault. Independent time-format saves and Stop remain usable where the
  authoritative device state allows them.
- Layout checks: 320px, 390px and 1280px, ordinary and 200% root text, both time formats and system/wider native fonts;
  Today and Settings have no horizontal overflow. Linux CI exposed wide-font
  time and native dropdown overflow. The corrected hero scale and bounded native
  fields retain the approved composition and a visible outer focus ring. Buttons/ranges/selects meet 44px dimensions;
  checkbox labels provide 48px targets. Native Chromium Tab and macOS Safari
  Option-Tab traversal are checked. Text pairs meet 4.5:1 and control boundaries
  3:1; unchanged status polls do not mutate the readiness live text.
- 103 Python tests passed with no skipped tests. Production and isolated settings
  adapters compile the actual local API with pinned ArduinoJson; coverage adds
  valid local date, timezone date rollover and invalid-clock omission.
- All 16 CTest tests passed with undefined-behavior sanitization.
- All nine developer ESPHome configurations validated. Affected reference,
  isolated, qualification and rollback variants compiled and passed the existing
  capacity/dependency/partition/audio-exclusion checks before and after.
- JavaScript syntax and Git whitespace checks passed.

Browser endpoint fixtures and all figures below are **simulated data**. They do
not establish physical audio, screen/light behavior or actual assistive-technology
speech. Tests inspect DOM semantics and announcements; VoiceOver/NVDA speech is
unmeasured.

## Matched OTA measurements

Baseline commit: `16fd942951361955fbb047e1bdfd2a3668580f62`.
Both phases used the same disposable source/build directories, public compile-only
settings, generated synthetic audio fixture, Python 3.13.0, ESPHome 2026.9.0,
ESP-IDF 5.5.5 and `esp-14.2.0_20260121`, with the existing reviewed dependency pins.
Only the intended UI assets and local API source were copied into the candidate.
Actual OTA images were measured; linked-image estimates were not substituted.

Application budget: **1,572,864 bytes**. Each slot remains **2,097,152 bytes**,
with both slots and the separate 3.5 MiB shared audio partition unchanged.

| Variant | Before OTA bytes | After OTA bytes | Delta | Budget remaining |
| --- | ---: | ---: | ---: | ---: |
| reference | 1,256,144 | 1,262,448 | +6,304 | 310,416 |
| isolated | 1,257,904 | 1,264,240 | +6,336 | 308,624 |
| qualification | 1,263,280 | 1,269,616 | +6,336 | 303,248 |
| rollback | 1,263,280 | 1,269,616 | +6,336 | 303,248 |

Reference slot free space is 834,704 bytes;
the largest affected image leaves 827,536
bytes. All exceed the required 512 KiB slot headroom. Capacity checks verified
factory/OTA payload equality, reviewed dependencies, unchanged partitions,
qualification/test-material isolation and absence of audio recordings in apps.
Every final OTA contains the exact current gzip bytes of all three UI assets.

| Asset | Before gzip bytes | After gzip bytes | Delta |
| --- | ---: | ---: | ---: |
| `index.html` | 2,553 | 3,389 | +836 |
| `app.js` | 8,048 | 11,988 | +3,940 |
| `style.css` | 1,524 | 2,989 | +1,465 |

Total compressed UI assets grow from 12,125
to 18,366 bytes. No frontend framework, font,
image, remote asset or new runtime dependency is bundled. npm remains test tooling.

| Variant | Before OTA SHA-256 | After OTA SHA-256 |
| --- | --- | --- |
| reference | `8af88f13d1ff410315a0dd182470fa7bf3817ba0741977d2cb37696372b1d1e3` | `8be9eadbbb5b57d4ece9cf376a5bdc796bb22d141b35d2b1f57555eaad16c55f` |
| isolated | `988a71a075bad4b2780134cdda81676d4d8fda8a4ca0e2e18c2c330314dd7a4e` | `cd5c2bc8984c89359b969b6d3ac97ab398e4f23e41fd4a1281e62676b608f725` |
| qualification | `685500c1b2911c00daa6bf2042aebf9b8e4d509a44a90fd327755872f5e3a699` | `d355d72c8f8fa883ee2621dcbeb46f6d42d4d042563c3380e1b0e85e6852035f` |
| rollback | `39b85947a3e86e2b553d1f46bbe998a82df5f17423e552563fdfca59091d38bc` | `1863cc54a027e01adf4f40bb7bd55a3c48c2374536d7e7d16b0c1f10045ccb74` |

The 2026-10-06 review fixes were also measured against reviewed PR head
`d6d3a1054c894cd8f1bb9e2e7bb6833a0d339313`, using those same pinned
directories and configurations, with prior images/logs archived before rebuilding.

| Variant | Reviewed OTA bytes | Corrected OTA bytes | Review-fix delta |
| --- | ---: | ---: | ---: |
| reference | 1,262,128 | 1,262,448 | +320 |
| isolated | 1,263,904 | 1,264,240 | +336 |
| qualification | 1,269,280 | 1,269,616 | +336 |
| rollback | 1,269,280 | 1,269,616 | +336 |

These UI-only corrections leave static RAM unchanged and add no device request
polling or runtime dependencies. All four capacity checks pass.

Use the pinned environment and prepared public CI fixture for reproduction:

```sh
python tools/prepare_ci.py --output-dir /tmp/openathan-ui-audio
# For each phase, set a matching build path, then compile the configuration.
ESPHOME_BUILD_PATH=/tmp/openathan-ui-reference python -m esphome compile firmware/esphome/openathan.yaml > /tmp/reference.log 2>&1
python tools/check_feasibility.py --build-dir /tmp/openathan-ui-reference/openathan --log /tmp/reference.log --audio-image /tmp/openathan-ui-audio/athan-audio.bin
```

Repeat with `provisioning/validation.yaml` (`openathan-test`),
`upgrades/qualification.yaml` (`openathan-test`), and that qualification config
with `-s qualification_version v0.0.3 -s qualification_startup_failure true`.
Archive baseline logs/images before replacing source with candidate assets.
See the CI README for full prerequisites and configuration checks.

## Runtime assessment

Static RAM is unchanged: 115,219 bytes for reference/isolated and 115,291 for
qualification/rollback. The UI is gzip data served from flash; its extra browser
state lives in the client. Status adds one bounded device-local date string.
Existing 5-second status and 3-second update polling cadences are retained, with
hidden-tab polling suppressed and unchanged text/timetable updates coalesced.
Preference writes are serialized and slider keyboard changes debounced; Stop
retains an independent request path. These changes do not add an unbounded queue.

Static RAM and successful compilation do **not** establish heap, fragmentation,
PSRAM, concurrent audio/network headroom or device response latency. Those remain
unmeasured for this revision; no physical runtime acceptance is claimed.

## Visual review and representative captures

The fresh Impeccable review found the supplied production composition faithful to
the approved browser direction and no material visual defect. It requested one
truthful Stop-feedback correction. Its verdict pass scored that one fix Resolved
and returned `ship` at the listed-fix scope. Documentation records the production
translation while preserving the historical generated-comp pixel gate as open.
No exact pixel reproduction or release readiness is implied.

All figures use **simulated API data** from the real production-asset test harness.
Full-page phone captures retain fixed navigation at the original viewport bottom;
content below that viewport remains reachable by scrolling.

![Simulated Today phone](device-ui-redesign/today-phone.png)
![Simulated Today desktop](device-ui-redesign/today-desktop.png)
![Simulated skipped Asr](device-ui-redesign/skipped-phone.png)
![Simulated Settings desktop](device-ui-redesign/settings-desktop.png)
![Simulated first-run timetable review](device-ui-redesign/setup-review-phone.png)
