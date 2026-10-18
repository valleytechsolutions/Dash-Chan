# Dash-Chan on M5Burner

**Official M5Stack StackChan K151 only: ESP32-S3, 16 MB flash, 8 MB Quad PSRAM.**

## Recommended installation

Use the [Dash-Chan web installer](https://valleytechsolutions.github.io/Dash-Chan/#install). It backs up the robot and writes four separate firmware images while verifying that the existing NVS/servo-calibration range is unchanged. It is also the recommended way to update.

## M5Burner factory-reset image

M5Burner's public upload form accepts one `.bin`, and its desktop burner writes that binary at address `0x0`. The supplied `Dash-Chan-0.4.0-beta-K151-FACTORY-RESET.bin` is a **clean merged factory-reset image**, assembled from the four published v0.4.0-beta release images. It is not an export of a personal robot.

**Burning this merged file resets NVS, including saved Dash-Chan preferences and factory servo zero positions.** This happens even without a separate whole-chip erase: the merged file's blank gaps are also flashed. It does not preserve calibration. The SD card is not formatted by the image.

Use the web installer instead unless you have backed up your own robot and can restore its calibration. Do not enable head motion after a factory reset until calibration has been restored. Head motion and speech default to off on a fresh Dash-Chan installation; alert sounds can be disabled in Audio settings.

### Advanced backup and restore procedure

Install `esptool==5.4.0`, close other serial applications and enter download mode using the K151 bottom RST button (hold about three seconds, release when the indicator turns green). Replace `YOUR_PORT` with the robot's actual port.

Before using M5Burner:

```sh
python -m esptool --chip esp32s3 --port YOUR_PORT read-flash 0x0 0x1000000 my-stackchan-full-backup.bin
python -m esptool --chip esp32s3 --port YOUR_PORT read-flash 0x9000 0x5000 my-stackchan-nvs.bin
```

Keep both files private. They can contain credentials and settings. Never upload them to M5Burner, GitHub, or another person's device.

Select Dash-Chan for **StackChan**, read the factory-reset warning, and use M5Burner's Burn operation. The image starts at **0x0**, includes bootloader/partition table/OTA initialization/application, and already has the required DIO, 80 MHz, 16 MB boot header. Do not use a separate Erase operation. This replaces the previous firmware.

After installation, keep head motion off, close M5Burner's serial connection, enter download mode again and restore only this same robot's saved NVS:

```sh
python -m esptool --chip esp32s3 --port YOUR_PORT write-flash 0x9000 my-stackchan-nvs.bin
```

Restart the robot. Existing Dash-Chan preferences, if present in the backup, are restored along with calibration. A full original-firmware restore is described in [FLASHING.md](FLASHING.md). If no backup is available, use M5Stack's calibration/recovery process for the exact K151 before enabling motion; another robot's zero positions are not a substitute.

## Package verification

The merged image is derived from the public v0.4.0-beta release ZIP and checked against the SHA-256 manifest served by the web installer. Each embedded image is compared byte-for-byte at its intended offset. The entire `0x9000–0xDFFF` gap is verified as erased (`0xFF`) and contains no factory dump, observations, settings or credentials. This blank gap is why the M5Burner installation resets calibration.

The corresponding split-image release was built by CI and installed on the development K151. **A factory-reset installation through M5Burner has not been physically tested.** Its review/publication status is controlled by M5Stack; submission is not approval.

Source: [v0.4.0-beta](https://github.com/valleytechsolutions/Dash-Chan/tree/v0.4.0-beta). License: GPL-3.0-or-later, with dependency notices retained in the [release ZIP](https://github.com/valleytechsolutions/Dash-Chan/releases/tag/v0.4.0-beta). See [SOURCES.md](SOURCES.md) for full credits.


The current 0.5 Arcade Scout update is available through the [web flasher](https://valleytechsolutions.github.io/Dash-Chan/) and [GitHub releases](https://github.com/valleytechsolutions/Dash-Chan/releases). The M5Burner submission described here is the earlier 0.4 factory image; its listing/approval status is separate.
