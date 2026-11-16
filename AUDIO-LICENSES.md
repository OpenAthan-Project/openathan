# Release recording rights and attribution

The project maintainer approved Fajr on **2026-09-28** and the current replacement
normal recording on **2026-10-01**, following review of AlAdhan / Islamic Network's
written permission for offline
bundling and redistribution in a free application without ads or in-app purchases.
On **2026-10-18**, the maintainer approved the supplied dua recording and confirmed
the same source and permission basis. The
[recording registry](release/recordings.json) binds these decisions to the exact
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

All three files were supplied by the maintainer as compressed MP3s, with
[AlAdhan's download page](https://aladhan.com/download-adhans) identified as their
source. Exact upstream variants and the original compression settings were not
recorded. Packaging preserves the supplied bytes without further transcoding,
normalization or trimming. Files remain outside Git.

| Role | Supplied file | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| Normal | `Adhan-Mishary_compressed.mp3` | 1,115,496 | `7db84d037f1643c17834bd5052da504f10c0781da334426123f71cf7ba4ea2cb` |
| Fajr | `Adhan-Fajr-Mishary_compressed.mp3` | 1,768,332 | `2005993def65a985d6c68fa8a327e2d504f328261cba9f893b2912038fe980e6` |
| Dua | `Adhan-Dua_compressed.mp3` | 97,700 | `e012c265ed388b72c28216528c583035643401737de6fbfc69aad486268b5c49` |

The replacement normal recording was selected by the maintainer, who confirmed
the same AlAdhan source and existing permission basis. It is approximately
233.55 seconds long; the supplied bytes replace the 257.15-second normal selection
approved on 2026-09-29. Fajr is unchanged. Previously published release assets
retain their original recording selection.

The dua recording was approved as supplied, without further compression. It is
approximately 21.45 seconds long, 44.1 kHz stereo MP3 and fits the existing 252 KiB
optional-media limit. The maintainer confirmed the same AlAdhan / Islamic Network
source and permission basis as the Athan recordings. Its reciter and exact
upstream variant were not independently identified; do not infer them from the
Athan credits. A subsequent [automated text check](docs/development/dua-qualification-validation-2026-10-19.md)
matched the reference wording. On-device listening and qualification remain
separate from that check and this approval of the supplied recording and permission
basis.

Although the provider does not require attribution, retain this credit with the
recordings:

> Adhan and Fajr Adhan recited by Mishary Rashid Alafasy. Audio source:
> AlAdhan.com / Islamic Network. Supplied as compressed MP3s; copyright remains
> with the respective rights holders.

> Dua after Athan. Audio source: AlAdhan.com / Islamic Network. Supplied as a
> compressed MP3; copyright remains with the respective rights holders.

## Changes and release validation

Review replacement recordings before changing their approval or hashes. Record
the source, permission, attribution and transformations here and commit the review
before building a release candidate. Packaging reads approval from that committed
revision. Hash and format checks bind the review to supplied bytes; audible
quality, correct Fajr content and final hardware qualification require separate
evidence. Approval alone does not establish a qualified public release.

## Prospective catalog recordings

The separate [Athan catalog registry](release/athan-recordings.json) retains the
current normal and Fajr recordings above with their existing approvals. Their
catalog entries refer to those exact hashes. An older normal selection is omitted
because its exact source and approval cannot be recovered from this registry.

Three prospective recordings are from Ahmed Khalaf's Athan project at commit
[`ad2bf1799add51cca4aaf1f123b1c212378c7786`](https://github.com/nandxorandor/Athan/tree/ad2bf1799add51cca4aaf1f123b1c212378c7786).
The author's [README](https://github.com/nandxorandor/Athan/blob/ad2bf1799add51cca4aaf1f123b1c212378c7786/README.md#licence)
identifies the three `Developer_Athan-*` recordings as his own and states CC BY
4.0. The other recordings in that project have different terms and are not included.
These three are **unapproved candidates**. Permission evidence does not establish
maintainer acceptance of the recordings, listening quality, Fajr wording, or
physical device qualification.

| Source file | Source SHA-256 | Candidate SHA-256 |
| --- | --- | --- |
| [`Developer_Athan-1.mp3`](https://github.com/nandxorandor/Athan/blob/ad2bf1799add51cca4aaf1f123b1c212378c7786/app/src/main/assets/athan/developer/Developer_Athan-1.mp3) | `48ce7a63922bc666be0d4919056a31445ace775b3b7ffc4ab07325d8edf9ddb6` | `c34af7604dbb7407f8e550e58cfa46774115d1c3a0d50a001b84d55d6471b687` |
| [`Developer_Athan-2.mp3`](https://github.com/nandxorandor/Athan/blob/ad2bf1799add51cca4aaf1f123b1c212378c7786/app/src/main/assets/athan/developer/Developer_Athan-2.mp3) | `0fb1127d6fea0d8552999c10798fd19624c0815128d49e420f650ef5bdd79acb` | `c98235206faaa5b229bacf95cc041f6bf0faa017313de1cc9ae9a96b2cd2390e` |
| [`Developer_Athan-3_Fajr.mp3`](https://github.com/nandxorandor/Athan/blob/ad2bf1799add51cca4aaf1f123b1c212378c7786/app/src/main/assets/athan/fajr/Developer_Athan-3_Fajr.mp3) | `18b43fca35558ed05de0cbf8fcf468fd606e17cacb688bfed065e45787f56378` | `2fa61ff15b3a261860e6bf403e442e4c23f8a376bce57a9a91fdd2a451118813` |

The candidates were re-encoded outside the repository using ffmpeg 8.0. For each
source, the command was `ffmpeg -i INPUT -map_metadata -1 -ac 1 -ar 44100
-codec:a libmp3lame -b:a 64k OUTPUT`. The candidates are 1,157,580, 1,057,688,
and 796,255 bytes respectively. Re-encoding is a modification and must be
identified alongside the creator, source, and [CC BY 4.0 license](https://creativecommons.org/licenses/by/4.0/)
when distributed. Original source assets and converted files remain outside Git.
