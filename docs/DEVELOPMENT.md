# Architecture and verification

Radio callbacks only timestamp and copy bounded packets into a fixed FreeRTOS queue. They do not render, allocate unbounded strings, write SD, speak, or move servos. The main loop drains at most eight observations per pass and services GPS between them. Oversized/full-queue observations are counted as drops. Malformed lengths are rejected by the parser.

Wi-Fi runs in `WIFI_MODE_NULL`, listening passively for 300 ms on each US channel 1–11. BLE gets a 1.6-second passive window after each sweep. The screen refreshes at 20 Hz. Scan state is independent of recording state and UI page. A single S3 radio cannot hear all channels/transports at once.

Recording has a monotonic session generation copied into each received packet. This prevents an observation queued before Start or in an earlier session from appearing in a newly opened file. Logging requires a fresh, quality-gated UTC GPS fix and queue latency ≤250 ms. Stale-fix packets are skipped, not backfilled with a later location. Files are separate from the event CSV. SD write failure stops recording and reports the error; background radio watch continues.

Servo work uses the official BSP's asynchronous motion task and a narrow application travel range. UI, pet needs, randomized chatter, siren timing, and scan windows use elapsed milliseconds. No cloud AI or credentials are involved.

## Build verification

`pio run -e stackchan` produces `.pio/build/stackchan/firmware.bin` and `firmware.elf`. The ELF is useful for debugging; publish only the intended artifacts. Dependency versions and the BSP commit are pinned in `platformio.ini`.

`python tools/test.py` uses a system GNU compiler, `CXX`/`CC`, or `ziglang==0.13.0`. It compiles the actual protocol/CSV headers, checks supported signatures and false-positive guard cases, exercises malformed lengths and 50,000 deterministic random inputs, verifies fresh GPS gating and session/cooldown behavior, and round-trips the emitted 11-column CSV with Python's CSV reader. Random-input checks are smoke fuzzing, not a proof of parser correctness.

`tools/render_avatars.cpp` generates the preview from the firmware drawing routine. It is a design preview, not a photograph of the physical LCD.

## Physical acceptance checklist (pending)

- Correct board/flash/PSRAM detected; boot without reset loop; no unintended servo movement.
- Touch coordinates, display orientation, head touch, three avatars, all menus.
- Speaker clarity, quiet mode, playful versus gentle banter, demo versus real alert separation.
- Servo travel, factory zero positions, motion-off behavior, stationary installation.
- GPS unit wiring, baud, NMEA reception, outdoor fix, loss/reacquisition, no-fix start.
- SD absent/full/removal, session numbering, repeated Stop/Start, valid CSV upload to WiGLE.
- Simultaneous UI, GPS, SD, radio and audio during a long drive; queue drops and memory stability.
- Compare known emitting devices against signatures; document false positives and missed detections.

Do not mark these as passed based on a successful compile or host fixtures.
