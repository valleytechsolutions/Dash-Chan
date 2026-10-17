# Flash, update, and recover

**Supported target: official M5Stack StackChan K151, ESP32-S3, 16 MB flash, 8 MB Quad PSRAM.** Other CoreS3 boards and DIY Stack-chan builds need a different hardware port.

## Browser installer

Open **[Dash-Chan Web Flasher](https://valleytechsolutions.github.io/Dash-Chan/#install)** in desktop Chrome or Edge on HTTPS. Firefox/Safari and phone flashing are not supported by this interface.

1. Connect a USB **data** cable. Close PlatformIO monitors, other browser serial sessions, and desktop serial tools. Stop any wardrive.
2. Confirm the official K151 checkbox and click **Connect StackChan**. Choose its Espressif USB serial port. The flasher validates ESP32-S3 and 16 MB; these checks cannot distinguish a K151 from another board with the same chip.
3. If this is your first installation, use **Download factory backup**. It reads all 16 MB and downloads a SHA-256 companion file. Keep both private. Backups may contain original settings and credentials.
4. Click **Install Dash-Chan**. Downloads are checked against SHA-256 before any flash write. Four split images are written without whole-chip erase. Each written image is verified with the chip's MD5 operation.
5. The installer reads the NVS/calibration range before and after writing and checks that it is unchanged, then explicitly pulses reset. Look for the BWM/VTS boot screen and your pet. Disconnect closes the browser's serial handle.

The site self-hosts a pinned esptool-js bundle and all firmware files. There is no runtime CDN, analytics, external font, or server-side serial connection. Firmware downloads need internet; the robot runs offline afterward. The microSD is never formatted by the installer.

If port selection is canceled, reconnect normally. During writing, keep USB connected and the tab open. If a write/check/reset fails, the page reports the failure; it never labels an unchecked image successful. Re-enter boot mode and retry the same release. A post-flash automatic browser-side application handshake is not performed; the displayed success means flash checks and calibration preservation passed, not that GPS or every peripheral is working.

### Boot mode and connection trouble

- On K151, hold the **bottom RST button beside the microSD slot for about three seconds**, then release when its indicator turns green. Select the newly appearing Espressif port. This is the [official M5Stack download-mode procedure](https://docs.m5stack.com/en/StackChan#download-mode).
- Try another data cable or direct computer USB port. A power-only cable will not work.
- Close all other users of the serial port, including the avatar uploader. Installer and avatar upload cannot own it simultaneously.
- If the installer completed but the face has not appeared, briefly press RESET.
- Do not use “erase flash” as a connection fix. It destroys NVS and can remove factory servo calibration.
- GPS WAIT does not mean installation failed. Confirm blue Port C, the selected baud, and a clear sky view.

## Manual flashing

Download the release ZIP from [Releases](https://github.com/valleytechsolutions/Dash-Chan/releases), extract it, and verify `SHA256SUMS.txt` before writing.

```sh
python -m pip install esptool==5.4.0
python -m esptool --chip esp32s3 --port YOUR_PORT read-flash 0x0 0x1000000 stackchan-factory.bin
python -m esptool --chip esp32s3 --port YOUR_PORT --baud 460800 write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB 0x0 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin
```

Replace `YOUR_PORT` with the confirmed robot port (for example `COM60` on the development machine; do not assume that number on yours). **DIO** in the boot header is required on the tested K151 configuration. The Arduino `qio_qspi` memory profile is a separate PSRAM/build setting and should remain as pinned.

| Image | Offset | Maximum reserved range |
|---|---:|---:|
| bootloader.bin | `0x0000` | below `0x8000` |
| partitions.bin | `0x8000` | below `0x9000` |
| NVS — never supplied/written | `0x9000` | through `0xDFFF` |
| boot_app0.bin | `0xE000` | below `0x10000` |
| firmware.bin | `0x10000` | below `0x510000` |

The package and browser both validate sector-rounded image bounds. An app-only update at `0x10000` is appropriate **only after this Dash-Chan partition layout is already installed**. A first installation needs all four split images.

## Restore the original firmware

Use your own complete 16 MB backup for this specific robot. Check its saved SHA-256 first. Restoring overwrites the firmware and settings with the backed-up state; it does not change SD files.

```sh
python -m esptool --chip esp32s3 --port YOUR_PORT --baud 460800 write-flash 0x0 stackchan-factory.bin
```

Do not substitute another robot's backup or calibration. If your original backup is missing, consult M5Stack's official recovery/calibration documentation for the exact model.

## Build the same release

```sh
git clone https://github.com/valleytechsolutions/Dash-Chan.git
cd Dash-Chan
git checkout v0.4.0-beta
python -m pip install platformio==6.2.0
pio run -e stackchan
python tools/package.py
```

The ZIP contains split images, install instructions, corresponding-source links, checksums, and runtime license notices. The package script also prepares `site/firmware/` for the static web flasher.
