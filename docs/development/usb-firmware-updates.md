# Preserving USB application updates

This additive development protocol streams a signed application into the existing
inactive 2 MiB OTA slot. It does not write the bootloader, partition table,
settings/credentials, prayer-consumption history or 3.5 MiB shared audio. The
public website keeps USB updating disabled until transport-specific physical
qualification and publication of a capable release. Legacy released firmware
continues to support credential recovery; it needs an initial Wi-Fi or maintainer
update before using this extension.

The [development validation report](usb-update-validation-2026-10-08.md) records
automated results, matched OTA measurements and the outstanding physical limits.

## Trust and ownership

The firmware owns signature validation, hardware/layout/settings/audio-format
compatibility, stable newer-version checks, the 1.5 MiB application budget, image
hash and embedded version validation, inactive-slot selection, startup confirmation
and rollback. It reuses the [existing upgrade contracts](firmware-upgrades.md).
BEGIN requires a confirmed running application, compatible rollback bootloader,
healthy services/storage, no playback and a safe prayer window. A missing clock
permits transfer only while automatic playback is blocked and services are healthy.
No firmware-health or rollback requirement is relaxed for Atom-only USB power.

USB and Wi-Fi update ownership are exclusive. A pending credential join/scan blocks
BEGIN; active USB requests block other USB credential/scan mutations and authenticated
HTTP mutations. Read-only status and identity remain available. No USB transfer
automatically restarts, retries chunks or becomes a Wi-Fi download.

## Wire contract

Use existing OpenAthan extension framing at 115200 baud. RPC request type 3 and
response type 4 retain their field encoding. Error type 2 retains codes 1 invalid,
2 unsupported and 255 busy/storage/uncertain selection.

| Command | Fields | Successful response |
| --- | --- | --- |
| INFO `0x10` | none | `1`, installed version, source commit, `supported`/`unsupported`, state, `confirmed`/`pending`, result, queued version or empty, received application bytes |
| BEGIN `0x11` | descriptor bytes, 1–8192 | nonzero token as 16 lowercase hexadecimal digits |
| VERIFY `0x12` | token | `accepted`, verified offered version |
| FINISH `0x13` | token | `verified`, selected version |
| ABORT `0x14` | token | `aborted` |
| ABORT `0x14` | `interrupted` | `aborted`, only for a rebooted incomplete USB request with no selection marker |

DATA type 5 payload is a 13-byte header plus 1–242 bytes: little-endian uint64
token, uint8 kind (0 descriptor, 1 application), little-endian uint32 exact byte
offset. ACK type 6 returns the same token/kind with the next offset and no data.
Out-of-order, duplicate, stale-token and oversized chunks are rejected, not replayed.
No flash erase occurs before VERIFY validates the complete signed descriptor.
Application chunks are copied into bounded internal stack RAM for flash writes;
the descriptor scratch allocation is released after verification or abort.

One browser owner permits one outstanding RPC or chunk. Every mutation is sent
once. Missing/malformed acknowledgements or disconnects block subsequent writes;
read-only reconciliation remains allowed. FINISH errors and failed post-FINISH
readback require uncertainty handling even if a device error was received, because
selection may already have happened. A transfer idle for more than 40 seconds
aborts before selection. Once selected, a timeout cannot erase or cancel it.

## Durable request and power handoff

Existing Wi-Fi requests remain schema 1 with `queue` and `expected`. USB requests
use schema 2 with `source: "usb"`, the signed `queue` and `expected`; no source is
inferred from old records. The request is committed before erase, and the expected
version before boot selection. Only confirmed capable firmware creates USB records.
If interrupted before selection, reconciliation reports `usb_interrupted` and
waits for explicit USB discard. If the exact candidate is selected, it reports
`awaiting_power` and never reboots automatically. A selection error retains the
journal and reports `usb_selection_uncertain` until authoritative reconciliation.
Unreadable boot selection, missing image identification after a completed handoff,
or contradictory metadata also retain this uncertain state; they never authorize
another transfer or journal clearing. Recovered reads return a selected matching
candidate to `awaiting_power`, including a request whose `expected` marker is empty.
A successful read selecting the confirmed running application is required before
reporting an interrupted transfer. An absent inactive-slot OTA entry is normal after
staging and cannot establish rejection by itself. `ABORT interrupted` requires
confirmed startup health and repeats the selection check under the updater lock
before clearing an interrupted request. Rollback requires an identified requested
image with a successfully read `INVALID` or `ABORTED` state. A newer running
version supersedes an old USB request only when selection proves the candidate
is unselected; version comparison alone cannot clear a pending selection.

After FINISH, the browser reports **written and verified**, closes USB and directs
the owner to unplug Atom USB and power only the Pyramid bottom port. Startup
success is a separate observation on the device page. Existing 30-second health
confirmation and 90-second failed-startup rollback apply; no network or valid clock
is required for confirmation. The running requested version or an actually rejected
candidate clears bookkeeping through the existing success/rollback reconciliation.
Settings/history/audio formats and the previous application remain compatible.

## Required qualification

Host and browser tests establish software contracts, not physical preservation,
power behavior or memory headroom. Use the provisioning hardware runbook for fresh
preflight, current-state readback and private backups. Retain source/artifact hashes
and test actual Chrome and Edge USB ownership; Atom-only transfer and bottom-only
startup; success and failed-startup rollback; interruptions before descriptor
verification, during writes and around selection; wrong signatures/images/tokens/
offsets; competing Wi-Fi/settings activity; and before/after settings, credentials,
consumption history and audio evidence. Measure runtime heap/fragmentation and
prayer/audio responsiveness during transfer separately from binary size. Do not
install synthetic CI recordings. Keep public activation and firmware publication
separately reviewed after these results.
