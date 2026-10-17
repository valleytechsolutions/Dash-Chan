# Architecture and verification

## Runtime

`radio.cpp` timestamps bounded packets into a 32-entry FreeRTOS queue. Each packet has at most 1024 captured bytes. Data frames are filtered to built-in address candidates and reduced to 24 header bytes. Callbacks never render, write SD, speak, or move servos. The main loop drains at most eight observations per pass and services GPS between them. Oversized/full-queue frames increment the drop counter; parsers reject malformed lengths.

Wi-Fi uses `WIFI_MODE_NULL`. US channels 1–11 are visited in the order **1,6,11,2,7,3,8,4,9,5,10**. Dwell is 140 ms while recording and 180 ms while watching; the following BLE passive window is 850/1100 ms respectively. BLE interval/window are 80/70 ms. This shortens the sweep compared with the alpha's 300 ms × 11 + 1600 ms schedule. It is an explicit coverage/latency tradeoff, not simultaneous reception or proof of an optimal dwell for every environment. Current channel policy is US; other regional channel plans need a code change.

Screen refresh is capped at 20 Hz. Idle motion uses a nonblocking two/three-pose sequence and the BSP spring controller, followed by variable rests. Relaxed travel stays within ±20° yaw and 14.5–31° pitch; panic cues within ±32° yaw and 15–36° pitch. Gentle motion halves excursions. The BSP preserves factory zero positions. Motion off disables servo torque/power; animation continues.

Speech is optional and defaults off. The global voice gate covers every WAV playback path. Master sound additionally gates all tones. Turning either relevant switch off stops current audio. Head-sensor affection is visual only and has a 250 ms hold / 600 ms release gate. Deliberate face-tap love speech is rate-limited to 60 seconds. Sensor debounce is also increased through the official BSP. No microphone processing is used.

## Storage and food

Recording has a monotonic session generation copied into each packet. Pre-start and previous-session packets cannot enter the new file. Rows require fresh valid UTC/position/altitude, at least four satellites, HDOP in (0,5], and queue latency ≤250 ms. Wi-Fi rows are only AP observations. `AccuracyMeters` is the HDOP × 5 m estimate. Reobservations are limited to 30 seconds in the bounded 256-contact cache; very dense-area eviction can permit earlier repeats.

Files flush every two seconds and on Stop. A failed row write stops recording and records an error message while watching continues. Sudden power loss is not transactional FAT storage. No-fix observations are skipped, not backfilled with a later position. Event CSV is separate from WiGLE. SD formatting is never automatic or exposed as a casual menu action.

Food is credited only after successful Wi-Fi row writes. A per-session 2 KB Bloom filter prevents duplicate BSSIDs from earning repeat food, independent of the contact cache. Five discoveries add 12 fuel; each adds XP. Petting only affects bond. Fuel/preferences persist in NVS.

## Web installer

`site/flasher.mjs` contains the tested installation policy. It validates the manifest, exact file names, offsets, sizes, sector-rounded bounds, SHA-256 downloads, chip/flash size, post-write MD5 values, and the before/after NVS digest. `eraseAll` is always false. There are no image slots for `0x9000–0xDFFF`. The packaged bootloader is DIO/80 MHz/16 MB.

The self-hosted esptool-js 0.7.0 bundle is unmodified. Its `HardReset` implementation omits asserting RTS; Dash-Chan uses its documented custom-reset mechanism with `D0|R1|W100|R0|W100`. This was verified on the actual K151 USB Serial/JTAG interface. Vendor tracing is disabled. UI logs contain progress and device information, not firmware/NVS byte dumps.

`site/app.mjs` handles connect/cancel/disconnect, backup downloads, progress, failure messages, unsupported browsers, and mutually exclusive installer/avatar serial access. An install button never bypasses the hardware checkbox or chip-size checks. Download hashes are integrity checks against the published manifest, not independent cryptographic signing.

Avatar Studio exports a bounded `DCAV2` text profile; `DCAV1` imports migrate bow/scarf/crown into independent layers. USB checks `STATUS` for `avatar_schema:2`, refuses older firmware and recording, validates the profile on both ends, and waits for `AVATAR OK`. It does not reset or flash. SD profiles override the saved custom appearance on boot. The web renderer mirrors the firmware's original primitive shapes and RGB565 quantization; LCD and browser antialiasing differ. Drafts stay in browser local storage. [Profile fields and style choices](AVATAR-STUDIO.md).

