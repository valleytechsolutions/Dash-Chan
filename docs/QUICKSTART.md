# Your first adventure

Dash-Chan turns the official M5Stack StackChan K151 into an offline companion for passive radio awareness and GPS wardriving. It has scripted personality, original animated avatars, head motion, a touch sensor, and local speech. No account or cloud AI is required.

## Set up

1. [Install with the web flasher](https://valleytech-dash-chan.pages.dev/#install), or follow [manual flashing and recovery](FLASHING.md). Use the official K151, not a DIY servo Stack-chan.
2. Insert a FAT32 microSD card. Connect your M5Stack Unit GPS to **blue Port C**. GPS uses receive-only UART2 on G18; servos use their own UART. For other modules, see the [compatibility table and wiring guide](GPS.md).
3. Open **MENU → GPS & SD → GPS SETUP**. Select your module preset and actual port. Tap **APPLY SETTINGS**, or **APPLY + FIND BAUD** if its speed is unknown. The finder saves a rate after two valid NMEA sentences, or restores the applied rate after cancellation/timeout. Stop recording before changing GPS settings.
4. Go outside with a clear view of the sky. **GPS READY** means a fresh fix, valid UTC, four or more satellites, and HDOP no greater than 5. Acquiring satellites alone is not enough.
5. Choose **Buddy + Look** for your buddy and theme. Under **Personality**, enable gentle or lively head motion if desired. Motion is off on a new installation; existing preferences persist.
6. Under **Sound & Voice**, choose your volume and alert tone. **Voice starts OFF**. Tap it for **ALERTS** (spoken detection announcements) or **ALL** (also pet phrases) if you want speech; **TEST VOICE** plays a sample. **Master sound OFF** silences both speech and tones immediately. Your pet keeps animating and collecting signals when muted.

## The face is home

The pet remains visible during alerts and recording. No event automatically opens a different page. An alert replaces the FOOD/LOVE/XP row with a card in the device's tier colour: its name, signal strength, tier and confidence. The face reacts, and the speech bubble shows its personality.

- **START DRIVE:** creates a new SD file and begins waiting for usable GPS observations.
- **STOP & SAVE:** flushes and closes the file. Listening continues.
- **Tap the face:** affection, heart eyes, and bond. Each pet gets a different reaction; rapid petting turns into giggles and then a request for a breather. A napping buddy wakes up when touched.
- **Stroke/hold the top sensor:** visual affection. Head-sensor events never trigger speech. The sensor must settle released for 600 ms, then hold a meaningful touch for 250 ms; a held or noisy sensor cannot repeatedly fire.
- **Tap the alert:** dismiss the reaction. Listening and logging continue. **DETAILS** on the alert card opens the device's card instead.
- **FACE / RECON:** switch between a screen-filling expression and the pet beside the RECON panel.
- **RECON panel:** a radar and up to four device rows while watching; the top two rows and the drive counters while recording. Tap a row for that device, or anywhere else on the panel for the Recon page. In Full Face, tap the **RECON n** chip.
- **MENU:** opens the eight main controls. BACK returns through the menu to the face.

## Settings map

| Menu | Controls |
|---|---|
| Buddy + Look | Mochi, Sprout, Orbit, Wisp, Ember, or a custom buddy; Dark, Light, Matrix, BWM; Avatar Studio with a live preview, style packs, eyes, layered outfits, extras, and Auto/Always/Off field gear |
| Sound & Voice | Personality, Siren, Arcade, Chime, Silent; volume 0/100/170/220/255; Master sound; Voice OFF / ALERTS / ALL; tone preview; TEST VOICE; synthetic Axon demo |
| Personality | Head motion Off/Gentle/Lively; Spicy panic/Gentle lookout; pet button |
| Detection | Confidence filter; same-family quiet period 60/180/300 seconds; optional five-minute reminders; Quiet road ON/OFF; Auto snooze OFF/5/10; device types (whole tiers or single families); ignored-device count with CLEAR ALL (tap twice) |
| Recon List | **LIVE**: devices in range, most important first. **SEEN**: everything listed, newest first, including rows restored from the SD card. **STATS**: live/listed per tier, devices ever seen per family, following and ignored counts, ledger state. Five rows per page; tap a row for the device card |
| Display + Fun | Recon/Face, four backgrounds, mood lights, tilt, head swipe, wave hello, auto brightness, NFC outfit cards |
| Upload drives | Latest 24 drive files, configured destinations, explicit confirmation, cancel, refresh; [setup guide](UPLOADS.md) |
| GPS & SD | Fix/satellites/HDOP, received NMEA characters, rows, queue drops, storage errors, module presets, port/RX, baud finder, wiring help, SD retry |

### Reading the Recon screens

- **Tier colours:** red surveillance (cameras, plate readers, body cameras, camera glasses, drones, watchlist hits), violet hacker gear (Flipper, Pineapple and friends), amber attack patterns, blue trackers, grey beacons.
- **A row:** name (the specific product when the signature names one, such as a body-camera model), signal in dBm, an up arrow when it is getting stronger and a down arrow when weaker, and `xN` when it has been met more than once. A dimmed row is out of range and shows how long ago it was heard; `earlier` means it came from the SD ledger and the clock is not set yet.
- **Device card:** full name and tier, model and advertised name, address and whether it is a fixed, random or rotating one, signal now and at its strongest, first and last heard, encounters, why it did or did not alert, where it was last heard if GPS had a fix, and a plain description of what that kind of device is.
- **IGNORE (IT'S MINE):** your own tag or dashcam stays listed, dimmed, and never alerts again. UNIGNORE reverses it. The list is saved on the robot.
- A rotating Bluetooth address changes every few minutes to a day, so the same tag can appear as several rows over time.
- Everything here is a classification from radio metadata. It is a hint about what is nearby, not proof of who owns it or what it is doing.

**Silent** is a detection-tone choice. **Voice OFF** disables every spoken clip; **ALERTS** speaks detection announcements with any tone except Silent; **ALL** adds pet and system phrases. **Master sound OFF** and volume 0 disable all audio. With Voice OFF, alerts use electronic sounds instead of speech. Spicy captions can remain on-screen independently of voice. The spoken clips are synthesized and have not been reviewed by ear yet.

## Dinner is earned

Only **new Wi-Fi BSSIDs successfully saved with valid GPS during a wardrive** count as discoveries. Each earns one XP; five earn +12 fuel. Every 25 XP adds one level; all looks remain unlocked from level 1. BLE, watch mode, head touches, face taps, and synthetic demos award no food.

Food progress resets on each new drive. A 2 KB approximate per-drive filter prevents repeated BSSIDs from earning food again in that session. On very large drives it can conservatively miss a new discovery. Food deduplication does not remove otherwise eligible CSV rows. Fuel drains slowly to a floor of 10 and is saved with experience.

## Your files

- `/wardrives/dash-00000001.csv` and subsequent numbers: WiGLE 1.4 CSV, ready for your own WiGLE upload.
- `/events/recon.csv`: detection ledger, separate from WiGLE. One line per device when it is first heard, when it comes back, and when it alerts. The most recent devices are shown again after a restart. An older `/events/detections.csv` is left alone.
- `/dashchan/avatar.cfg`: optional custom appearance, loaded at boot or SD retry.
- `/dashchan/upload.json`: optional private Wi-Fi/API setup; [upload guide](UPLOADS.md).
- `/dashchan/watchlist.csv`: optional [personal signature rules](DETECTIONS.md#personal-watchlist).

Tap STOP & SAVE before ejecting the card or powering off. A sudden power loss can still lose the last buffered writes or corrupt FAT metadata. Uploads are off until configured. Optional auto-send after Stop + Save requires an explicit setup choice. Keep real observation files and factory backups private when reporting issues.

## Custom buddy

Open [Avatar Studio](https://valleytech-dash-chan.pages.dev/#creator). Start from a style pack (Kawaii pastel, Cyberpunk neon, Goth, Vaporwave, Retro terminal) or mix a base shape, ten eye styles, three colors, headwear, glasses, neckwear, a face mark, a back item, a held item, an aura, and a persona. Names allow up to 16 ASCII letters, digits, spaces, hyphens, or underscores. Your browser keeps a local draft. Preview different moods and travel poses before sending it.

**USB:** install firmware **0.4 or newer**, disconnect the installer, leave Dash-Chan running normally, stop any recording, and select **Send to StackChan**. The uploader checks the running firmware, sends the profile, and waits for `AVATAR OK`. It does not reflash the robot or change mute settings. An older firmware receives a clear upgrade message before a profile it cannot read is sent.

**SD:** download `avatar.cfg`, create `/dashchan/`, copy the file there, and restart. That file takes priority on every boot; remove or update it if you later want a USB-uploaded appearance to persist. USB uploads are stored in NVS. The creator customizes the five original vector shapes; it is not an arbitrary photo/sprite uploader.

The original `DCAV1` files still work. A plain design is still exported as `DCAV2`, which firmware 0.3 and newer reads; a design that uses newer gear is exported as `DCAV3`, which needs current firmware:

```text
DCAV2 0 FFF3D2 FFAD73 1 101029 5 0 2 Signal scout
DCAV3 3 17141F B3123C F2E9FF 9 7 0 5 4 2 4 5 3 1 Nyx the 2nd
```

The [studio guide](AVATAR-STUDIO.md) documents every field and style. RGB colors are quantized to the LCD's RGB565 format. On-device, use **Buddy + Look → Avatar Studio**: the FACE, OUTFIT and EXTRAS pages cycle each slot beside a live preview, **Style** cycles the style packs, and **DONE** returns home.

**Field kit: Auto** shows the antenna and listening receiver while recording or moving. Fresh usable GPS speed must stay at least 5 km/h for three seconds to start cruising; speed at most 2 km/h for five seconds stops it. Invalid/stale GPS clears cruising immediately. **Always** keeps the kit visible; **Off** hides it. The illustrated receiver is decorative and does not process microphone audio or determine a signal's bearing.

## What a signal means

A match is evidence about a radio broadcast, not proof of police presence, recording, ownership, tracking intent, or an exact product model. Wi-Fi/BLE addresses and names are spoofable. The robot's looking-around motion is personality, not direction finding. See [coverage and confidence](DETECTIONS.md) before interpreting alerts.
