# Architecture and verification

## Runtime

`radio.cpp` timestamps bounded packets into a 32-entry FreeRTOS queue. Queue storage prefers PSRAM with an internal-RAM fallback. Each packet has at most 1024 captured bytes. Data frames are filtered to built-in address candidates and reduced to 24 header bytes. Callbacks never render, write SD, speak, or move servos. The main loop drains at most eight observations per pass and services GPS between them. Oversized/full-queue frames increment the drop counter; parsers reject malformed lengths.

Wi-Fi uses `WIFI_MODE_NULL`. US channels 1–11 are visited in the order **1,6,11,2,7,3,8,4,9,5,10**. Dwell is 140 ms while recording and 180 ms while watching; the following BLE passive window is 850/1100 ms respectively. BLE interval/window are 80/70 ms. This shortens the sweep compared with the alpha's 300 ms × 11 + 1600 ms schedule. It is an explicit coverage/latency tradeoff, not simultaneous reception or proof of an optimal dwell for every environment. Current channel policy is US; other regional channel plans need a code change.

Screen refresh is capped at 20 Hz. Original primitive arcade habitats, particles and expressive poses render in the buffered canvas without image decoding or per-frame heap allocation. Scrolling uses the existing validated GPS movement flag. A discovery timestamp starts a short cheer only after a new Wi-Fi row was saved; active alerts override it. Idle motion uses a nonblocking two/three-pose sequence and the BSP spring controller, followed by variable rests. Relaxed travel stays within ±20° yaw and 14.5–31° pitch; panic cues within ±32° yaw and 15–36° pitch. Gentle motion halves excursions. The BSP preserves factory zero positions. Motion off disables servo torque/power; animation continues.

Speech is optional and defaults off. The Voice setting has three modes: OFF, ALERTS (one short announcement per detection family) and ALL (also pet and system phrases such as drive start/saved, GPS locked/lost, level up, hungry, tickles and greetings). It is stored under the NVS key `voicemode`; an older saved `voice` switch migrates as on = ALL, and the old key is still written for downgrades. Every clip goes through one function, `voiceSpeak(Clip, SpeechKind)` in `src/buddy.cpp`, and the rules it applies are pure code in `include/voice.h`: `speechEnabled` (master sound, volume 0 and the Silent tone mute speech; the mode picks which kinds speak) and `SpeechGate` (4 s between any two clips, 20 s between pet phrases, 5 minutes before the same pet phrase again; a detection cuts a pet phrase short but never another detection). Speech works with every alert tone except Silent: a two-note attention chirp on speaker channel 1 is followed by the clip on channel 0 in place of the long tone sequence, so tones and speech never overlap, and the Spicy wee-woo waits for the clip to end. When an alert cannot speak (rate limit, Voice OFF) the usual tone plays. Whenever a clip is spoken the screen shows that clip's caption (`ClipInfos`), so the text matches the voice; otherwise the personality deck picks the line as before. Spicy banter keeps its two recorded body-camera lines, and the recorded love line still plays only when the pet picks that exact line. `Clip::Following` ("We're being followed!") and `Clip::Flood` ("Signal flood!") are recorded and reachable through `alertClip(family, spicyLine, AlertTwist)`, but no detector raises those twists yet, so today they are only heard through TEST VOICE. Host tests in `tests/voice_tests.h` cover the family-to-clip map, agreement between the manifest, the `Clip` enum and the captions, the setting migration, the gates and rate limits, the GPS-news debounce and the Sound & Voice tap map (`include/audio_view.h`, also rendered by `tools/preview.py` as `build/preview/audio.png`). Turning Master sound or Voice off stops current audio. Head-sensor affection is visual only and has a 250 ms hold / 600 ms release gate. Sensor debounce is also increased through the official BSP. No microphone processing is used.

