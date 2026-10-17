# Hardware bring-up

This build targets the official **K151 StackChan**. A similarly named DIY robot can have different servos, PSRAM, power hardware, and GPIO assignments. Do not assume compatibility.

| Peripheral | Assignment |
|---|---|
| Built-in LCD/touch/speaker/power | M5Unified + M5GFX board detection |
| Feetech servos | StackChan-BSP, UART1, G6/G7, 1 Mbaud |
| GPS Port C | UART2, host RX G18, host TX G17 |
| GPS Port A option | UART2, host RX G1, host TX G2 |
| SD | CS G4, SCK G36, MISO G35, MOSI G37; 4 MHz with 1 MHz mount fallback |
| Head touch / lights | StackChan-BSP through built-in I2C |

The GPS Unit and stackable GPS Module/Module GPS v2 are different products. Verify the actual model, cable orientation, UART voltage, baud, and switch/pin routing. The default profile is for an external UART unit connected through Port C. Never connect RS-232 voltage levels directly to ESP32 GPIO. GPS settings cannot be changed during an active recording session.

Before installing:

1. Identify the USB port by unplugging/reconnecting just StackChan. Close M5Burner and serial monitors using it.
2. Enter download mode using the **bottom RST button held for around three seconds, then released at the green LED**, according to the K151 documentation. The side power button held for six seconds turns it off instead.
3. Read the chip identity and flash size with esptool. Save a **full 16 MB flash backup** outside Git (the ignored `backups/` directory is suitable). Factory backups may contain Wi-Fi credentials and pairing data; do not publish them.
4. Build, then flash only the verified port. The partition layout reserves 5 MB per app slot. This is not an OTA-compatible update to the factory application.
5. Confirm the display, touches, SD mount, speaker, and GPS NMEA byte counter. Wait outdoors for a fix. Validate a saved CSV before relying on a drive.
6. Check mechanical clearance and enable Motion. Avoid pulling on the head. If calibration is wrong, switch Motion off and restore/calibrate with the official firmware before retesting.

Example with an installed esptool (replace the port):

```sh
esptool --chip esp32s3 --port YOUR_PORT flash-id
esptool --chip esp32s3 --port YOUR_PORT read-flash 0 ALL backups/factory.bin
```

Esptool 4 uses underscore command names (`flash_id`, `read_flash`). Restoring a complete verified backup overwrites the current firmware and settings. M5Burner also offers the factory firmware; follow M5Stack's restoration instructions.

Use the packaged split-image instructions with **DIO** bootloader flash mode. The QIO/QSPI Arduino memory profile enables runtime Quad flash/PSRAM access; forcing a QIO bootloader header caused a watchdog reset loop on the tested K151. Preserve the NVS sectors and factory servo calibration. LCD rotation 1 provides the 320 × 240 landscape interface.

The project carries a scoped [Espressif SD-driver backport](../lib/DashChanSD/README.md) for card initialization and completed writes. Shared Arduino/PlatformIO package files are not modified. SD mount starts at 4 MHz and retries at 1 MHz if needed.

USB maintenance commands are supported by `tools/device.py --port YOUR_PORT COMMAND`: `STATUS`, `SCREEN`, `TAP x y`, `START`, `STOP`, and `CHECK LOG`. `STATUS` includes NMEA checksum counters, mounted SD capacity, food progress, and storage errors. After stopping, `CHECK LOG` reopens the latest session and checks its WiGLE header and file size.

`SOUND TEST` plays a short diagnostic tone and reports the playback configuration and amplifier registers; `SOUND STATUS` reads those diagnostics without playing anything. The explicit tone test can sound even if normal companion sounds are set to Quiet.

The destructive command `FORMAT SD <exact_bytes>` performs a FAT32 quick format of the inserted card, removing all its files. It refuses while recording or if the supplied capacity does not match. For a card without a mountable FAT filesystem, it initializes the physical card and reads its capacity again before writing anything. Use only when intentionally erasing that card; this is not secure data erasure. Large cards can take several minutes; keep power connected until `FORMAT OK` or a failure response arrives. Normal boot and SD retry never format a card.

Official references: [StackChan pin map/download mode](https://docs.m5stack.com/en/StackChan), [SD example](https://docs.m5stack.com/en/arduino/stackchan/sdcard), [servo BSP](https://github.com/m5stack/StackChan-BSP).
