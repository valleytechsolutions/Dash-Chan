# Sources and attribution

Reviewed on 2026-10-16. Reference clones are kept outside the published tree.

| Source | Pinned snapshot | Use |
|---|---|---|
| [Dragon Detector](https://github.com/valleytechsolutions/Dragon-Detector) | `3204e7a0caa3b107e751e929ec63ec1f80739828` | MIT passive parser, vendor facts, Axon and Remote ID helpers copied under `include/vendor/dragon`; original [MIT notice](licenses/DRAGON-DETECTOR-MIT.txt) retained |
| [SquachWatch-CYD](https://github.com/skizzophrenic/SquachWatch-CYD) | `9e06e0461fca45eb3a80ca18436556b4f088efbb` (first review); `33a8c3091eac1a233a15ca976ab9e072b053d210` (grading audit) | GPL-3.0 detection/category/confidence reference. Its per-signature grades, false-positive removals, broadcast disconnect rules and same-maker evil-twin exclusion are reused as facts and behaviour, with credit ([details](#detection-grading-audit)); no artwork, sprites or sounds copied |
| [Colonel Panic Unified Blue](https://github.com/colonelpanichacks/oui-spy-unified-blue) | `6e5e2829540925ff1ab5fa6d0d365378818ed56e` | Reviewed Detector's six filter classes, FlockYou source/receiver/probe and BLE methods, AXEOFF, and SkySpy. No explicit license in this snapshot; no source copied. Implementations use protocol facts, independent Dash-Chan code, and the licensed Dragon helpers. [Coverage audit](DETECTIONS.md) |
| [Espressif esptool-js](https://github.com/espressif/esptool-js) | npm `0.7.0`, exact tarball integrity in [dependencies.json](../site/vendor/dependencies.json) | Unmodified Apache-2.0 browser bundle, self-hosted; explicit custom reset works around the reviewed HardReset omission. Runtime dependencies pako 2.1.0 (MIT AND Zlib), atob-lite 2.0.0 (MIT); original [notices](../site/vendor/NOTICE.txt) retained |
| [ESP32 Dual Band Wardriver](https://github.com/justcallmekoko/ESP32DualBandWardriver) | `c142254647ed7799d7b31e7f7114f30265011619` | WiGLE Wi-Fi/BLE row conventions and SD/GPS workflow reference; [MIT notice](licenses/KOKO-MIT.txt) retained |
| [Piglet / Midwest Gadgets](https://github.com/Hamspiced/piglet) | `fb71eded9c7bdec6b646f14dff3711ff4bd1bf5a` | Reviewed GPS quality gates, buffering, SD and session behavior. Source LICENSE at this snapshot is CC BY-NC-SA 4.0; no Piglet code bundled |
| [M5Stack StackChan-BSP](https://github.com/m5stack/StackChan-BSP) | `8d4d6fc3b7a6be379c6317c45a02a30bff8c492e` | MIT board support, actual serial-servo control and factory calibration handling |
| [OpenDroneID core](https://github.com/opendroneid/opendroneid-core-c) | `849c03adbe0cdd68997a462fafab397edcfe53f3` | Apache-2.0 decoder, copied from Dragon Detector with its upstream ledger and license |
| [Espressif Arduino SD](https://github.com/espressif/arduino-esp32/tree/3.0.7/libraries/SD/src) | 3.0.7 plus upstream fixes `4e086ffd` and `c1edb834` | Apache-2.0 local SD driver with SPI reset and write-completion backports; [details and full commit links](../lib/DashChanSD/README.md) |
| [Biscuit GATT reference](https://codehedge.github.io/Biscuit-Wiki/3rd-party-integration/gatt-reference.html) | Reviewed 2026-10-16 | Advertising name/service reference; no proprietary firmware included |

Dragon Detector's upstream credits include JustCallMeKoko/ESP32 Marauder, Flock Noir, Colonel Panic/Flock-You/AXEOFF, NitekryDPaul, DeFlockJoplin, wgreenberg, CodeHedge, Bluetooth SIG, and community device researchers. Retained MIT notices from [Marauder](licenses/MARAUDER-LICENSE.txt) and [Flock Noir](licenses/FLOCK-NOIR-LICENSE.txt) accompany imported derived material. See [Dragon Detector's full ledger](https://github.com/valleytechsolutions/Dragon-Detector/blob/main/CREDITS.md).

## Detection grading audit

The confidence grades in [DETECTIONS](DETECTIONS.md) come from two projects, and both deserve the credit.

**Dragon Detector** supplies the parser and every signature Dash-Chan matches: the vendor registry checked against the IEEE-derived Wireshark list, the Axon combiner, the Flock probe fingerprint, the Find Hub length check and the Remote ID decoding.

**SquachWatch-CYD** did the audit that decides how much each signature is worth. Its authors read every address block against the IEEE registry and every company and service ID against the Bluetooth SIG lists, graded each signature on its own instead of by type, and wrote down what they removed and why. From SquachWatch `33a8c30` Dash-Chan takes, as facts and rule behaviour reimplemented in its own code:

- the per-row grades for address blocks, and the grades for Axon, Tile, SmartTag, Chipolo, Flipper, Pwnagotchi, glasses and skimmer signatures;
- the removals: the Espressif block `BC:DD:C2`, the bare `FEAA` service, the `00:13:37` "Pineapple" block, and Flipper service `3080`;
- the two broadcast disconnect rules (6 from one sender, 24 from any senders, in 3 seconds);
- the same-maker exclusion for SSID conflicts, including ignoring the locally administered bit;
- the iBeacon label format (three UUID bytes, then `major.minor`) and the vendor sub-labels.

Where Dash-Chan keeps a lower grade than SquachWatch, [DETECTIONS](DETECTIONS.md#where-the-grades-differ-from-squachwatch) lists each case and the reason. SquachWatch's own sources for these facts include Flock-You and OUI-SPY AXEOFF (Colonel Panic), NitekryDPaul and DeFlockJoplin, all-cameras-are-beacons (soyboi1312), AirGuard (seemoo-lab), the SparkFun Skimmer Scanner, ESP32 Marauder, the IEEE MA-L registry and the Bluetooth SIG assigned numbers.

### Local changes to the Dragon snapshot

The files under `include/vendor/dragon` are Dragon Detector `3204e7a` with these changes. Changed regions are marked `Local (Dash-Chan)` in the source. `remote_id.h` is unchanged.

`detection.h`

1. `Match` has an optional `label` naming the product inside a family; `named()` and `parseBle()` set it.
2. New helpers `containsWord()` and `startsWith()`.
3. `named()` checks documented forms first and grades them: exact `Flock`, `Flock-`, `Flock_`, `Flock Camera`, names starting with the word `Axon`, names containing `BodyWorn`, SSIDs `AB2-` / `AB3-` / `AB4-`, and BLE names `FS Ext Battery`, `FS-` plus hex, `Penguin-` plus ten digits, `Pigvision`, and the exact serial-module names are Strong; SSIDs `Pineapple_` and `pwned` and BLE names starting `Flipper` are Medium. The older substring rules remain below them at Low.
4. `named()` matches `axon` as a whole word, not as a substring.
5. `parseWifi()` grades a Pwnagotchi peer beacon Strong from any address (was Medium unless sent from `DE:AD:BE:EF:DE:AD`).
6. `parseWifi()` no longer reports `00:13:37` as a Pineapple.
7. `parseBle()` accepts Flipper services `3081` to `3083` only (was `3080` to `3083`) and grades them Strong without the appearance value, which is no longer read.
8. `parseBle()` no longer reports a bare `FEAA` service.
9. `parseBle()` regrades: `BWCDEVICE` marker, Axon company or service ID, SmartTag `FD5A` and Tile services to Strong; Chipolo, the serial-port service `1101`, Ray-Ban Meta `FD5F` alone, and Luxottica, Snap or Vuzix identifiers alone to Medium. A Meta company ID or `FEB7` / `FEB8` alone stays Low.
10. `parseBle()` files the DULT service under `GoogleTag` (was `Tracker`).
11. `parseBle()` splits the Xuntong composite: with a Penguin or FS Ext Battery name it is Strong, with a bare ten-digit name Medium.
12. `parseBle()` tests Tile before Chipolo, and compares the name match again at the end so that a stronger documented name is not replaced by a weaker payload clue.

`axon.h`

13. `AxonEvidence::match()` grades the `BWCDEVICE` marker, company ID, service ID and the `00:25:DF` block Strong on their own, reads them before the Medium WatchGuard / Digital Ally clue, and sets labels.
14. `axonName()` matches `axon` as a whole word.
15. `withAxon()` replaces the parsed match only when the Axon clue has the higher confidence. Before, any Axon clue replaced any non-Axon match.

`vendor_prefixes.h`

16. Removed `BC:DD:C2` (Espressif).
17. Regraded to Strong: Axon `00:25:DF`; Flock Safety `B4:1E:52`; ShotSpotter `D4:11:D6`; Motorola Solutions `00:04:7D`, `00:18:85`, `00:1F:92`, `4C:CC:34`, `B8:E2:8C`; Genetec `00:BF:15`, `0C:BF:15`; Wyze `2C:AA:8E`, `D0:3F:27`, `7C:78:B2`; Hikvision `C0:56:E3`, `44:19:B6`, `28:57:BE`; Arlo `FC:9C:98`, `48:62:64`, `A4:11:62`; Verkada `E0:A7:00`; Avigilon Alta `70:1A:D5`; Axis `00:40:8C`, `B8:A4:4F`; i-PRO `D4:2D:C5`; Skydio `38:1D:14`; and the thirteen Ring LLC blocks.
18. Regraded from Low to Medium: Luxottica Group `98:59:49`; Parrot `90:03:B7`, `90:3A:E6`, `A0:14:3D`.

Mochi, Sprout, and Orbit are original code-drawn designs in `include/avatars.h`; the same drawing routine generates `media/companions.svg`. Alert voice clips are locally synthesized speech of the requested original phrases, compiled as PCM WAV data; no recordings from third-party firmware are included.

Avatar Studio's six eye styles, layered outfits, presets, handheld antenna, receiver, and travel animation are original Dash-Chan code-drawn artwork. Version 0.3 adds no copied third-party avatar assets or additional upstream detector code. The browser renderer independently mirrors those primitives for previews and portrait export.

SquachWatch's expressive emote sequences and companion-centered interactions were reviewed as design references for blinking, breathing, panic, and affection. Dash-Chan implements its own vector companions and bounded servo animation; it does not copy SquachWatch's mascot, sprites, sound recordings, or UI source. The reviewed SquachWatch repository is GPL-3.0. Its authors and the upstream researchers deserve credit for their detection work.

The BWM and Valley Tech Solutions PNG marks were supplied by the project owner for the boot screen and website. Original files are retained unchanged in `assets/brands`; `tools/pack_logos.py` only prepares LCD-sized encoded copies. Brand/trademark rights remain with their owners and are not granted by Dash-Chan's source-code license. See [brand asset notice](../assets/brands/NOTICE.md).

Runtime libraries are fetched with their original licenses: M5Unified/M5GFX/StackChan-BSP (MIT), NimBLE-Arduino (Apache-2.0 and included notices), TinyGPSPlus (LGPL-2.1), Arduino ESP32 (LGPL-2.1 with ESP-IDF component notices). Dash-Chan's GPL-3.0-or-later license does not replace those notices or imply that upstream authors endorse this project.

## GPS preset research (0.4)

The [GPS compatibility table](GPS.md) links each M5Stack receiver's official product documentation, the K151 pin map, and the ZhongKe ATGM336H-5N/-6N manufacturer manuals. Presets are original baud/connection metadata; the bounded NMEA framing and search implementation are original Dash-Chan code. No manufacturer firmware or manual artwork is bundled. The reviewed modules are UART/NMEA compatible with appropriate adapters; only Unit GPS v1.1 has been bench tested on this robot. Wisp and Ember are original vector characters rendered in firmware and the web studio.

## Arcade Scout (0.5)

The smiling one-antenna radar-head mark in `site/assets/dashchan.svg`, firmware mark primitives, four environments, Recon/Face layouts, and website console shell are original Dash-Chan artwork. The virtual-pet aesthetic is a design direction; no Tamagotchi branding, characters or graphics are copied. Avatar Studio remains original code-drawn art, with all customization available immediately.

Hardware references: [official K151 specifications](https://docs.m5stack.com/en/StackChan), the pinned StackChan BSP's touch/RGB/NFC examples, and the manufacturer's LTR-553ALS register definitions. NFC uses [M5Unit-NFC 0.1.2](https://github.com/m5stack/M5Unit-NFC) under its MIT license; the original notice is included in flash packages. Outfit UID mapping, settings, UI and reaction logic are original code. Tag contents are not read.

Upload integration references, checked 2026-10-17: [WiGLE API specification](https://api.wigle.net/swagger.json) and [WDGWars developer documentation](https://www.wdgwars.pl/press). Dash-Chan implements multipart transport, preview, conversion and receipt handling independently. No service firmware source or logos were imported. HTTPS uses the ESP-IDF certificate bundle and HTTP client from the pinned Arduino/IDF runtime. Reference and integration do not imply endorsement.