Clips are mono 16 kHz 8-bit unsigned PCM WAV files with a canonical 44-byte header in `assets/voice/`, listed in `tools/voice_phrases.tsv` (id, spoken text with phonetic spellings, on-screen caption) and compiled from the generated `src/voice_data.cpp`. M5Unified 0.2.26 `Speaker_Class::playWav` (`src/utility/Speaker_Class.inl`) accepts PCM with 8 to 16 bits per sample, one or two channels and any sample rate, and plays 8-bit data as unsigned; this was read in the source, not yet confirmed on the speaker. `tools/generate_voice.py` (Linux) synthesizes each phrase with piper-tts and the `en_US-ljspeech-high` voice, then trims, raises the pitch about five semitones, equalizes and resamples with ffmpeg; `--asr` keeps a take only when two speech recognisers both read it back, and `--check-asr` reports how the committed clips transcribe. `tools/pack_voice.py` writes the source file, and `--check` (run by `tools/test.py`) verifies every clip's format, the length cap (1.6 s, aiming for 1.5 s; 3 s for the three older recordings), the 900 KB total and that the generated source matches the WAV files. The engine is not deterministic, so the WAV files are committed. **No person has listened to these clips yet**: they were checked only by speech recognition, which is a sanity check and not a listening test. As committed, 40 of the 44 clips are transcribed word for word by both recognisers used to pick takes (faster-whisper small.en 42, base.en 40); two of those passes rest on the manifest's "heard as" spellings ("Camera glass is spotted", "Flipper 0 spotted"). The misses are "Biscuit device spotted!" (one recogniser), "Pwnagotchi spotted!" (a name neither can spell) and the two older Spicy recordings, whose "Act natural" is not recognised in the original recordings either. Takes were chosen with those same recognisers, so that figure flatters the clips: two recognisers that took no part in the choice scored 38/44 (medium.en) and 30/44 (tiny.en). Three clips run slightly over the 1.5 s target (1.50 to 1.56 s). Wording matters more than processing: phrases ending in "nearby" came back as "bye" after the pitch shift, "spotted" did not, and a -45 dB silence trim was cutting soft first consonants. The Sound & Voice page's TEST VOICE button steps through every clip for that purpose. Engine, voice model and licences: [voice notice](licenses/VOICE-NOTICE.txt).

`include/personality.h` is the hardware-free personality engine. `feelingFor` derives one of seven feelings from fuel, bond, time since the last interaction, and whether the pet is recording or moving. `Banter` is a per-topic shuffle bag over one flat `Lines[]` table filtered by buddy and the Spicy setting: each eligible line is used once before the bag refills, and a refill never starts with the line just spoken. `petTopic`, `chatterTopic`, `chatterGapMs` and `decayNeeds` hold the remaining rules; `buddy.cpp` only supplies state and timers. Unprompted chatter waits 12 seconds after any other message. Network-name remarks quote at most 16 sanitized characters on the local screen only. Bond and lifetime counters are stored in NVS with the settings. `utcSeconds`, `awayHours`, `applyAway` and `awayLine` implement time apart: GPS UTC (or the board RTC, which is written once from GPS and trusted only when its year is plausible) is compared with the last time stored in NVS, capped at 60 days. Host tests check that every line is printable ASCII within 47 characters and wraps losslessly into the speech bubble, every topic has 3–32 lines for every buddy, bags never repeat early, and each rule's thresholds.

`include/home_view.h` draws the whole home screen from a plain `HomeView` snapshot and `include/avatars.h` draws the buddies; both are templates over the canvas type. The firmware instantiates them with `M5Canvas`, and `tools/preview.py` instantiates them with a small software canvas (`tools/host_canvas.h`) to write PNGs of real screens to `build/preview/` (`--site` also refreshes the home-screen images in `site/assets/`). The preview needs Pillow and the display font that `pio run` downloads; its rasterizer is close to, not identical with, the LCD driver. Buddy outlines come from drawing each silhouette twice through `art::Pen` (once 2 px fatter in the outline color); Full Face and the buddy picker redraw the same shapes through `art::Zoom` instead of scaling a bitmap. `tools/web_art.py` converts the two art functions between the `web-art` markers into `site/buddy_art.mjs` for Avatar Studio; `tools/test.py` fails if that file is stale.

## Storage and food

Recording has a monotonic session generation copied into each packet. Pre-start and previous-session packets cannot enter the new file. Rows require fresh valid UTC/position/altitude, at least four satellites, HDOP in (0,5], and queue latency ≤250 ms. Wi-Fi rows are only AP observations. `AccuracyMeters` is the HDOP × 5 m estimate. Reobservations are limited to 30 seconds in the bounded 256-contact cache; very dense-area eviction can permit earlier repeats.

Files flush every two seconds and on Stop. A failed row write stops recording and records an error message while watching continues. Sudden power loss is not transactional FAT storage. No-fix observations are skipped, not backfilled with a later position. Event CSV is separate from WiGLE. SD formatting is never automatic or exposed as a casual menu action.

