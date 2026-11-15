# Detections: families, evidence and confidence

Dash-Chan listens passively to 2.4 GHz Wi-Fi and Bluetooth LE and compares what it hears with published signatures. This page lists every family it can report, what the family is, exactly what triggers it, and the confidence each trigger carries.

The parser is the Dragon Detector snapshot pinned at `3204e7a0caa3b107e751e929ec63ec1f80739828`, with the local changes listed in [SOURCES](SOURCES.md). The grades were audited against SquachWatch-CYD `33a8c3091eac1a233a15ca976ab9e072b053d210`, whose per-signature grading is the model for the tables below. Colonel Panic's Unified Blue `6e5e2829540925ff1ab5fa6d0d365378818ed56e` was the earlier method reference: its address, OUI, company ID, name and service filters, its Flock probe and battery methods, AXEOFF and SkySpy are all covered by rows below. These projects and the researchers they credit are acknowledged in SOURCES. No active radio tools are imported.

## What a grade means

| Grade | Meaning |
|---|---|
| **High** | The identifier belongs to the product family: an address block registered to its maker, a Bluetooth SIG company or service ID assigned to it, a fixed payload format, or a default name in its documented form. |
| **Medium** | Real evidence that is shared more widely than the family: a parent company that makes other things, one research service nobody has checked against hardware, or a traffic pattern with ordinary explanations. |
| **Low** | A guess: a chip or module maker's address block, a locally administered address, a word found inside a longer name, a bare serial number. |

Medium or higher triggers notifications by default. Low-confidence clues can be enabled in Detection settings.

A grade describes the signature, never the situation. High does not mean a camera is recording, that police are present, or that a tracker is following you. Every identifier on this page is sent in the clear and can be copied by another transmitter. Equipment may also be silent, out of range, on another channel, or using a protocol this hardware cannot hear.

## Families

Address blocks are the first three bytes of a Wi-Fi transmitter address or a **public** BLE address. Service and company IDs are Bluetooth SIG 16-bit values. A family with no row at a grade has no signature at that grade.

