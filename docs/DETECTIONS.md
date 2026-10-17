# Evidence, methods, and coverage

Dash-Chan uses the passive Dragon Detector headers pinned at `3204e7a0caa3b107e751e929ec63ec1f80739828`, including its Axon combiner, vendor registry, and OpenDroneID helpers. It adds bounded encounter handling, watchlists, and passive traffic hints. No active radio tools are imported.

## Reference audit

Reviewed: **Colonel Panic Unified Blue `6e5e2829540925ff1ab5fa6d0d365378818ed56e`**, **SquachWatch-CYD `9e06e0461fca45eb3a80ca18436556b4f088efbb`**, and the pinned Dragon Detector source. Links and licensing are in [SOURCES](SOURCES.md).

| Reference method | Dash-Chan implementation / qualification |
|---|---|
| Full MAC, OUI, company ID, name, 16-bit service filters | Built-in evidence plus user watchlist rules. BLE random addresses are not treated as IEEE vendor assignments. Standard 16-bit services also normalize zero-extended 32-bit and exact Bluetooth Base UUID forms. |
| Meta/Luxottica company + Meta service composite | Correlated within the parsed advertisement. Name-only or service-only claims remain weak; recording state is unknown. |
| Axon OUI, company `034D`, `FC81`/`FE6B`/`FE6C` services, supported payload evidence | Dragon Axon combiner across supported Wi-Fi/BLE frames. Exact model and recording state are not asserted. |
| Flock transmitter/receiver prefixes | Source-address registry and receiver-header clues. A receiver clue is not an AP observation and does not establish a camera. Broad shared supplier prefixes remain low confidence. |
| Flock wildcard probe + exact IE sequence/vendor fields | Bounded probe parser and the Dragon fingerprint rule. A malformed/truncated IE sequence fails closed. |
| Flock battery names + `09C8` manufacturer; accessory 128-bit service | Name/company composite and exact service. The battery/accessory does not prove a camera is present. Generic `09C8` alone stays weak. |
| Raven exact `3100`–`3500` research services | Exact values, not a blanket numeric range. Full set is stronger than a single service. Field validation remains needed. |
| SkySpy / Remote ID | Decodes BLE, Wi-Fi beacon, and supported Wi-Fi NAN action payloads through OpenDroneID. Broadcast identity/position are self-reported. |
| SquachWatch/Dragon tracker families | Find My, DULT, Samsung SmartTag, Tile, supported Find Hub formats, and qualified iBeacon format. Broader protocol family does not identify a specific AirTag or owner. |
| SquachWatch gadget clues | Flipper service/appearance, Biscuit name+service, Pineapple-style names/address clues, Pwnagotchi peer fields, Nyanbox/HaleHound names, generic serial-module clues. |
| Disconnect traffic | 12 non-retry, unprotected deauth/disassociation frames from one source within 2 seconds. Sequence duplicates are ignored. This reports a burst, not intent. |
| SSID security conflict | Same nonempty raw SSID, different BSSIDs, open versus protected, at least three observations of each within 30 seconds. Low confidence: legitimate configurations can do this. |
| BLE pairing burst | At least 24 distinct advertising addresses in five seconds within one Apple popup / Swift Pair / Fast Pair class. Crowded environments can qualify; no attacker is identified. |

**Method coverage does not mean complete firmware/mode parity.** Unified Blue's Foxhunter navigation, PCAP tools, raw capture interfaces, and active workflows are not ported. Dash-Chan does not implement RF direction finding, sub-GHz/LoRa/Meshtastic hardware, Bluetooth Classic inquiry, or 5 GHz reception. The single ESP32-S3 cannot supply those missing radios. No claim is made that every signature in a changing upstream repository is covered or field-verified.

## Families and interpretation

| Display family | Important limit |
|---|---|
| Axon / body camera | Matching radio signature; not proof that police are present |
| Flock / ALPR vendor | Shared chip makers and vendors may make unrelated products |
| Camera glasses | Model, wearer, and recording state are unknown |
| Find My / tracker | May be an accessory other than an AirTag; does not establish ownership or stalking |
| Flipper Zero | Name/service evidence; activity is unknown |
| Biscuit family | Passive evidence cannot distinguish Pro from Ultra |
| WiFi Pineapple / Pwnagotchi | Configurable/spoofable identity and peer fields |
| Nyanbox / HaleHound | Name clues are weak |
| Serial / skimmer | Ordinary serial modules are common and benign |
| Camera / Ring vendor | Vendor evidence only |
| Raven candidate | Community research signatures, not authenticated identity |
| Remote ID drone | A decoded public broadcast, not a verified aircraft identity |
| Proximity beacon | Often ordinary benign iBeacon infrastructure |
| Disconnect / SSID conflict / BLE burst | Traffic patterns with plausible benign explanations |
| My watchlist | A user rule matched; the underlying identity is still spoofable |

Medium or higher triggers notifications by default. Low-confidence clues can be enabled in Detection settings. “High” means strong signature evidence, not certainty about a physical device or person. Equipment may be silent, out of range, on another channel, or using an unsupported protocol.

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

Only valid Wi-Fi beacons/probe responses describing AP BSSIDs, and valid BLE observations, can become WiGLE rows. Client probes, data headers, disconnect frames, and NAN action frames can produce detections but are never mislabeled as access points. All rows must also pass the session, GPS, age, and write-success gates. See [architecture](DEVELOPMENT.md).

Tests use synthetic protocol fixtures and random malformed packets. They do not establish real-world recall, accuracy, or absence of false positives. Field comparisons with known emitting Axon, ALPR, glasses, tag, Flipper, and Biscuit hardware remain necessary.
