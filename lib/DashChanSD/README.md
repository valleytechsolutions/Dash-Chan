# SD driver backport

Base: Espressif Arduino ESP32 **3.0.7**, `libraries/SD/src`, Apache-2.0. Original copyright headers are retained, including Neil Thiessen's CRC implementation. This local library replaces the framework SD library only in this project; shared PlatformIO installations are not modified.

`sd_diskio.cpp` carries the initialization and write-path functions from the following upstream commits, adapted to the 3.0.7 framework:

- [`4e086ffdac5c992ec5a959e462b22e42b26c26fb`](https://github.com/espressif/arduino-esp32/commit/4e086ffdac5c992ec5a959e462b22e42b26c26fb): SPI initialization arguments, two idle commands, CRC retry, three-second readiness timeout.
- [`c1edb8347d1e4b32286774fb40f413edbc15225c`](https://github.com/espressif/arduino-esp32/commit/c1edb8347d1e4b32286774fb40f413edbc15225c): data-response decoding and waiting for a completed multi-block write before checking status.

Changed functions: `sdWait`, `sdStop`, `sdWriteBytes`, `sdWriteSector`, `sdWriteSectors`, and `ff_sd_initialize`, plus their timing/token constants. Initialization failures use error-level diagnostics. Dash-Chan also adds `sdcard_refresh_sectors` and its header declaration to read card capacity under the SPI lock before an explicitly requested format of an unmounted card. Other source files remain from 3.0.7. The application avoids reinitializing an already-remounted card after FAT32 formatting.