| Family | What it is | How it is detected | Confidence |
|---|---|---|---|
| **Axon / body camera** | Axon and TASER equipment, plus WatchGuard, Digital Ally and BodyWorn body cameras | Company ID `034D`. Services `FC81`, `FE6B`, `FE6C`. The text `BWCDEVICE`, in either byte order, inside manufacturer data or a 128-bit UUID field. Address block `00:25:DF`. A name or SSID that starts with the word `Axon`. SSIDs starting `AB2-`, `AB3-`, `AB4-` (Axon Body pairing networks). A name containing `BodyWorn`. | High |
| | | WatchGuard Video `00:1D:96`, Digital Ally `00:23:BD` | Medium |
| | | `Axon` as a separate word later in a name. `AB2` / `AB3` / `AB4` in any other form. A random BLE address that merely starts `00:25:DF`. A custom UUID that embeds one of the service numbers. Shared-module blocks `E4:05:40`, `28:24:FF`. | Low |
| **Flock camera** | Flock Safety plate-reader cameras and their battery packs | Flock Safety's own block `B4:1E:52`. The accessory service `e8ccbb38-9532-46a8-9fe5-1814df172e6f`. A name or SSID that is exactly `Flock`, or starts `Flock-`, `Flock_` or `Flock Camera`. BLE names `FS Ext Battery`, `FS-` plus 4 to 12 hex digits, `Penguin-` plus ten digits, and names starting `Pigvision`. Company ID `09C8` (Xuntong) together with a Penguin or FS Ext Battery name. | High |
| | | A wildcard probe request from a research block whose element order and vendor bytes match the published fingerprint exactly. Company ID `09C8` with a bare ten-digit name. | Medium |
| | | Thirty module-maker blocks from the Flock-You field lists (Liteon, USI, Silicon Labs, Atheros and others). Company ID `09C8` alone. A bare ten-digit name. Any other name containing `flock`, `penguin` or `pigvision`. The local address pattern `82:6B:F2`. | Low |
| **ALPR vendor** | Companies that sell licence-plate readers. The block is certain; that a given device on it reads plates is not. | Motorola Solutions `00:04:7D`, `00:18:85`, `00:1F:92`, `4C:CC:34`, `B8:E2:8C`. Genetec `00:BF:15`, `0C:BF:15`. | High |
| | | Motorola Solutions `10:74:6F`, `9C:86:2B` | Low |
| **Camera glasses** | Smart glasses with a camera: Ray-Ban Meta, Oakley Meta, Snap Spectacles, Vuzix | Service `FD5F` (Ray-Ban Meta), alone or with company ID `01AB` or `0D53`. Company IDs `0D53` (Luxottica), `03C2` (Snap), `060C` (Vuzix). Service `FE45` (Snap). Company `01AB` or `0D53` together with service `FEB7` or `FEB8`. Luxottica Group block `98:59:49`. | Medium |
| | | Company ID `01AB` (Meta) alone. Services `FEB7`, `FEB8` alone. Luxottica Tristar blocks `38:47:12`, `80:AA:1C`. Names containing `Ray-Ban`, `Rayban` or `Oakley Meta`, or starting `Spectacles`. | Low |
| **Find My / tracker** | An Apple Find My accessory: an AirTag or any other tag or product on that network | Apple manufacturer data of type `12`, length `19`, at least 29 bytes long. Service `FD44`. | Medium |
| | | Any other 29-byte Apple manufacturer block (also AirPods and other accessories). A name containing `AirTag`. | Low |
| **Samsung SmartTag** | Galaxy SmartTag and SmartTag2 | Service `FD5A` | High |
| | | A name containing `SmartTag` | Low |
| **Google / Chipolo tag** | Trackers on Google's Find Hub network, Chipolo tags, and any tag announcing the cross-maker unwanted-tracker standard (DULT) | Find Hub: service data `FEAA` whose frame type is `40` or `41` and whose ephemeral ID is a valid length (20 or 32 bytes, optionally one flags byte) | High |
| | | DULT service `FCB2` or the 128-bit DULT UUID. Chipolo services `FE33`, `FE65`, company ID `08C3`. | Medium |
| **Tile tracker** | Tile tags | Services `FEED`, `FEEC`, `FD84` | High |
| **Ring camera** | Ring doorbells and cameras | The thirteen blocks registered to Ring LLC: `00:B4:63`, `18:7F:88`, `24:2B:D6`, `34:3E:A4`, `50:E4:67`, `54:E0:19`, `5C:47:5E`, `64:9A:63`, `90:48:6C`, `9C:76:13`, `AC:9F:C3`, `C4:DB:AD`, `CC:3B:FB` | High |
| | | Amazon-wide blocks `FC:65:DE`, `68:37:E9`. A name starting `Ring`. | Low |
| **Camera vendor** | Wi-Fi cameras from makers that sell little else | Wyze `2C:AA:8E`, `D0:3F:27`, `7C:78:B2`. Hikvision `C0:56:E3`, `44:19:B6`, `28:57:BE`. Arlo `FC:9C:98`, `48:62:64`, `A4:11:62`. Verkada `E0:A7:00`. Avigilon Alta `70:1A:D5`. Axis `00:40:8C`, `B8:A4:4F`. i-PRO `D4:2D:C5`. | High |
| | | Blink's six blocks. Eighty-three further Hikvision blocks, Wyze `80:48:2C`, `F0:C8:8B` and Axis `AC:CC:8E`, `E8:27:25`, which only Dragon lists. | Medium |
| | | Amazon-wide blocks `34:D2:70`, `F0:27:2D`. Module makers Realtek `00:E0:4C`, Telink `A4:C1:38`, Murata `B8:D7:AF` and `4C:69:05`. | Low |
| **Raven candidate** | Acoustic gunshot sensors: Flock Raven and ShotSpotter | ShotSpotter's block `D4:11:D6`. All five Raven research services `3100`, `3200`, `3300`, `3400`, `3500` in one advertisement. | High |
| | | Any one of those five services. Nothing between them matches. | Medium |
| | | A name starting `Raven` | Low |
| **Remote ID drone** | Drones, by their legally required Remote ID broadcast or by maker | A decoded ASTM F3411 message: BLE service data `FFFA` with application code `0D`, a Wi-Fi beacon vendor element `FA:0B:BC` type `0D`, or a Wi-Fi NAN action frame. Skydio's block `38:1D:14`. | High |
| | | DJI's ten main blocks. Parrot `90:03:B7`, `90:3A:E6`, `A0:14:3D`. | Medium |
| | | DJI Baiwang's four blocks. Parrot `00:12:1C`, `00:26:7E` (mostly car kits). Service `FFFA` with nothing decodable behind it. | Low |
| **Flipper Zero** | The Flipper Zero multi-tool | Service `3081`, `3082` or `3083`, one per case colour | High |
| | | A BLE name starting with the word `Flipper` (the default; the owner can change it) | Medium |
| | | `flipper` anywhere else in a BLE name, or an SSID starting `Flipper` | Low |
| **Pwnagotchi** | A handshake-collecting gadget that announces itself to its peers | A beacon carrying the key `pwnd_tot` and a quoted, printable `name` | High |
| | | A name containing `pwnagotchi` | Low |
| **WiFi Pineapple** | Hak5 WiFi Pineapple, and the ESP deauther family | SSID starting `Pineapple_` (the default management network). SSID starting `pwned` (the deauthers' default control network; labelled Deauther AP). | Medium |
| | | Local address patterns `02:C0:CA`, `02:13:37`. Names containing `pineapple` or `hak5`. BLE names starting `pwned`. Names starting `deauther`. | Low |
| **Biscuit family** | Biscuit Pro and Ultra | A name starting `Biscuit` together with its 128-bit service | Medium |
| | | The name alone | Low |
| **Nyanbox**, **HaleHound** | Hobby multi-tools | A name containing `nyanbox` or `halehound` | Low |
| **Serial / skimmer** | Generic Bluetooth serial modules, the part card skimmers are built from. Most are harmless projects. | A BLE name that is exactly `HC-03`, `HC-05`, `HC-06`, `RN42` or `BT04-A` | High |
| | | The serial-port service `1101` | Medium |
| | | Those names with anything after them, and `HC-08`. Module-maker blocks `20:13:00`, `98:D3:31`, `00:1A:7D`. | Low |
| **Proximity beacon** | Apple iBeacon: a fixed transmitter that apps use for indoor positioning. Usually ordinary shop or venue equipment. | Apple manufacturer data `4C 00 02 15` of exactly 25 bytes | High |
| | | The same header with extra bytes | Low |
| **Disconnect burst** | A run of Wi-Fi deauthentication or disassociation frames. See [traffic rules](#traffic-rules). | Any of the three rate rules | Medium |
| **SSID conflict** | Two access points using one network name and disagreeing about security: the shape of an evil twin. See [traffic rules](#traffic-rules). | The confirmed-pair rule | Medium |
| **BLE advert burst** | A flood of pairing pop-up advertisements. See [traffic rules](#traffic-rules). | The distinct-address rule | Medium |
| **My watchlist** | One of your own [watchlist](#personal-watchlist) rules matched | An exact rule from the SD card | High |

### How matching works

- **Addresses.** Registry blocks apply to Wi-Fi transmitter addresses and to BLE public addresses. A random BLE address is never treated as a vendor assignment. A Wi-Fi data frame *addressed to* a listed block is reported with that block's grade and the evidence "Receiver address vendor clue"; it is never an access-point observation.
- **Services.** A 16-bit service matches in the service list and in service data, including its zero-extended 32-bit form and its exact Bluetooth Base UUID form. Custom 128-bit UUIDs are never shortened.
- **Names.** SSIDs are read from beacons and probe responses only: the SSID in a probe request names the network a phone is looking for, not the phone. BLE names are read from the advertisement only (see [limits](#limits)).
- **Strongest evidence wins.** Within one advertisement the stronger of the payload match and the name match is kept; on a tie the payload wins. An Axon clue replaces another family's match only when the Axon clue is the stronger one.

## Traffic rules

These report a pattern of traffic. None of them identifies who sent it or why.

| Rule | Fires when | Confidence |
|---|---|---|
| Disconnect burst, one sender | 12 deauthentication or disassociation frames from one transmitter within 2 seconds | Medium |
| Disconnect burst, broadcast from one sender | 6 such frames addressed to broadcast from one transmitter within 3 seconds | Medium |
| Disconnect burst, broadcast from many senders | 24 such frames addressed to broadcast within 3 seconds, from any mix of transmitters. This is what a tool that spoofs one access point after another looks like. | Medium |
| SSID conflict | The same non-empty SSID from two access points, one open and one protected, whose makers differ, each seen at least three times within 30 seconds | Medium |
| BLE advert burst | 24 distinct addresses within 5 seconds in one pairing class: Apple proximity pop-ups, Microsoft Swift Pair, or Google Fast Pair | Medium |

- Retransmissions, frames repeating the previous sequence number, and protected (802.11w) frames are not counted as disconnects. Sixteen transmitters are tracked at once. Each rule reports a burst once, then stays quiet for 30 seconds.
- Makers are compared on the first three address bytes with the locally administered bit ignored, because a mesh node or a second radio often derives its address from the base address by setting that bit. Two access points from one maker are never reported, whatever they say about security. Sixty-four access points are tracked.
- The pair is reported on whichever beacon completes the condition. The rule cannot tell which of the two is the impostor, and a guest network on different hardware from the main one will also match.
- A Pwnagotchi's beacon is left out of the SSID rule: its SSID is throwaway.
- A crowded venue can produce a BLE advert burst with no attacker present.

## Labels

Besides the family, the classifier returns a short label (at most 23 printable characters) naming the product inside the family when the signature says so:

| Family | Label |
|---|---|
| Axon / body camera | `Axon Body 2`, `Axon Body 3`, `Axon Body 4` (pairing SSID), `Axon body cam` (the `BWCDEVICE` marker), `Axon / TASER` (company or service ID), `BodyWorn`, `WatchGuard`, `DigitalAlly` |
| Camera glasses | `Ray-Ban Meta`, `Meta glasses`, `Meta`, `Luxottica`, `Snap Spectacles`, `Vuzix` |
| Trackers | `Find My`, `SmartTag`, `Find Hub`, `Chipolo`, `DULT tracker`, `Tile` |
| Flock camera | `Flock battery`, `Flock accessory` |
| WiFi Pineapple | `Pineapple AP`, `Deauther AP` |
| Proximity beacon | The first three bytes of the proximity UUID, then `major.minor`, for example `A1B2C3 258.772`. The UUID names the deployment, so the same six hex digits in two places are the same operator. |
| Pwnagotchi | The unit's own name from its beacon |
| Remote ID drone | The serial number or session ID the aircraft broadcasts |
| Any address-block match | The registrant, for example `Hikvision`, `Ring LLC`, `Motorola Solutions`, `DJI`. Blocks listed only as a shared radio vendor have no label. |

## Where the grades differ from SquachWatch

SquachWatch grades each address block itself but gives BLE IDs, names and SSIDs the grade of their whole type. Dash-Chan follows SquachWatch wherever its notes justify the grade for that one signature, and keeps the lower grade where they do not:

| Signature | SquachWatch | Dash-Chan | Why |
|---|---|---|---|
| Company ID `09C8` (Xuntong) alone | High | Low | Xuntong supplies radio modules. SquachWatch's own rule for address blocks grades a module maker Low. |
| Amazon-wide blocks `FC:65:DE`, `68:37:E9`, `34:D2:70`, `F0:27:2D` | Medium | Low | They are Echo, Fire TV and Kindle too, as SquachWatch notes. Medium alerts by default here. |
| Company ID `01AB`, services `FEB7`, `FEB8`, each alone | Medium | Low | SquachWatch notes that Quest headsets send them and that real glasses send `01AB` with `FD5F`. |
| Serial-port service `1101` | High in code, Medium in its notes | Medium | The lower of its two statements. |
| `Flock`, `Penguin`, `Pigvision`, `Axon`, `Flipper` anywhere in a BLE name | High (Flipper Medium) | Low unless in a documented form | A substring is a guess. `Axon` must stand as a word, so Saxon, Klaxon and taxonomy never match. |
| iBeacon header with more than 25 bytes | High | Low | The format is fixed at 25 bytes. |
| Service data `FFFA` with code `0D` that does not decode | Medium | Low | Nothing was verified. |
| SSID conflict | High, on the first conflicting beacon | Medium, after three sightings of each | Legitimate networks do this too, and the rule cannot tell which access point is the impostor. |
| Disconnect burst, one sender | 12 frames in 3 seconds | 12 frames in 2 seconds | Unchanged from earlier Dash-Chan releases. The two broadcast rules use SquachWatch's numbers as they are. |

Dash-Chan stays *above* SquachWatch's type-level Medium in three places where Dragon checks more than SquachWatch does: a Find Hub frame with a valid ID length, all five Raven services together, and a fully decoded Remote ID message are High.

Address blocks that only Dragon carries keep Dragon's grade until someone re-reads them against the IEEE registry: the extra Hikvision, Wyze and Axis blocks (Medium), Motorola `10:74:6F` and `9C:86:2B`, and Luxottica Tristar (Low).

## Removed false positives

| Was | Why it is gone |
|---|---|
| `BC:DD:C2` as a camera | The block is Espressif's: every ESP32 in it, this robot's relatives included, read as a camera. |
| Service `FEAA` alone as a Google tag | That is every Eddystone shop beacon. Find Hub is matched on its frame type and length instead. |
| `00:13:37` as a Pineapple | Hak5 holds no IEEE block. Only the two locally administered patterns remain, at Low. |
| Service `3080` as a Flipper | A Flipper Zero advertises `3081`, `3082` or `3083`. |
| `axon` inside another word | See the table above. |
| A weak Axon clue overriding a stronger match | A Low Axon name or shared-module block used to replace a High tracker or drone match. |

## Limits

- **Scanning is passive.** Dash-Chan never sends a scan request, so a BLE name that a device puts only in its scan response is never seen. Name rules work on the advertisement itself; company IDs, services and service data are unaffected.
- There is one 2.4 GHz radio. It cannot hear 5 GHz or 6 GHz Wi-Fi, Bluetooth Classic (where most serial modules actually live), Bluetooth 5 Long Range, sub-GHz, LoRa or Meshtastic, and it hops channels, so a short burst on another channel is missed.
- Remote ID position and identity are whatever the aircraft says they are.
- Method coverage is not firmware parity with any reference project. Direction finding, packet capture and every active workflow are out of scope.
- Tests use synthetic fixtures and random malformed packets. They pin the rules on this page; they do not establish real-world recall or the absence of false positives. Field comparison with known Axon, ALPR, glasses, tag, Flipper and Biscuit hardware is still needed.

## Quiet encounters and recent signals

- A separate 128-entry ledger keys encounters by MAC, transport, BLE address type, and family. Continuing observations update last-seen time and stay quiet after the first notification.
- Five minutes without a qualifying observation rearms an encounter. Stronger evidence can also notify after the global/family gates.
- Default family quiet time is 60 seconds; 180 and 300 seconds are selectable. Global notification spacing is ten seconds. These apply even when a new rotating BLE address appears.
- Optional continuing-encounter reminders every five minutes are **off** by default.
- A 32-entry recent list updates counts/RSSI/last-seen information without replaying audio. Four entries per page are available; details show evidence and confidence.
- The general observation cache holds 256 contacts and expires after two minutes unseen. Encounter suppression is separate, so ordinary contact-cache churn does not immediately rearm an alert. More than 128 relevant encounters can evict suppression entries.
- Recent/encounter state is in RAM and resets on reboot. `/events/detections.csv` contains emitted non-demo alerts when SD is available. Raw advertisements are not persisted.

## Personal watchlist

Create `/dashchan/watchlist.csv` on FAT32 SD. Stop recording and restart or use GPS & SD → RETRY SD to reload. Up to 16 rules are accepted. Use one rule per line, no CSV header; `#` starts a comment.

```text
# Examples only; replace with your own research identifiers.
MAC,001122334455
OUI,001122
NAME,My test beacon
COMPANY,034D
SERVICE,FC81
```

MAC/OUI values are hexadecimal without separators. Company/service IDs are four hex digits. Names are case-insensitive substrings up to 24 characters. A service rule matches standard 16-bit services, including their exact Bluetooth Base UUID representation; it does not shorten arbitrary custom 128-bit UUIDs. Full MAC rules can match random addresses exactly, but these can rotate. OUI rules only apply to public/global unicast addresses. Matches are shown as **My watchlist** with explicit unauthenticated-identity evidence.

The Wi-Fi data-header callback only forwards built-in vendor candidates to keep the queue bounded; a custom arbitrary MAC rule is available on received probes/AP frames and BLE, not every data frame. Captured data candidates retain only 24 address/header bytes. Nothing sends traffic to the watched device.

## What goes into WiGLE

Only valid Wi-Fi beacons/probe responses describing AP BSSIDs, and valid BLE observations, can become WiGLE rows. Client probes, data headers, disconnect frames, and NAN action frames can produce detections but are never mislabeled as access points. A Pwnagotchi's beacon is a detection only: it announces a gadget, not a network, so it is never written as an access point. All rows must also pass the session, GPS, age, and write-success gates. See [architecture](DEVELOPMENT.md).
