# Dua after Athan

Development feature: one optional offline recording after scheduled Athan. The
[dated validation report](dua-validation-2026-10-18.md) records matched builds,
automated coverage and the remaining hardware/media gates. The
preference defaults to off. The maintainer approved the supplied recording and
permission basis on 2026-10-18; its exact hash and unchanged bytes are documented
in [recording rights and attribution](../../AUDIO-LICENSES.md). The recording stays
outside Git. Text verification and the hardware qualification below remain
required before a release with dua media; recording approval does not publish
or install it.

## Owner behavior

The device page has one **Dua after Athan** preference. Enabling it saves intent
and, if needed, queues a recording download from that firmware version's release.
The page distinguishes the saved preference from a recording ready for offline
playback. Closing the page or restarting retains enabled intent; a pending
playback sequence is never restored after restart.

Only a scheduled normal/Fajr announcement can arm the follow-up. The recording
must already be installed and enabled, with nonzero volume, when Athan starts.
A successful reader EOF, successful decoding and drained speaker output start a
two-second monotonic pause, then one dua at the current Athan volume. A shared
Isha/Fajr announcement has one follow-up. Direct previews do not arm one.

Stop is available during Athan, the pause and dua. Disabling the preference or
setting volume to zero cancels the remaining dua; disabling during Athan leaves
Athan playing. A new scheduled Athan replaces the sequence. Quran Play is refused
until the sequence ends. A stopped, failed or interrupted announcement never
triggers or retries a dua. Dua errors do not change prayer consumption or block
future Athan.

## Interfaces and persistence

All endpoints use existing Digest authentication and same-origin checks:

| Endpoint | Input | Behavior |
| --- | --- | --- |
| `GET /api/dua` | None | Preference revision, enabled intent, readiness, installation state/progress, error and sequence phase |
| `POST /api/dua` | `{"schema":1,"expected_revision":1,"enabled":true}` | Durable independent preference save; conflicts return 409 |
| `POST /api/dua/retry` | `{}` | Explicit retry of a failed installation while enabled |

Uncertain writes require GET readback before another save. `/api/status` includes
`dua` and `playback_busy`; `playing` retains audible pipeline semantics. The pause
is busy, so Stop, Quran, physical controls, setup and upgrade admission use shared
ownership rather than audible playback alone. `playback_source` additionally
supports `dua`. Optional device displays show “Dua / Starting” during the pause
and “Dua / Playing” during playback; the calculated prayer ring is unchanged.

The `supported` observation describes media installation availability, independently
of preference storage. An owner can save enabled intent or turn it off while media
is unavailable, provided the preference store is healthy. Saving the preference
does not establish that a recording is ready.

The portable sequence controller wraps only scheduler starts and has no network,
ESPHome or board dependencies. Optional adapter methods provide a verified dua
file and a session-specific completion result. Adapters without this capability
continue ordinary Athan operation. The pinned local speaker pipeline advertises
`OPENATHAN_DUA_PIPELINE`, including when Quran is disabled. Generic builds using
the unmodified ESPHome player retain ordinary Athan and do not advertise verified
dua playback. The independent 16-byte checksummed `dua`
preference is stored in the prayer namespace (the isolated namespace in test
builds). Absent preferences are off without a boot write. Corrupt or failed dua
storage disables the option without changing prayer settings or startup health.

## Media and safe installation

The existing two-track `OAUDIO01` header, normal/Fajr bytes, partition table and
application slots are unchanged. Two banks occupy the final 512 KiB of the shared
3.5 MiB audio partition, at relative offsets `0x300000` and `0x340000`. Each bank
has a 4096-byte header and a maximum 258,048-byte (252 KiB) MP3 payload.

The header contains `OADUA001`, a generation, lengths, format, payload hash and a
signed descriptor, followed by a header hash and final commit marker. The device
verifies signature and payload hash before selecting the highest valid committed
bank; it can fall back to the other valid bank. Fresh bundles use bank zero,
generation one. A missing or corrupt optional bank does not invalidate Athan.

Before writes, the device validates the existing Athan catalog and proves that
it ends before the banks. Erased or recognized dua storage is accepted; unrelated
tail data is refused. An independently committed `dua_stage` journal identifies
an interrupted staging bank, allowing a retry after a partial erase. Only that
bank is erased/written. Full payload and header readback precede the final commit
marker. Cuts before that marker leave staging ineligible for playback.

`dua.json` is a P-256 signed envelope under the existing production trust key.
Its payload binds schema, `kind: dua`, media identity, release version, layout,
format, exact size and SHA-256. Downloads derive `dua.json`/`dua.mp3` URLs from the
running firmware version, using the existing restricted HTTPS release transport.
Installed valid media is retained across application updates; this version does
not automatically replace it.

Downloads start after firmware startup confirmation with valid time, healthy
scheduling and at least 15 minutes until the next announcement. They exclude
playback, firmware/USB updates and provisioning transactions. Safety is published
by the main loop and rechecked after network operations and before bounded flash
operations. Cancellation stops staging; network failures, including errors while
reading the signed descriptor, retry after five minutes.
Verification, layout and persistent HTTP failures require explicit Retry.
Internal-RAM buffers are used for flash operations. Ordinary isolated builds and
development identities cannot fetch production media. The
[dua-only qualification profiles](dua-qualification.md) use private media trust
on either board while keeping firmware updates disabled. They expose runtime
heap/PSRAM and completed media-worker stack observations only in the test build.
The explicitly selected upgrade-qualification component also uses its existing private release origin,
CA and signing key for dua assets; its isolation/release-exclusion guards remain
mandatory. A qualification feed supplies `dua.json` and `dua.mp3` under the running
qualification version's `releases/download/<version>/` route. Preserve the entire
shared-audio partition, including any existing optional banks, before an authorized
hardware session; test preference isolation does not create a second physical
audio partition.

## Packaging and validation

The third approved entry in `release/recordings.json` binds the supplied dua to
its exact hash, source, permission URL and attribution in `AUDIO-LICENSES.md`.
Review replacement media before changing that approval. Keep the actual recording
and signing key outside Git.
Pass `--dua /private/path/dua.mp3` to the ordinary release packaging command.
Packaging requires the same source-bound approval and production signing key as
other recordings, emits signed `dua.json` and `dua.mp3`, and includes the same
recording/descriptor in the fresh-install audio image and checksums. Without
`--dua`, existing two-recording bundles remain supported.

Run the normal host, Python, browser and affected firmware capacity checks, plus:

```sh
python tools/check_pipeline_completion.py
python tools/check_dua.py --arduinojson /path/to/resolved/arduinojson/src
```

These host fixtures are synthetic and must never be installed or played. They
exercise the production pipeline state machine and downloader with deterministic
I/O; they do not establish device TLS, scheduling latency under flash writes,
audible quality, power-loss behavior or runtime memory headroom.

Before release qualification, separately validate real offline playback and Stop,
interrupted installation/recovery, firmware/provisioning exclusion, the existing
scheduler continuity/lateness windows during flash work, and heap/fragmentation/
PSRAM on both boards. Use isolated test storage and private fixtures under the
existing hardware runbooks, with fresh device identity/recovery checks and an
attended listening session. No device installation or qualification is implied
by a development build or CI pass.
