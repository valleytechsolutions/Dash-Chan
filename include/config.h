#pragma once
#include <stddef.h>
#include <stdint.h>
namespace dash {
constexpr const char *Version = "0.1.0-alpha";
// K151 StackChan. Its servos own UART1/G6/G7. GPS uses UART2.
// Port C is RX G18 / TX G17; change only after checking your GPS wiring.
constexpr int GpsRx = 18, GpsTx = 17;
constexpr uint32_t GpsBaud = 9600;
constexpr int SdCs = 4, SdSck = 36, SdMiso = 35, SdMosi = 37;
constexpr uint8_t LastWifiChannel = 11;
constexpr uint32_t WifiDwellMs = 300, BleWindowMs = 1600;
constexpr size_t ContactCapacity = 256, HistoryCapacity = 20;
} // namespace dash
