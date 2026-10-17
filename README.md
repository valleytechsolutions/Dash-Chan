# Dash-Chan

**A tiny copilot for the invisible world.** Passive wardriving, device-signature alerts, and a pocket companion with a personality, built for the M5Stack StackChan K151.

![Mochi the cat, Sprout the dragon, and Orbit the robot](media/companions.svg)

**0.1.0 alpha — builds and host tests pass; installed on an official StackChan.** USB diagnostics confirm board detection, 8 MB PSRAM, live Wi-Fi/BLE reception, and SD mounting. Outdoor GPS, actual WiGLE upload, and field detection accuracy remain unverified. The GPS model and wiring must match the selected profile.

## Meet your copilot

- **Mochi:** a curious cream-colored cat with a serious snack habit.
- **Sprout:** a mint dragon who collects signals for a tiny treasure hoard.
- **Orbit:** a lilac robot whose strongest signal is friendship.

Tap their face or the head touch sensor for scritches. Choose a buddy, build bond and experience, and watch blinking, expressions, chatter, and bounded head movements. **Wardriving is the only way to feed your buddy:** every five new Wi-Fi BSSIDs saved with a valid GPS fix earns a snack (+12 fuel). Each discovery also earns experience. Background watch, BLE advertisements, petting, and demo alerts never feed it. Fuel is saved periodically and when stopping a drive; a new installation starts with 85 fuel. Personality is local and scripted; no AI service, account, microphone, or internet is required.

Food progress resets for each new drive. Repeated BSSIDs in that drive do not count again, even after the encounter cache expires. A bounded 2 KB Bloom filter can conservatively miss some new discoveries on large drives; it never awards extra food for a repeat. The food filter does not affect CSV logging.

## Two jobs, one buddy

**Keep watch:** passive Wi-Fi and BLE listening starts at boot and continues on every screen. Dragon Detector's passive classifiers identify advertised signatures associated with Axon/body cameras, Flock, ALPR vendors, camera glasses, Find My/DULT/SmartTag/Tile/Find Hub trackers, Flipper, Biscuit, WiFi Pineapple, Pwnagotchi, Nyanbox, HaleHound, camera vendors, and other supported categories.

**Go wardriving:** tap **START WARDRIVE** on the face screen. GPS-tagged Wi-Fi access points and BLE advertisements are written as WiGLE 1.4 CSV to the SD card. **STOP & SAVE** closes the session and returns to background watch. No upload happens automatically.

Axon alerts can speak **“PIG DETECTED! ACT NATURAL!”** or **“Oh man, the fuzz!”**, play a short siren, and trigger a startled expression and head reaction. Choose gentle banter, quiet audio, or motion off in settings. **TRY AXON REACTION** is explicitly a demo and never writes a detection.

## Controls

| Control | Action |
|---|---|
| START WARDRIVE / STOP & SAVE | Open or close a new SD session |
| Tap face / touch head | Pet your buddy |
| BUDDY | Pick an avatar, pet, or FIND FOOD by starting a wardrive |
| MENU | Sound, motion, banter, confidence threshold, history, diagnostics |
| Tap title/status bar | Recent detection history |
| Tap an alert | Dismiss without stopping radio collection |
| GPS / SD STATUS | GPS fix, satellites, NMEA bytes, SD status, radio drops, GPS port/baud, SD retry |

Head movement defaults off for the first installation. Place the robot securely and enable **Motion** once its mechanical travel and factory calibration are checked. Reactions stay within approximately ±22° yaw and 13–30° pitch, with no continuous spinning. The BSP uses stored factory zero positions when available.

## Hardware and build

- Target: **official M5Stack StackChan K151/CoreS3**, ESP32-S3, 16 MB flash and 8 MB Quad PSRAM; not a generic DIY SG90 Stack-chan.
- Native radio: **2.4 GHz Wi-Fi + BLE only**. No 5 GHz or Bluetooth Classic on this chip.
- GPS: NMEA UART, default **Port C, RX G18 / TX G17, 9600 baud**, separate UART2; Port A and 38400/115200 are selectable. Stacked GPS modules may require different routing. [Hardware notes](docs/HARDWARE.md).
- FAT32 microSD, installed in the StackChan slot.

```sh
python -m pip install platformio==6.2.0
pio run -e stackchan
# Only after identifying your board and backing up its flash:
pio run -e stackchan -t upload --upload-port YOUR_PORT
pio device monitor --port YOUR_PORT --baud 115200
```

[Installation and recovery](docs/HARDWARE.md) · [Detection limits](docs/DETECTIONS.md) · [Architecture and verification](docs/DEVELOPMENT.md) · [Sources and credits](docs/SOURCES.md)

## Logs

- `/wardrives/dash-00000001.csv`: standard 11-column WiGLE CSV, one new file per Start. Reobservations are logged at most once per address/transport every 30 seconds in the bounded encounter cache.
- `/events/detections.csv`: alert evidence, confidence, received signal level, and timestamp. This is a separate event log, not a WiGLE upload file.
- Valid UTC, at least four satellites, HDOP ≤ 5, and fresh position/altitude are required for wardrive rows. Recording waits or pauses through fix loss; it never substitutes a cached location or invents coordinates. `AccuracyMeters` is an estimate of HDOP × 5 m, not measured accuracy.
- Files flush every two seconds and when stopped. Always use **STOP & SAVE** before power-off or removing the card; sudden power loss can still lose buffered data or damage FAT metadata.

## What an alert means

It means a matching **radio signature was received**, not proof of a person's identity, device model, recording state, or intent. Names and addresses can be spoofed. “AirTag” signatures often identify the broader Find My network. “Biscuit” cannot passively distinguish Pro from Ultra. Motorola/Genetec prefixes do not prove an ALPR. Medium-or-higher evidence triggers alerts by default; low-confidence clues are available in settings. Silent, out-of-range, unsupported-band, or non-advertising equipment can be missed.

The single radio alternates Wi-Fi channel listening and BLE windows, so background watch is continuous as a feature, not simultaneous reception on all bands. Looking around is personality animation, not direction finding.

## Development

```sh
python -m pip install ziglang==0.13.0
python tools/test.py
```

The host suite checks protocol fixtures, malformed packets, UTC/GPS gates, session boundaries, cooldown rollover, and CSV round-trip compatibility. GitHub Actions builds firmware and runs these checks. Actual WiGLE server acceptance and field detection recall remain unverified.

GPL-3.0-or-later for Dash-Chan; imported components retain their original notices. This project credits **Dragon Detector**, **SquachWatch**, **JustCallMeKoko's ESP32 Dual Band Wardriver**, **Midwest Gadgets' Piglet**, and **M5Stack**. It is independently maintained, with no endorsement implied. Piglet is a behavior reference; its noncommercial source is not bundled.
