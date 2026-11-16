# Dash-Chan handbook

Welcome to your tiny road buddy: passive Wi-Fi/BLE awareness, GPS wardriving, SD logs, and a scripted companion for the official M5Stack StackChan K151.

**[Open the web flasher and Avatar Studio](https://valleytech-dash-chan.pages.dev/)**

- [First adventure and every setting](https://github.com/valleytechsolutions/Dash-Chan/blob/main/docs/QUICKSTART.md)
- [Uploads: WiGLE, WDGWars, USB setup, and receipts](https://github.com/valleytechsolutions/Dash-Chan/blob/main/docs/UPLOADS.md)
- [Views, levels, sensor play, and NFC outfit cards](https://github.com/valleytechsolutions/Dash-Chan/blob/main/docs/PLAY.md)
- [Avatar Studio: eyes, outfits, and field gear](https://github.com/valleytechsolutions/Dash-Chan/blob/main/docs/AVATAR-STUDIO.md)
- [Flash, update, factory backup, and recovery](https://github.com/valleytechsolutions/Dash-Chan/blob/main/docs/FLASHING.md)
- [Detection methods, confidence, quiet encounters, and watchlists](https://github.com/valleytechsolutions/Dash-Chan/blob/main/docs/DETECTIONS.md)
- [GPS presets, baud finder, and ATGM336H wiring](https://github.com/valleytechsolutions/Dash-Chan/blob/main/docs/GPS.md)
- [Hardware connections and GPS](https://github.com/valleytechsolutions/Dash-Chan/blob/main/docs/HARDWARE.md)
- [Architecture, serial commands, and measured validation](https://github.com/valleytechsolutions/Dash-Chan/blob/main/docs/DEVELOPMENT.md)
- [Pinned sources and attributions](https://github.com/valleytechsolutions/Dash-Chan/blob/main/docs/SOURCES.md)
- [Release downloads](https://github.com/valleytechsolutions/Dash-Chan/releases)

## The short version

Install, connect GPS to blue Port C, insert FAT32 microSD, and wait for GPS READY. Tap START DRIVE on the pet. Five new Wi-Fi networks successfully logged with a usable fix earn one snack. Tap STOP & SAVE before removing the card.

Tap the face or touch the head for affection. Head touches are visual only. Speech starts off; set **MENU → Sound & Voice → Voice** to ALERTS or ALL if desired. **Master sound OFF** silences everything. You can keep tones with speech off.

Before a drive starts, the home screen is a recon tool: the RECON panel beside the pet lists what Dash-Chan has classified nearby (plate readers, body cameras, camera glasses, Ring cameras, trackers, Flipper-style gadgets, drones and more), colour coded by tier, with a small radar above. Tap a row for the device card or the panel for the full list, including devices seen earlier and on earlier boots. During a drive the pet and the drive counters take over and the two most important devices stay in view.

The pet stays visible during an alert. Signals are clues, not proof of police, recording, ownership, or intent. Native hardware covers 2.4 GHz Wi-Fi and BLE. The project's detailed pages distinguish implemented methods from unverified field claims.

This handbook is versioned with the firmware under `docs/wiki/` so it is available even when GitHub's separate Wiki service is unavailable. The linked guides are the maintained source of truth.