`include/travel.h` keeps GPS movement detection separate from scanning, food, and logging. A usable fix and speed no older than two seconds are required. Speed ≥5 km/h for three seconds enters cruising; ≤2 km/h for five seconds exits. Invalid, stale, nonfinite, negative, or implausible (>300 km/h) speeds clear cruising. This is animation state, not a claim of car detection. Auto/Always/Off controls visibility of the original antenna/receiver art. The home pet is clipped to its drawing area so outfits cannot cover status, gauges, or controls.

`include/gps_support.h` and `src/gps.cpp` provide validated presets/Grove receive pins, bounded checksum-first NMEA framing, talker normalization, and a nonblocking one-shot baud search. Each trial resets parser and travel state; timeout/cancel restores the saved rate. There are no GPS UART writes. Only known GGA/RMC talkers enter TinyGPSPlus. The separate `tools/test_gps.py` integration test compiles the resolved, pinned TinyGPSPlus source with a small host Arduino shim, verifying eight talkers, coordinates, altitude, satellite count, HDOP, UTC and speed, plus no-fix/corrupt input. GPS setup is locked during recording and Start is locked during search. Tests cover checksum errors, overlong/binary input, resynchronization, talker normalization, unsafe pins, preset bounds, two-sentence acquisition, timeout, cancellation, and timer wrap.

## Build, package, and test

```sh
python -m pip install platformio==6.2.0
pio run -e stackchan
python tools/test_gps.py
python tools/test.py
python tools/package.py
node --test tests/web_test.mjs tests/studio_test.mjs
```

Host tests use GNU compilers, `CXX`/`CC`, or `ziglang==0.13.0`. They cover WiGLE CSV escaping and 11-column round trips, GPS quality, session boundaries, duplicate-food protection, cooldown rollover, encounter absence/repeat/family rules, custom profiles/watchlists, probe/header separation from AP logs, traffic hints, head-touch debounce, motion travel, and 50,000 deterministic malformed-packet inputs. Random testing is smoke fuzzing, not a correctness proof.

Browser tests use the dev dependency in `package.json`:

```sh
npm install
npx playwright install chromium
python -m http.server 8765 --directory site
# In another terminal:
node tests/browser_test.mjs
```

`DASH_SITE_URL` selects a deployed site; `DASH_BROWSER` optionally supplies a local Chromium executable. Checks cover desktop and 320/390/768 px layouts, all images, layered profile downloads, draft reload, legacy import, rejected imports, keyboard affection, portrait export, invalid names, unsupported-browser messaging, and page/network errors. `web_test.mjs` additionally exercises chip/size refusal, unsafe manifests, corrupt downloads, failed written checks, changed calibration, avatar recording lockout, and old-firmware schema refusal. `studio_test.mjs` round-trips all 2,880 style combinations. Host tests exercise movement thresholds, stale speed, hysteresis, and timer rollover, plus both profile versions and their bounds.

`tools/package.py` writes the flash ZIP and `site/firmware/` from a built application, checks boot mode and protected ranges, and includes source/license notices. `tools/vendor_web.py` refreshes exact pinned browser dependencies and verifies npm tarball integrity before retaining bundle/notices. `tools/pack_logos.py` encodes the supplied artwork at LCD resolution; original PNGs remain in `assets/brands`. Voice regeneration is optional Windows SAPI synthesis in `tools/generate_voice.ps1`; it is not needed to build committed clips.

## USB diagnostics

Use `python tools/device.py --port YOUR_PORT STATUS`. The utility does not intentionally reset the board. Commands:

| Command | Result |
|---|---|
| `STATUS` | Version, board, memory, radio/SD/GPS, rows/drops, fuel/discoveries, sound/voice, recent/suppressed counts, avatar schema, GPS speed/movement, ESP-IDF reset reason |
| `GPS FIND` / `GPS CANCEL` | Start/cancel a bounded baud search; saved and active rate are in STATUS; search refuses during recording |
| `START` / `STOP` | Same recording operations as the face buttons |
| `CHECK LOG` | Reopen last drive and verify header/byte count |
| `SCREEN` | Current RGB888 framebuffer; use `--output path.png` |
| `TAP x y` | Execute a touch at a valid 320×240 coordinate |
| `AVATAR GET` / `AVATAR SET DCAV2 …` | Inspect/apply custom profile; legacy DCAV1 accepted; Set refuses during recording |
| `RESTART` | Software restart; refuses during recording |
| `RELOAD PROFILES` | Reload SD avatar/watchlist while stopped |
| `DEMO AXON` | Synthetic reaction, no real detection/log/food |
| `SOUND STATUS` | Speaker/amp diagnostics |
| `SOUND TEST` | Explicit diagnostic beep; intentionally bypasses normal mute for hardware testing |

