#pragma once
#include <stddef.h>
#include <stdint.h>
namespace dash {
constexpr const char *Version = "0.3.0-beta";
// K151 StackChan. Its servos own UART1/G6/G7. GPS uses UART2.
// Port C is RX G18 / TX G17; change only after checking your GPS wiring.
constexpr int GpsRx = 18, GpsTx = 17;
constexpr uint32_t GpsBaud = 9600;
constexpr int SdCs = 4, SdSck = 36, SdMiso = 35, SdMosi = 37;
constexpr uint8_t LastWifiChannel = 11;
constexpr uint32_t WifiDwellDriveMs = 140, WifiDwellWatchMs = 180;
constexpr uint32_t BleDriveMs = 850, BleWatchMs = 1100;
constexpr size_t ContactCapacity = 256, HistoryCapacity = 32;
} // namespace dash
