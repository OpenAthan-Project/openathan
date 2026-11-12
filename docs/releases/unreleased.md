# Unreleased

## Automatic updates

Speakers can install signed stable firmware releases automatically. Automatic
updates are on by default for new installations and existing speakers once they
receive this feature through one preserving manual or USB update.

Use **Settings → Updates → Automatic updates** to turn it off. The saved choice
survives restarts and preserving updates; manual checks and installation remain
available. Turning it off cancels an automatic download or queued update before
restart begins, while retaining explicit manual and USB requests.

Installation waits until playback ends and the next Athan is at least 15 minutes
away. Settings, credentials, prayer history and recordings are retained. Canceled
or rejected releases stay skipped automatically across restarts; a newer release
or an explicit manual installation can proceed.

The [validation record](../development/automatic-updates-validation-2026-11-12.md)
contains automated checks and firmware measurements. Physical update/recovery and
runtime-memory checks remain pending before release.