The development-only `FORMAT SD expected_bytes` command erases the inserted card only after matching its reported capacity and while recording is stopped. It is not part of the web flasher or normal UI. Never send it without intentional authorization for that exact card.

## Measured evidence and remaining work

Hardware: board 27, ESP32-S3 revision 0.2, 16 MB flash, 8 MB PSRAM, 320×240 display. The user confirmed correct orientation, servo motion, and audible output. A private full factory backup was retained. GPS on blue Port C at 115200 has produced valid 6–8 satellite fixes during beta checks. The installed card reports 511,868,665,856 bytes and mounts as FAT32; that does not verify its entire advertised capacity.

A 0.2 beta bench drive saved 77 rows, credited 13 new Wi-Fi discoveries, and raised fuel from 70 to 93 (including ongoing passive fuel decay). After Stop, the 8,196-byte file reopened with the expected WiGLE header. Queue drops were zero. Watching afterward did not add food. A prior no-fix session wrote only its header. Heap was approximately 70–79 KB with active radios; PSRAM supplies display buffers. The 0.3 outfit and travel-state changes do not alter the radio or CSV pipeline.

The web flashing module and pinned esptool-js were exercised against COM60 through a local Web Serial-compatible development bridge: four image verification checks, unchanged NVS, explicit reset, and an acknowledged custom-avatar transfer passed. This tests the actual serial/flashing code on hardware; the browser's native port chooser itself was not automated. Headless browser UI checks and protocol failure tests complement this, not replace it.

The user's repeated-love-speech report led to a global voice switch, silent-by-default speech, visual-only head affection, stronger touch gating, and one-minute face-speech spacing. The flashed device confirmed `sound:false` and `voice:false`; those user preferences are preserved.

For 0.3, a six-field styled profile was sent through the actual web uploader module and acknowledged on the K151. `AVATAR GET` confirmed eyes, colors, and all three accessory layers. The running device supplied the studio and field-kit screenshots. Three commanded software restarts returned fresh boot/status output with reset reason 3 and retained both mute settings. These checks are not cold-power-cycle or road tests.

A truncated panic message appeared during an ad hoc development USB control-line reset sequence. It did not recur in the three software-restart captures or subsequent standard esptool restarts, and no runtime panic was observed during the avatar checks. Its exact cause was not established; use the documented reset/recovery procedure and include complete boot output if reporting a recurrence.

For 0.4, the connected Unit GPS v1.1 was acquired by the baud finder at 115200 without needing a satellite fix. Start refused during search, cancellation restored the saved speed, and search refused during recording. A subsequent usable-fix bench session saved **38 rows / 12 new Wi-Fi discoveries** and reopened with a valid WiGLE header (**4,117 bytes**). The full interactive session including framebuffer captures accumulated seven queue drops; this was not a zero-drop field run. A separate roughly 45-second no-fix session saved only its **236-byte header**, with zero queue drops and no invented positions. Serial test clients must allow enough time for SD flush/reply completion; short development polling windows were not used as proof of missing data.

The actual web uploader module acknowledged Wisp and Ember profiles on the K151, then restored the previous custom appearance. A software restart retained GPS profile/port/baud, theme and both mute switches. Native screenshots cover the new buddies, model settings and wiring help. USB TX buffering was increased to hold a complete expanded STATUS response when host reads lag. Automated tests additionally decode synthetic fixes through the pinned TinyGPSPlus library for all eight supported GNSS talkers, and round-trip 2,880 style combinations. Other GPS models, custom wiring and alternate ports have **not** been physically tested; see the [manufacturer-based compatibility table](GPS.md).

Still unverified: actual WiGLE server upload acceptance, long outdoor-drive stability and GPS loss/reacquisition under movement, full/removed/failing SD behavior across cards, speaker clarity in the enclosure at each volume, and detection recall/false positives against known real devices. Do not turn a successful build or synthetic fixture into a claim that these field checks passed.