Food is credited only after successful Wi-Fi row writes. A per-session 2 KB Bloom filter prevents duplicate BSSIDs from earning repeat food, independent of the contact cache. Five discoveries add 12 fuel; each adds XP. Petting only affects bond. Fuel/preferences persist in NVS.

## Recon table, alert policy and ledger

`include/recon.h` is the hardware-free recon engine; `tests/recon_tests.h` exercises all of it on a PC. `radio.cpp` owns one `ReconTable` (256 rows of about 200 bytes in PSRAM, 64 rows of internal RAM as a fallback), one `AlertPolicy`, and exposes them through `reconTable()`, `reconPolicy()`, `reconCounts()`, `reconContext()`, `reconIgnore()` and `reconRefresh()` in `app.h`. Everything runs on the loop task.

- **Listing** never depends on the alert threshold. `process()` passes every packet of a classified contact to `ReconTable::observe()` unless the family is switched off in the type filter (iBeacons are off by default). A row is keyed by MAC, transport, BLE address type and family. `encounters` grows only after 120 s of silence; `packets` counts frames. `reconLive()` means heard within 60 s; `reconTrend()` compares the latest RSSI with a sample about 2 s old (4 dB either way).
- **Eviction** takes restored rows first, then rows out of range, then the lowest tier, then the longest unheard. A live row is never given up for a newcomer of a lower tier.
- **Views**: `sorted()` fills a caller-provided index array without heap. `ReconSort::Priority` is live first, then tier, then most recent, where "most recent" for a live row is the start of its current encounter so rows do not reorder with every packet. `ReconSort::Recent` is strictly last-heard. `app.history` is the first 32 rows in priority order, copied twice a second.
- **Alerts** go through `AlertPolicy::decide()`, which returns `Alert`, `Flood`, or a named quiet reason (`verdictName()`), also stored in `row.verdict`. Gates in order: type filter, ignore list, tracker flood gate, alert threshold, quiet road, same encounter, auto snooze, family quiet, global quiet. A strictly higher tier bypasses the ten-second global quiet.
- **Follow detection** keeps up to four GPS places per tracker-tier row, each more than 500 m from the others. Three places and more than ten minutes between the first place and the latest fix set `row.following`; `followDue()` hands out one alert per device. Identity is the advertised address: rows are not folded by payload, because in Find My, SmartTag and Find Hub adverts the payload rotates with the address.
- **Rollover**: all timers use unsigned subtraction; `maintain()` saturates ages at 2^30 ms so a device heard once cannot look fresh when `millis()` wraps.
- **NVS** (namespace `dashchan`): `typeoff` (families switched off, so new families default on), `quietroad`, `autosnooze` (0/5/10), `ignores` (blob of 7-byte records: MAC + family, 64 at most), `seenfam` (blob of little-endian 32-bit lifetime counters, one per family, written at most once a minute).

### `/events/recon.csv`

Append-only CSV, one header line, written by `storage.cpp` in batches (every 5 s, or within 1 s of an alert; never while an upload is busy; rows wait in RAM while there is no card). A line is added when a device is first heard, when it returns after 120 s of silence, and when it alerts. An alert line repeats the boot and uptime of the encounter it belongs to, so readers can tell it from a new encounter. Addresses created during a tracker flood are not written.

| Column | Meaning |
| --- | --- |
| `UTC` | `YYYY-MM-DD HH:MM:SS` of the last sighting, empty while the time is unknown |
| `UptimeMs` | `millis()` at the start of the encounter |
| `Boot` | Boot counter (`boots` in NVS), so uptimes from different power cycles are not confused |
| `Family` | Stable token from `familyKey()`: `axon`, `flock`, `alpr`, `meta`, `tracker`, `flipper`, `biscuit`, `pineapple`, `pwnagotchi`, `nyanbox`, `halehound`, `skimmer`, `raven`, `drone`, `camera`, `beacon`, `deauth`, `eviltwin`, `blespam`, `watchlist`, `smarttag`, `googletag`, `tile`, `ring` |
| `Confidence` | `low`, `medium` or `high` |
| `Transport` | `WIFI`, `BLE` (public address) or `BLE1`..`BLE3` (NimBLE address type: random, public identity, random identity) |
| `MAC` | `AA:BB:CC:DD:EE:FF` |
| `Name` | Quoted advertised name or SSID; quotes doubled, unprintable bytes written as `?` |
| `Label` | Quoted engine tag: empty, `following` or `flood` |
| `RSSI` | dBm at write time |
| `Channel` | Wi-Fi channel, `0` for BLE |
| `Lat`, `Lon` | Latest usable GPS position during this encounter (six decimals), both empty if none |
| `Alerted` | `1` once the device has raised an alert, else `0` |

