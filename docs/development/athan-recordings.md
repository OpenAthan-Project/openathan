# Curated Athan recordings

The device-local Settings page provides separate regular and Fajr choices. The
speaker stores the selected pair in its existing shared audio partition.
Installed recordings remain available without internet after a restart.
Automatic prayer playback retains the device's existing valid-clock requirement.

The default catalog address is
`https://github.com/OpenAthan-Project/athan-audio/releases/latest/download/catalog.json`.
Provisioning the catalog repository and publishing its first approved signed
release are separate deployment steps. Firmware implementation and an unsigned
review bundle do not make recordings public.

## Selection and status

Settings distinguishes installed recordings, the requested pair and unsaved
browser choices. Installation is explicit. Polls retain edits, and an uncertain
response requires healthy readback before another mutation. Unavailable storage
cannot establish whether a request was saved.

The speaker verifies the catalog's P-256 signature, then binds downloads to each
recording's signed size, SHA-256, role and version. Asset addresses derive from
the fixed repository and signed filenames. The API accepts catalog IDs rather
than arbitrary media URLs. Restoring an earlier choice downloads it again.

All endpoints use existing Digest authentication and same-origin checks.

| Endpoint | Input | Behavior |
| --- | --- | --- |
| `GET /api/athan` | None | Installed and requested pair, revision, progress, errors and catalog |
| `POST /api/athan/catalog` | `{}` | Queue catalog refresh |
| `POST /api/athan/selection` | `schema`, `expected_revision`, `normal`, `fajr` | Save two catalog IDs and queue installation |
| `POST /api/athan/retry` | `{}` | Retry an eligible failed request or repair |
| `POST /api/athan/cancel` | `{}` | Cancel before destructive replacement |

Actions return 202 when accepted, 409 for revision or ownership conflicts, 400
for invalid requests and 503 for unavailable storage. Acceptance does not mean
installation is complete. Status supplies `can_cancel` and `can_retry`.
Older firmware without the endpoint retains its existing Settings interface.

## Storage and recovery

Both OTA slots, the partition table and `OAUDIO01` format are unchanged. The pair,
including its header and alignment, must end by relative offset `0x300000`.
The two final 256 KiB dua banks are never written by this service. Catalog
validation checks every regular and Fajr combination against that boundary.

Both files are staged and verified in PSRAM before replacement. Download and
allocation failures before destructive work preserve installed bytes. An
independent checksummed record in `oa_athan` stores intent and recovery identity.
Its commit and readback precede erasure. Settings and consumption remain separate.

Initial installation requires confirmed firmware, drained playback and at least
15 minutes before the next Athan. Queued choices allow normal prayer playback.
Destructive ownership excludes new playback, firmware writes and boot selection,
dua installation and provisioning changes. Once replacement starts, network or
browser loss cannot cancel a fully staged write.

The service verifies written payloads, publishes the header last and retains its
journal through a controlled reboot. Fresh startup loads the new track offsets
and initializes preserved dua media. A healthy dua baseline must be restored
before completion; a preexisting optional dua fault does not prevent Athan
installation. A reset or write failure during replacement can leave both
recordings unavailable until online repair.

Repair is bound to the exact intended pair and the same previously validated
firmware. It does not require audio readiness or weaken pending-firmware startup
checks. Missing or corrupt recovery evidence never authorizes a guessed erase.
Repair fetches the pinned catalog version and exact assets again. Missing assets
or verification failures remain actionable errors. Elapsed prayers are not
replayed. Older firmware can read a completed pair but cannot repair an
interrupted replacement.

## Validation boundary

`tools/check_athan.py` runs the production service, playback adapter and dua
initialization against deterministic flash, NVS and transport substitutes. Host
recordings are synthetic and must never be installed or played. Runtime signature
verification uses the existing mbedTLS test substitutes. Catalog-tool tests use
actual P-256 signing and verification.

Browser checks use embedded assets and the production Digest verifier with
simulated APIs. Firmware builds establish size and dependency compatibility.
Physical power cuts, audible quality, worker stack, runtime fragmentation and
PSRAM headroom require separate attended qualification. Ordinary isolated and
updates-disabled builds cannot replace production audio.

See [catalog authoring](athan-library.md) for media approval and packaging, and
the [CI instructions](../../.github/workflows/README.md) for host checks. The
[implementation validation](athan-validation-2026-11-16.md) records measurements
and remaining qualification work.
