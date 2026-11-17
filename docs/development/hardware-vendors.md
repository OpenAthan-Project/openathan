# Hardware vendors and referral programs

Reviewed on **2026-11-11**. Product links, program terms and stock can change.
This reference covers both manufacturers used by OpenAthan, verified regional
examples and additional sellers considered during the review. It is not an
exhaustive list of retailers worldwide. The manufacturers maintain broader
[M5Stack](https://m5stack.com/distributor) and
[Waveshare](https://www.waveshare.com/distributors) distributor directories.

The two direct M5Stack product links use OpenAthan's referral code `dagcclbm` and
are marked as affiliate links. Waveshare and regional retailer links remain
ordinary URLs. A referral code does not verify account approval, approved
promotion channels, product commission rates or sale attribution.

OpenAthan is a participant in the GoAffPro Affiliate Program for M5Stack. We may
earn advertising fees from qualifying purchases through the links marked
"affiliate". These commissions support OpenAthan.

## Hardware and purchase links

| Manufacturer | Parts | Purchase listing | OpenAthan boundary |
| --- | --- | --- | --- |
| M5Stack | AtomS3R C126 | [Official store (affiliate)](https://shop.m5stack.com/products/atoms3r-dev-kit?ref=dagcclbm) | Confirm the GC9107 display. C126 alone does not establish the panel revision. Camera kits, VoiceS3R and ST7735 revisions are not substitutes for the qualified build. |
| M5Stack | Voice Pyramid A167 | [Official store (affiliate)](https://shop.m5stack.com/products/echo-pyramid-smart-speaker-base-without-m5atom?ref=dagcclbm) | Also called Echo Pyramid. The Atom controller is a separate purchase. |
| Waveshare | ESP32-S3-Touch-LCD-1.85C-BOX V2 | [Official store](https://www.waveshare.com/product/esp32-s3-touch-lcd-1.85c.htm) | Select the speaker-box option and confirm V2. V1 and the bare board are unsupported for this build. |
| Waveshare | ESP32-S3-AUDIO-Board | [Official store](https://www.waveshare.com/product/arduino/boards-kits/esp32-s3/esp32-s3-audio-board.htm) | Included in published v0.9.0. Confirm the physical revision and included speaker. USB-powered use does not require a battery. Preserve the release's physical-testing limits. |

The [published v0.9.0 release](https://github.com/OpenAthan-Project/openathan/releases/tag/v0.9.0)
and [live website release metadata](https://openathan.com/release.json) were
checked on the review date. All three builds are selected by the public installer.
The website importer verified each published manifest and artifact. This current
evidence supersedes the source documents' earlier proposed-release wording.
It does not establish new physical validation.

The [M5Stack BOM](../../hardware/reference-builds/voice-pyramid/bom.md#procurement-notes)
contains direct Atom and Pyramid links for DigiKey, OpenELAB, The Pi Hut and
TinyTronics. All four list the required model numbers. Seller listings do not
qualify every shipped revision.

Other reviewed listings have additional limits:

| Seller | Product evidence | Limit |
| --- | --- | --- |
| Mouser | [C126](https://www.mouser.co.uk/en/ProductDetail/M5Stack/C126?qs=Vi0JKjFBtYmwgBUI%252Bl7YMQ%3D%3D) and [A167](https://www.mouser.com.tr/tr/ProductDetail/M5Stack/A167?qs=KLj0QfLrw0HazNsZodT4zA%3D%3D) | C126 is marked end-of-life. Remaining inventory does not guarantee the GC9107 revision. The A167 listing was search-indexed, but direct retrieval did not complete. |
| Botland | [C126](https://botland.store/atom-series/25964-atoms3r-dev-kit-developer-module-with-085-display-m5stack-c126-6972934175557.html) and [A167](https://botland.store/atom-series/28546-voice-pyramid-smart-audio-base-for-atom-series-modules-m5stack-a167-6972934176332.html) | The English store lists a registered-B2B-customer requirement. Confirm customer eligibility and display revision. |
| Newegg marketplace | [Touch-LCD speaker box](https://www.newegg.com/p/299-005G-00001) and [AUDIO-Board](https://www.newegg.com/waveshare-esp32-s3-ai-audio-board/p/3D0-0053-00127) | Confirm the marketplace seller, Box V2 revision, included parts and affiliate eligibility before adding links to the user-facing purchase sections. AUDIO-Board retains its release limits. |

Generic cables, USB supplies, batteries and possible future expansions do not
require a particular seller or branded SKU. Their presence in a vendor's
catalog does not make them supported OpenAthan accessories.

## Referral-program findings

| Vendor | Published program | Terms observed | Before referral publication |
| --- | --- | --- | --- |
| M5Stack | [GoAffPro portal](https://m5stack.goaffpro.com/) and [application](https://m5stack.goaffpro.com/create-account) | The portal advertises **up to 5%**, not a guaranteed rate for each SKU. Its [operating agreement](https://m5stack.goaffpro.com/program-legal/terms) requires application acceptance and an identified participant disclosure. Referral code `dagcclbm` is configured on both direct product links. | Confirm account acceptance, website/repository coverage, product rates, payout terms and click attribution before publication. |
| Waveshare | [Official affiliate program](https://www.waveshare.com/join_affiliate.html) | Published base commission **2%**, tracking advertised as at least 72 hours, PayPal payout and a US$5 threshold. The rules include restrictions on directing traffic to third-party Waveshare sellers. | Obtain approval and product URLs. Confirm whether regional-seller links can coexist with participation before enabling referrals. |
| OpenELAB | [Official affiliate program](https://openelab.com/pages/affiliate-program) | Published **5%** on qualified referred sales. Applications and transactions require review through its linked UpPromote portal. | Apply, confirm the exact product and regional-store eligibility, and obtain approved product URLs. |
| DigiKey | No public sales-referral program verified in this review | Product listings establish a purchasing source, not a commission arrangement. | Ask the vendor before treating its URLs as affiliate links. |
| The Pi Hut | No public sales-referral program verified in this review | No current official commission or enrollment page found. | Ask the vendor. |
| TinyTronics | No public sales-referral program verified in this review | No current official commission or enrollment page found. | Ask the vendor. |
| Mouser | No public sales-referral program verified in this review | No current official commission or enrollment page found. | Ask the vendor. |
| Botland | No public sales-referral program verified in this review | Reseller, education and third-party sponsorship content do not establish a sales-referral program. | Ask the vendor. |
| Newegg | [Official affiliate program](https://www.newegg.com/promotions/affiliate_program/affiliate.html) | Applications go through its linked Rakuten program. The public page offers product deep links. | Confirm regional account eligibility, the specific marketplace seller, product rate and attribution before publishing tracked URLs. |

M5Stack's public store loader at
[`/a/goaffpro/loader.js`](https://shop.m5stack.com/a/goaffpro/loader.js)
identifies `m5stack.goaffpro.com` as its partner portal. The portal's landing page
and agreement were read in a browser. The public commission-details page did not
display a product-specific schedule during this review. The agreement includes
generic regional payment language, so its payout details need vendor confirmation.

Waveshare's store and enrollment pages returned HTTP 403 to automated direct
retrieval. Its indexed official product/program pages and current manufacturer
documentation supplied the available evidence. This review did not complete an
enrollment, test checkout or attributed-sale test.

[RobotShop staff reported its former affiliate program unavailable in 2019](https://community.robotshop.com/forum/t/robotshop-affiliate-program-status/37719).
That historical reply does not establish its present policy. Old launch posts
are not evidence of an active program, and the reviewed Touch-LCD listing did
not establish the required complete Box V2 variant.

## Referral activation requirements

Tracked links require an approved account, vendor-issued URLs and a record of
which public pages and products the approval covers. Credentials, payout details
and private approval correspondence stay outside Git.

Each referral URL needs a visible affiliate label and an adjacent disclosure
that commissions support OpenAthan. Program-specific participant wording also
applies, including M5Stack's named GoAffPro disclosure. Website links must use
`rel="sponsored"`. The shared website purchase component already supports the
label and relationship through its `affiliate` flag, including regional links.

Before activation, verify the redirect destination and selected variant, preserve
the model warnings, and update the firmware documentation and website together.
The generic component disclosure alone does not establish compliance with every
vendor's enrollment conditions. Referral configuration in source still requires
account and attribution verification before publication.
