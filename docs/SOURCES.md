# Sources and attribution

Reviewed on 2026-10-16. Reference clones are kept outside the published tree.

| Source | Pinned snapshot | Use |
|---|---|---|
| [Dragon Detector](https://github.com/valleytechsolutions/Dragon-Detector) | `3204e7a0caa3b107e751e929ec63ec1f80739828` | MIT passive parser, vendor facts, Axon and Remote ID helpers copied under `include/vendor/dragon`; original [MIT notice](licenses/DRAGON-DETECTOR-MIT.txt) retained |
| [SquachWatch-CYD](https://github.com/skizzophrenic/SquachWatch-CYD) | `9e06e0461fca45eb3a80ca18436556b4f088efbb` | Detection/category/confidence reference; selected public signature facts reviewed; no artwork copied |
| [ESP32 Dual Band Wardriver](https://github.com/justcallmekoko/ESP32DualBandWardriver) | `c142254647ed7799d7b31e7f7114f30265011619` | WiGLE Wi-Fi/BLE row conventions and SD/GPS workflow reference; [MIT notice](licenses/KOKO-MIT.txt) retained |
| [Piglet / Midwest Gadgets](https://github.com/Hamspiced/piglet) | `fb71eded9c7bdec6b646f14dff3711ff4bd1bf5a` | Reviewed GPS quality gates, buffering, SD and session behavior. Source LICENSE at this snapshot is CC BY-NC-SA 4.0; no Piglet code bundled |
| [M5Stack StackChan-BSP](https://github.com/m5stack/StackChan-BSP) | `8d4d6fc3b7a6be379c6317c45a02a30bff8c492e` | MIT board support, actual serial-servo control and factory calibration handling |
| [OpenDroneID core](https://github.com/opendroneid/opendroneid-core-c) | `849c03adbe0cdd68997a462fafab397edcfe53f3` | Apache-2.0 decoder, copied from Dragon Detector with its upstream ledger and license |
| [Espressif Arduino SD](https://github.com/espressif/arduino-esp32/tree/3.0.7/libraries/SD/src) | 3.0.7 plus upstream fixes `4e086ffd` and `c1edb834` | Apache-2.0 local SD driver with SPI reset and write-completion backports; [details and full commit links](../lib/DashChanSD/README.md) |
| [Biscuit GATT reference](https://codehedge.github.io/Biscuit-Wiki/3rd-party-integration/gatt-reference.html) | Reviewed 2026-10-16 | Advertising name/service reference; no proprietary firmware included |

Dragon Detector's upstream credits include JustCallMeKoko/ESP32 Marauder, Flock Noir, Colonel Panic/Flock-You/AXEOFF, NitekryDPaul, DeFlockJoplin, wgreenberg, CodeHedge, Bluetooth SIG, and community device researchers. Retained MIT notices from [Marauder](licenses/MARAUDER-LICENSE.txt) and [Flock Noir](licenses/FLOCK-NOIR-LICENSE.txt) accompany imported derived material. See [Dragon Detector's full ledger](https://github.com/valleytechsolutions/Dragon-Detector/blob/main/CREDITS.md).

Mochi, Sprout, and Orbit are original code-drawn designs in `include/avatars.h`; the same drawing routine generates `media/companions.svg`. Alert voice clips are locally synthesized speech of the requested original phrases, compiled as PCM WAV data; no recordings from third-party firmware are included.

Runtime libraries are fetched with their original licenses: M5Unified/M5GFX/StackChan-BSP (MIT), NimBLE-Arduino (Apache-2.0 and included notices), TinyGPSPlus (LGPL-2.1), Arduino ESP32 (LGPL-2.1 with ESP-IDF component notices). Dash-Chan's GPL-3.0-or-later license does not replace those notices or imply that upstream authors endorse this project.
