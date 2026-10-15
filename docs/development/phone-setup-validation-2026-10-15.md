# Phone setup validation — 2026-10-15

This report covers the unreleased first-run phone setup change relative to merged
main `5ac1802d9251596375caece5186f5637a0c4c3fa`. It records automated checks and
compile-only firmware measurements. No hardware installation, physical phone
handoff, public website change or release qualification occurred.

## Behavior and previews

A healthy fresh speaker offers an open setup network, automatic captive-page
redirects and a manual local address. The limited page creates or verifies the
device password, accepts home Wi-Fi and shows handoff instructions before the
network closes for the connection attempt. Failed attempts restore setup Wi-Fi;
configured-device outages never reopen enrollment. Existing USB recovery and the
authenticated prayer wizard retain their contracts. See [phone setup](phone-setup.md).

The [Atom/Waveshare renderer montage](phone-setup/screens.png) uses the shipping
renderer with synthetic values. Browser captures use the real setup assets with
simulated endpoints: [phone password](phone-setup/setup-password-phone.png),
[desktop password](phone-setup/setup-password-desktop.png),
[Wi-Fi selection](phone-setup/setup-wifi-phone.png) and
[address/handoff](phone-setup/setup-connect-phone.png). These are not device observations.

## Automated validation

- 21 UBSan CTests and all 138 pinned Python tests passed without skips. Final
  generated-config checks also passed after the AP-only SDK configuration change.
- The production-method bootstrap harness passed in production and isolated
  storage modes with the resolved ArduinoJson 7.4.3 library. It executes actual
  HTTP, USB and connection lifecycle bodies with deterministic platform adapters,
  real password verification and real credential-store commits. Cases include
  startup admission, AP/station exclusion, existing-password rejection, response
  interruption, 30-second retry, failed commits and USB transaction conflicts.
- Existing production HTTP authentication, updater ownership and rollback/runtime
  regression checks passed. The shutdown check still returns without waiting on
  HTTP work and now observes DNS revocation.
- 532 existing and 26 new Chromium/WebKit browser cases passed. Bootstrap cases
  cover hidden networks, validation, escaped network names, interrupted writes,
  storage failures and explicit 409/429 recovery without automatic resubmission.
  Independent UI review found and verified the correction of that recovery issue.
- Independent ZBar decoding recovered the exact open-network payload
  `WIFI:T:nopass;S:OpenAthan-Setup-a1b2c3;;` and permanent URL
  `http://openathan-a1b2c3.local/` from both renderer layouts. Connecting screens
  contain no unavailable-network QR. The existing version 3/M encoder is retained.
- Optional display-only compositions still generate without the product Device
  component. The screenless product firmware also compiles with phone setup.

## Matched firmware measurements

Both phases used Python 3.13.5, ESPHome 2026.9.0, ESP-IDF 5.5.5 and Xtensa GCC
14.2.0 (`esp-14.2.0_20260121`). Each pair used the same configuration, pinned
libraries and the same full baseline commit string in both phases. The commit string
is a measurement control; candidate binaries are compile-only synthetic-fixture
artifacts and must never be installed or played.

All eleven pairs passed capacity, dependency-pin, partition, image-header and
factory/OTA payload checks with matching dependency-lock hashes. Both application
slots and the separate 3.5 MiB shared audio partition are retained. The limit is
1,572,864 bytes per actual OTA image; recordings remain outside the application.
Static RAM below is the capacity checker's linked-RAM measure, not runtime heap.

| Profile | Baseline OTA bytes | Final OTA bytes | Delta | Budget remaining | Static RAM delta |
| --- | ---: | ---: | ---: | ---: | ---: |
| Atom production | 1,318,608 | 1,385,952 | +67,344 | 186,912 | +524 |
| Provisioning validation | 1,320,000 | 1,387,440 | +67,440 | 185,424 | +524 |
| Upgrade qualification | 1,324,720 | 1,392,288 | +67,568 | 180,576 | +532 |
| Upgrade startup failure | 1,324,720 | 1,392,288 | +67,568 | 180,576 | +532 |
| Waveshare production | 1,322,400 | 1,389,696 | +67,296 | 183,168 | +532 |
| Waveshare display development | 1,300,464 | 1,368,096 | +67,632 | 204,768 | +524 |
| Waveshare diagnostics | 1,387,424 | 1,455,696 | +68,272 | 117,168 | +532 |
| Quran qualification | 1,389,904 | 1,458,224 | +68,320 | 114,640 | +532 |
| Generic scheduler/device | 1,124,528 | 1,124,528 | +0 | 448,336 | +0 |
| Synthetic scheduler | 1,142,112 | 1,142,112 | +0 | 430,752 | +0 |
| Waveshare without display | 1,238,544 | 1,304,560 | +66,016 | 268,304 | +428 |

The three setup assets add **7,552 compressed bytes**, using the existing native
HTML/CSS/JavaScript system without a framework, font or media dependency. Map
analysis attributes most other growth to SoftAP MAC/WPA support, the DHCP server
and the bootstrap implementation. An open AP does not need SAE authentication:
disabling only AP SAE saved **6,336 bytes** on Waveshare production
(1,396,032 → 1,389,696). STA SAE, H2E, PK and OWE remain enabled; representative
station-authentication symbols retain identical sizes. No partition or budget
increase was needed.

## Physical acceptance still required

The host adapters do not exercise live sockets, DHCP delivery, AP radio behavior,
phone operating-system captive detection or physical screen scanning. Runtime
heap, fragmentation and PSRAM remain unmeasured; static sizes do not establish
headroom during later audio/network activity.

Before release claims, coordinate the [hardware runbook](../../firmware/esphome/provisioning/HARDWARE_TEST.md)
with isolated records and fresh compatible application-only recovery. Observe
both boards, iPhone/Android popup and manual fallback, failed Wi-Fi and restart
recovery, successful durable handoff, no-AP configured outages, readable/scannable
screens and the authenticated prayer wizard. Preserve current settings, prayer
history and shared audio. The separate website handoff remains a later milestone
following supported published firmware.