At the first successful card mount of a boot the last 16 KB of the file are read, lines are parsed oldest first with `ledgerParse()`, and the newest 64 distinct devices become `restored` rows: listed after everything from this boot, never live, first to be evicted. Hearing one again turns it into a normal row and counts as a return. Torn lines, the header and unknown family tokens are skipped. `/events/detections.csv` written by firmware up to 0.5.0-beta (`UTC,UptimeMs,Family,Confidence,MAC,Name,RSSI,Evidence`, emitted alerts only) is neither read nor modified.

## Web installer

`site/flasher.mjs` contains the tested installation policy. It validates the manifest, exact file names, offsets, sizes, sector-rounded bounds, SHA-256 downloads, chip/flash size, post-write MD5 values, and the before/after NVS digest. `eraseAll` is always false. There are no image slots for `0x9000–0xDFFF`. The packaged bootloader is DIO/80 MHz/16 MB.

The self-hosted esptool-js 0.7.0 bundle is unmodified. Its `HardReset` implementation omits asserting RTS; Dash-Chan uses its documented custom-reset mechanism with `D0|R1|W100|R0|W100`. This was verified on the actual K151 USB Serial/JTAG interface. Vendor tracing is disabled. UI logs contain progress and device information, not firmware/NVS byte dumps.

`site/app.mjs` handles connect/cancel/disconnect, backup downloads, progress, failure messages, unsupported browsers, and mutually exclusive installer/avatar serial access. An install button never bypasses the hardware checkbox or chip-size checks. Download hashes are integrity checks against the published manifest, not independent cryptographic signing.

Avatar Studio exports a bounded `DCAV2` text profile; `DCAV1` imports migrate bow/scarf/crown into independent layers. USB checks `STATUS` for `avatar_schema:2`, refuses older firmware and recording, validates the profile on both ends, and waits for `AVATAR OK`. It does not reset or flash. SD profiles override the saved custom appearance on boot. The web renderer uses buddy shapes generated from the firmware header and RGB565 quantization; LCD and browser antialiasing differ. Drafts stay in browser local storage. [Profile fields and style choices](AVATAR-STUDIO.md).

`include/travel.h` keeps GPS movement detection separate from scanning, food, and logging. A usable fix and speed no older than two seconds are required. Speed ≥5 km/h for three seconds enters cruising; ≤2 km/h for five seconds exits. Invalid, stale, nonfinite, negative, or implausible (>300 km/h) speeds clear cruising. This is animation state, not a claim of car detection. Auto/Always/Off controls visibility of the original antenna/receiver art. The home pet is clipped to its drawing area so outfits cannot cover status, gauges, or controls.

`include/gps_support.h` and `src/gps.cpp` provide validated presets/Grove receive pins, bounded checksum-first NMEA framing, talker normalization, and a nonblocking one-shot baud search. Each trial resets parser and travel state; timeout/cancel restores the saved rate. There are no GPS UART writes. Only known GGA/RMC talkers enter TinyGPSPlus. The separate `tools/test_gps.py` integration test compiles the resolved, pinned TinyGPSPlus source with a small host Arduino shim, verifying eight talkers, coordinates, altitude, satellite count, HDOP, UTC and speed, plus no-fix/corrupt input. GPS setup is locked during recording and Start is locked during search. Tests cover checksum errors, overlong/binary input, resynchronization, talker normalization, unsafe pins, preset bounds, two-sentence acquisition, timeout, cancellation, and timer wrap.

## Build, package, and test

