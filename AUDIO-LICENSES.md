# Release recording rights and attribution

**No recordings are approved for redistribution.** Recordings used in private
hardware tests are not release media. Listening tests do not establish rights.

The [recording registry](release/recordings.json) is deliberately unapproved.
Before changing an entry to `approved: true`, review compatible redistribution
rights and record its source URL, license URL, attribution and exact SHA-256.
Document the grant, required notices, any transformations and the exact hash here
for both the normal and Fajr recordings. Commit the review before building a
release candidate. Files remain outside Git and are supplied to release packaging
explicitly; packaging never downloads or transcodes them.

Approval is a human review recorded in source control. Hash validation binds that
review to supplied bytes; it cannot establish legal rights, audible quality or
correct Fajr content. Public release also requires physical qualification.
