# Your first adventure

Dash-Chan turns the official M5Stack StackChan K151 into an offline companion for passive radio awareness and GPS wardriving. It has scripted personality, original animated avatars, head motion, a touch sensor, and local speech. No account or cloud AI is required.

## Set up

1. [Install with the web flasher](https://valleytechsolutions.github.io/Dash-Chan/#install), or follow [manual flashing and recovery](FLASHING.md). Use the official K151, not a DIY servo Stack-chan.
2. Insert a FAT32 microSD card. Connect your M5Stack Unit GPS to **blue Port C**. GPS uses RX G18 / TX G17 on UART2; servos use their own UART.
3. Open **MENU → GPS & SD**. Select the GPS unit's actual baud: 9600, 38400, or 115200. The development unit is working at 115200. The default for new installations is 9600. Port and baud changes are locked during recording.
4. Go outside with a clear view of the sky. **GPS READY** means a fresh fix, valid UTC, four or more satellites, and HDOP no greater than 5. Acquiring satellites alone is not enough.
5. Choose **Look & Themes** for your buddy and theme. Under **Personality**, enable gentle or lively head motion if desired. Motion is off on a new installation; existing preferences persist.
6. Under **Sound & Voice**, choose your volume and alert tone. **Voice starts OFF**. Enable it only if you want speech. **Master sound OFF** silences both speech and tones immediately. Your pet keeps animating and collecting signals when muted.

## The face is home

The pet remains visible during alerts and recording. No event automatically opens a different page. A compact signal ribbon shows the family and confidence; the face reacts, and the speech bubble shows its personality.

- **START WARDRIVE:** creates a new SD file and begins waiting for usable GPS observations.
- **STOP & SAVE:** flushes and closes the file. Listening continues.
- **Tap the face:** affection, heart eyes, and bond. If Voice is on, deliberate screen pets may say “i Love Your Face,” at most once a minute.
- **Stroke/hold the top sensor:** visual affection. Head-sensor events never trigger speech. The sensor must settle released for 600 ms, then hold a meaningful touch for 250 ms; a held or noisy sensor cannot repeatedly fire.
- **Tap the alert bubble:** dismiss the reaction. Listening and logging continue.
- **MENU:** opens the six main controls. BACK returns through the menu to the face.

## Settings map

| Menu | Controls |
|---|---|
| Look & Themes | Mochi, Sprout, Orbit, or a custom buddy; Dark, Light, Matrix, BWM; Avatar Studio with eyes, layered outfits, and Auto/Always/Off field gear |
| Sound & Voice | Personality, Siren, Arcade, Chime, Silent; volume 0/100/170/220/255; Master sound; Voice; preview; synthetic Axon demo |
| Personality | Head motion Off/Gentle/Lively; Spicy panic/Gentle lookout; pet button |
| Detection | Confidence filter; same-family quiet period 60/180/300 seconds; optional five-minute reminders |
| Recent Signals | Up to 32 entries, four per page; transport, RSSI, name/address, evidence, count, last-seen age |
| GPS & SD | Fix/satellites/HDOP, received NMEA characters, rows, queue drops, storage errors, port/baud, SD retry |

**Silent** is a detection-tone choice. **Voice OFF** disables every spoken clip. **Master sound OFF** disables all audio. With Voice OFF, Personality uses electronic sounds instead of speech. Spicy captions can remain on-screen independently of voice.

## Dinner is earned

Only **new Wi-Fi BSSIDs successfully saved with valid GPS during a wardrive** count as discoveries. Each earns one XP; five earn +12 fuel. BLE, watch mode, head touches, face taps, and synthetic demos award no food.

Food progress resets on each new drive. A 2 KB approximate per-drive filter prevents repeated BSSIDs from earning food again in that session. On very large drives it can conservatively miss a new discovery. Food deduplication does not remove otherwise eligible CSV rows. Fuel drains slowly to a floor of 10 and is saved with experience.

## Your files

- `/wardrives/dash-00000001.csv` and subsequent numbers: WiGLE 1.4 CSV, ready for your own WiGLE upload.
- `/events/detections.csv`: emitted detection events, separate from WiGLE.
- `/dashchan/avatar.cfg`: optional custom appearance, loaded at boot or SD retry.
- `/dashchan/watchlist.csv`: optional [personal signature rules](DETECTIONS.md#personal-watchlist).

Tap STOP & SAVE before ejecting the card or powering off. A sudden power loss can still lose the last buffered writes or corrupt FAT metadata. Nothing uploads automatically. Keep real observation files and factory backups private when reporting issues.

## Custom buddy

Open [Avatar Studio](https://valleytechsolutions.github.io/Dash-Chan/#creator). Choose a preset or mix a base shape, six eye styles, three colors, headwear, glasses, and neckwear. Names allow up to 16 ASCII letters, digits, spaces, hyphens, or underscores. Your browser keeps a local draft. Preview different moods and travel poses before sending it.

**USB:** install firmware **0.3 or newer**, disconnect the installer, leave Dash-Chan running normally, stop any recording, and select **Send to StackChan**. The uploader checks the running firmware, sends the profile, and waits for `AVATAR OK`. It does not reflash the robot or change mute settings. An older firmware receives a clear upgrade message before a new profile is sent.

**SD:** download `avatar.cfg`, create `/dashchan/`, copy the file there, and restart. That file takes priority on every boot; remove or update it if you later want a USB-uploaded appearance to persist. USB uploads are stored in NVS. The creator customizes the three original vector shapes; it is not an arbitrary photo/sprite uploader.

The original `DCAV1` files still work. New exports use `DCAV2`:

```text
DCAV2 0 FFF3D2 FFAD73 1 101029 5 0 2 Signal scout
```

The [studio guide](AVATAR-STUDIO.md) documents every field and style. RGB colors are quantized to the LCD's RGB565 format. On-device, use **Look & Themes → Avatar Studio** to cycle eyes and accessories, then **Show my buddy** to return home.

**Field kit: Auto** shows the antenna and listening receiver while recording or moving. Fresh usable GPS speed must stay at least 5 km/h for three seconds to start cruising; speed at most 2 km/h for five seconds stops it. Invalid/stale GPS clears cruising immediately. **Always** keeps the kit visible; **Off** hides it. The illustrated receiver is decorative and does not process microphone audio or determine a signal's bearing.

## What a signal means

A match is evidence about a radio broadcast, not proof of police presence, recording, ownership, tracking intent, or an exact product model. Wi-Fi/BLE addresses and names are spoofable. The robot's looking-around motion is personality, not direction finding. See [coverage and confidence](DETECTIONS.md) before interpreting alerts.