```sh
python -m pip install platformio==6.2.0
pio run -e stackchan
python tools/test_gps.py
python tools/test.py
python tools/package.py
node --test tests/web_test.mjs tests/studio_test.mjs tests/uploads_test.mjs
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

`tools/package.py` writes the flash ZIP and `site/firmware/` from a built application, checks boot mode and protected ranges, and includes source/license notices. `tools/vendor_web.py` refreshes exact pinned browser dependencies and verifies npm tarball integrity before retaining bundle/notices. `tools/pack_logos.py` encodes the supplied artwork at LCD resolution; original PNGs remain in `assets/brands`. Voice regeneration is optional (`tools/generate_voice.py`, Linux, piper-tts and ffmpeg); it is not needed to build with the committed clips.

## Upload implementation

`src/uploads.cpp` uses a bounded worker task, CA-verified ESP-IDF HTTPS, fixed service destinations, streamed multipart requests, and per-file SHA-256 receipts. CSV parsing and 1.4-to-1.6 conversion are bounded and buffered. Larger TLS allocations prefer PSRAM; scanning pauses during uploads, but the NimBLE host stays initialized. Tearing that host down while its event loop was active caused a development panic; removing teardown fixed the reproduced path. Repeated unavailable-network and cancel tests now restore Wi-Fi/BLE without resetting. No blind network retries are used.

`SETUP BEGIN / DATA / END` transfers at most 2 KiB and verifies SD contents before acknowledgement, with a backup rename for interrupted replacement. Sensitive values are not printed. The web setup protocol retries only the read-only STATUS handshake because USB CDC may miss the first command after DTR; mutating chunks are never replayed automatically. Installer, avatar and setup operations share a UI lock. [Uploads and remaining service-test limits](UPLOADS.md).

## USB diagnostics

Use `python tools/device.py --port YOUR_PORT STATUS`. The utility does not intentionally reset the board. Commands:

| Command | Result |
|---|---|
| `STATUS` | Version, board, memory, radio/SD/GPS, rows/drops, fuel/discoveries, sound, `voice` (true unless Voice is OFF) and `voice_mode` (0 OFF, 1 ALERTS, 2 ALL), recent/suppressed counts, avatar schema, GPS speed/movement, ESP-IDF reset reason |
| `GPS FIND` / `GPS CANCEL` | Start/cancel a bounded baud search; saved and active rate are in STATUS; search refuses during recording |
| `START` / `STOP` | Same recording operations as the face buttons |
| `CHECK LOG` | Reopen last drive and verify header/byte count |
| `SCREEN` | Current RGB888 framebuffer; use `--output path.png` |
| `TAP x y` | Execute a touch at a valid 320×240 coordinate |
| `AVATAR GET` / `AVATAR SET DCAV2 …` | Inspect/apply custom profile; legacy DCAV1 accepted; Set refuses during recording |
| `RESTART` | Software restart; refuses during recording or upload |
| `SETUP BEGIN / DATA / END` | Private bounded USB setup transfer; use the web companion instead of typing secrets in a shell |
| `SETUP CLEAR` | Remove private upload configuration/transfer backups; never removes drives |
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


### 0.5 Arcade Scout verification

The actual web flasher installed the four packaged images on K151 and verified the before/after NVS digest unchanged. Original servo calibration, SD files, GPS settings and both user mute switches remained intact. New screenshots are native 320×240 framebuffer captures. The web preview is explicitly labeled as an illustration.

The first new bench drive saved 57 observations, with 15 Wi-Fi food/XP discoveries and 34 approximate BLE addresses saved. It had zero queue drops during the drive. Watching afterward left XP unchanged. Separate framebuffer capture/export sessions can delay packet processing and did increment drops; this is not a claim of zero-loss reception.

The actual browser USB setup module saved a synthetic unavailable network on SD. Failed-network and cancel cases both restored radio counters without a reset, preserved XP and mute settings, and the synthetic configuration was then removed. NFC and proximity devices initialized successfully; NFC tag enrollment and human gesture/proximity behavior still require physical field checks. NFC was returned to off.

Browser checks cover all four website sections at 320, 390, 768 and 1440 pixels, draft import/export, every outfit field, all four habitat previews, keyboard affection, discovery-cheer preview, PNG export, synthetic/mocked WiGLE upload and config download. Host tests cover XP boundaries/saturation and CSV 1.6 conversion. USB tests also exercise a lost initial STATUS reply. No real account credentials or drive submissions were used. Live WiGLE/WDGWars acceptance, TLS under a long upload, and long outdoor recording remain unverified.

A separate 45-second timed bench with the arcade artwork running saved **104 rows / 19 Wi-Fi discoveries / 34 approximate BLE addresses**, reopened its **10,717-byte** file with the expected header, and had **zero additional queue drops**. XP matched credited discoveries and stopped increasing after Stop. No framebuffer transfer was performed during that timed interval. This is a stationary bench sample, not an outdoor coverage result.
