# Release recording rights and attribution

The project maintainer approved Fajr on **2026-09-28** and the current replacement
normal recording on **2026-10-01**,
following review of AlAdhan / Islamic Network's written permission for offline
bundling and redistribution in a free application without ads or in-app purchases.
The [recording registry](release/recordings.json) binds that decision to the exact
MP3 bytes. Public release also requires physical qualification.

## Permission basis

In [Islamic Network discussion 255](https://community.islamic.network/d/255-permission-to-bundle-an-adhan-audio-file-in-our-android-app-nurafiq),
the developer asks whether an Adhan file may be bundled for offline playback,
whether attribution is required, and whether redistribution has restrictions for
a free app without ads or in-app purchases. The reply permits bundling, requires
no attribution and states no restrictions.

[Discussion 266](https://community.islamic.network/d/266-permission-to-bundle-the-download-adhans-set-in-an-ad-supported-free-app)
also addresses Mishary recordings and confirms no restriction from Islamic
Network's side. The recordings retain their respective copyrights. This project
approval relies on the provider's published permission; it does not assert
ownership of the recordings or place them under the software or documentation
license. Keep the audio freely accessible when distributing OpenAthan under this
approval. Review materially different distribution terms separately.

## Approved recordings

Both files were supplied by the maintainer as compressed MP3s, with
[AlAdhan's download page](https://aladhan.com/download-adhans) identified as their
source. Exact upstream variants and the original compression settings were not
recorded. Packaging preserves the supplied bytes without further transcoding,
normalization or trimming. Files remain outside Git.

| Role | Supplied file | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| Normal | `Adhan-Mishary_compressed.mp3` | 1,115,496 | `7db84d037f1643c17834bd5052da504f10c0781da334426123f71cf7ba4ea2cb` |
| Fajr | `Adhan-Fajr-Mishary_compressed.mp3` | 1,768,332 | `2005993def65a985d6c68fa8a327e2d504f328261cba9f893b2912038fe980e6` |

The replacement normal recording was selected by the maintainer, who confirmed
the same AlAdhan source and existing permission basis. It is approximately
233.55 seconds long; the supplied bytes replace the 257.15-second normal selection
approved on 2026-09-29. Fajr is unchanged. Previously published release assets
retain their original recording selection.

Although the provider does not require attribution, retain this credit with the
recordings:

> Adhan and Fajr Adhan recited by Mishary Rashid Alafasy. Audio source:
> AlAdhan.com / Islamic Network. Supplied as compressed MP3s; copyright remains
> with the respective rights holders.

## Changes and release validation

Review replacement recordings before changing their approval or hashes. Record
the source, permission, attribution and transformations here and commit the review
before building a release candidate. Packaging reads approval from that committed
revision. Hash and format checks bind the review to supplied bytes; audible
quality, correct Fajr content and final hardware qualification require separate
evidence. Approval alone does not establish a qualified public release.
