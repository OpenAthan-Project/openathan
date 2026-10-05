# Screen brightness default — development validation, 2026-10-05

The unreleased screen-brightness preference now defaults to **50%**. The reference
backlight initialization, reusable preference service, device-page slider and
storage-fault guidance agree on that value. Missing records use virtual revision
1 without writing flash. Corrupt records remain untouched and prevent saves;
the applied fallback is 50%. Existing saved values, including the earlier 10%
preference, keep their brightness and revision. The checksummed record format,
1–100% limits, white-channel current and prayer/audio behavior are unchanged.

The device-page status badge uses 14px text at desktop and phone widths. The
redundant header tagline was removed and heading tracking relaxed. These small
readability corrections resolve the design detector's tiny-text, wide-tracking
and redundant-label findings without adding assets or dependencies. No design
findings were suppressed or left unresolved.

## Automated validation

- All 16 UBSan host suites and 103 pinned Python tests passed. Production and
  isolated tests cover the 50% missing/corrupt-record fallback, unchanged-save
  suppression, API responses, PWM duty and preservation of a saved 10% record.
- All 54 Chromium/WebKit cases passed. Updated cases cover the default and
  storage-fault guidance while existing saved-value and cross-control regressions
  remain in place. Six affected browser cases passed again after the header edits.
- All nine existing firmware configurations passed.
- Desktop and 390px phone renders showed 50%, readable status text and no
  horizontal overflow. The final design hook found no deterministic issues.

## Matched firmware measurements

Baseline is `c0cb0d6`. Both sides use the same source directory, public compile-only
identity, synthetic audio fixture and pinned toolchain listed in the
[measurement data](screen-brightness-default-build-2026-10-05.json). These images
are for compilation/capacity verification and must never be installed or played.

| Profile | Before OTA bytes | After OTA bytes | Delta | Remaining 1.5 MiB budget |
| --- | ---: | ---: | ---: | ---: |
| Reference | 1,256,192 | 1,256,128 | -64 | 316,736 |
| Isolated provisioning | 1,257,952 | 1,257,888 | -64 | 314,976 |
| Scheduler device | 1,121,104 | 1,121,104 | +0 | 451,760 |
| Scheduler validation | 1,138,640 | 1,138,640 | +0 | 434,224 |
| Upgrade qualification | 1,263,328 | 1,263,280 | -48 | 309,584 |
| Forced startup failure | 1,263,328 | 1,263,280 | -48 | 309,584 |

The reference application is **64 bytes smaller**, leaving **316,736 bytes** of
application budget. The decrease includes compressed page-text and typography
changes. Static RAM remains 115,219 bytes. All six before/after capacity checks
passed, including dependency pins, factory/OTA consistency and the existing
partition contract. Both 2 MiB application slots and 3.5 MiB shared audio remain.

## Hardware evidence and limits

A separate attended session on merged `c0cb0d6` observed distinct steady 1%, 50%
and 100% levels, and retained a saved 50% setting through a confirmed ten-second
cold restart. The actual device-page brightness/time-format sequence also retained
the current brightness/revision. Those observations concern the preceding image
with an explicitly saved preference; they do not establish cold startup using
the new virtual default.

This default-change image has not been installed. The short isolated audio and
runtime-memory check remains pending, as does restoration of the normal
development application after that check. Static RAM and build passes do not
establish heap, fragmentation or PSRAM headroom. Release publication remains
separate from development validation.
