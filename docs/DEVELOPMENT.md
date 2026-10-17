# Architecture and verification

Radio callbacks only timestamp and copy bounded packets into a fixed FreeRTOS queue. They do not render, allocate unbounded strings, write SD, speak, or move servos. The main loop drains at most eight observations per pass and services GPS between them. Oversized/full-queue observations are counted as drops. Malformed lengths are rejected by the parser.

Wi-Fi runs in `WIFI_MODE_NULL`, listening passively for 300 ms on each US channel 1–11. BLE gets a 1.6-second passive window after each sweep. The screen refreshes at 20 Hz. Scan state is independent of recording state and UI page. A single S3 radio cannot hear all channels/transports at once.

Recording has a monotonic session generation copied into each received packet. This prevents an observation queued before Start or in an earlier session from appearing in a newly opened file. Logging requires a fresh, quality-gated UTC GPS fix and queue latency ≤250 ms. Stale-fix packets are skipped, not backfilled with a later location. Files are separate from the event CSV. SD write failure stops recording and reports the error; background radio watch continues.

Servo work uses the official BSP's asynchronous motion task and a narrow application travel range. UI, pet needs, randomized chatter, siren timing, and scan windows use elapsed milliseconds. No cloud AI or credentials are involved.

Idle motion is a nonblocking gesture sequencer with two or three eased poses and variable rests depending on activity, gesture, and fuel. Relaxed travel stays within ±20° yaw and 14.5–31° pitch; alert travel uses the wider limits described in the README. Eye gaze eases toward each intended turn. Alert dismissal cancels the previous idle sequence and returns toward center. Host checks exercise every avatar across randomized gestures, verify idle and panic travel/speed bounds and return-to-center, and cross the millisecond-counter wrap.

Nutrition is credited only after a Wi-Fi row passes the session/fix checks and is successfully written. The per-drive BSSID filter is independent of the expiring contact cache. Every five new discoveries add 12 fuel; duplicate sightings cannot add fuel. Petting only changes bond. Fuel persists in NVS alongside experience.

## Build verification

`pio run -e stackchan` produces `.pio/build/stackchan/firmware.bin` and `firmware.elf`. The ELF is useful for debugging; publish only the intended artifacts. Dependency versions and the BSP commit are pinned in `platformio.ini`.

`python tools/test.py` uses a system GNU compiler, `CXX`/`CC`, or `ziglang==0.13.0`. It compiles the actual protocol/CSV headers, checks supported signatures and false-positive guard cases, exercises malformed lengths and 50,000 deterministic random inputs, verifies fresh GPS gating and session/cooldown behavior, and round-trips the emitted 11-column CSV with Python's CSV reader. Random-input checks are smoke fuzzing, not a proof of parser correctness.

`tools/render_avatars.cpp` generates the preview from the firmware drawing routine. It is a design preview, not a photograph of the physical LCD.

## Bench evidence and remaining acceptance checks

The target identifies as board 27 (StackChan), ESP32-S3 with 16 MB flash and 8 MB PSRAM. A complete factory flash backup was retained locally before installation. USB status has confirmed sustained Wi-Fi/BLE reception, a 320 × 240 landscape display, and valid fixes on Port C at 115200 baud. The user confirmed that the display orientation is correct. Host tests include duplicate-food protection, session reset, five-network snack boundaries, and CSV round trips.

An explicitly selected card reporting 511,868,665,856 bytes (512 GB nominal) was successfully quick-formatted as FAT32 through the device. In an approximately 19-second bench session with eight satellites, Dash-Chan saved 37 Wi-Fi/BLE observations and credited 11 distinct Wi-Fi discoveries, taking fuel from 77 to 100. After Stop, the 4,004-byte file reopened with the expected WiGLE header; eight seconds of background watch awarded no further food. The radio queue reported zero drops. GPS diagnostics showed 2,185 valid sentences and one checksum failure after the format and recording sequence. These results do not establish full-card capacity integrity, WiGLE server acceptance, outdoor-drive performance, or field detection accuracy.

After installing the final application build and resetting the device, the same card mounted automatically, fuel persisted at 100, and GPS reacquired a valid fix. No card reinsertion or format was needed after that reset.

The user confirmed head movement during the Axon demo and audible speaker output at the raised volume. Subsequent tuning increases playful alert volume, speech emphasis, and movement intensity. The demo always selects the first spoken phrase for repeatable checks; actual alerts select among the three reactions.

Remaining physical checks:

- Correct board/flash/PSRAM detected; boot without reset loop; no unintended servo movement.
- Touch coordinates, display orientation, head touch, three avatars, all menus.
- Speaker clarity, quiet mode, playful versus gentle banter, demo versus real alert separation.
- Servo travel, factory zero positions, motion-off behavior, stationary installation.
- GPS unit wiring, baud, NMEA reception, outdoor fix, loss/reacquisition, no-fix start.
- SD absent/full/removal, session numbering, repeated Stop/Start, valid CSV upload to WiGLE.
- Simultaneous UI, GPS, SD, radio and audio during a long drive; queue drops and memory stability.
- Compare known emitting devices against signatures; document false positives and missed detections.

Do not mark these as passed based on a successful compile or host fixtures.
