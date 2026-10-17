#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
namespace dash {
struct GpsPreset {
  const char *name;
  uint32_t baud;
  bool adapter;
};
constexpr GpsPreset GpsPresets[] = {
    {"Manual / current", 9600, false},   {"Unit GPS (AT6558)", 9600, false},
    {"Unit GPS v1.1", 115200, false},    {"Module GPS (M8N)", 9600, true},
    {"Module COMX GPS", 9600, true},     {"Module GPS v2.0", 115200, true},
    {"Module GPS v2.1", 115200, true},   {"Module GNSS (M9N)", 38400, true},
    {"Atomic GPS Base", 9600, true},     {"Atomic GPS v2.0", 115200, true},
    {"Cap LoRa GPS UART", 115200, true}, {"ATGM336H-5N DIY", 9600, true},
    {"ATGM336H-6N DIY", 115200, true},   {"Other NMEA UART", 9600, true}};
constexpr unsigned GpsPresetCount = sizeof(GpsPresets) / sizeof(GpsPresets[0]);
constexpr uint32_t GpsRates[] = {9600, 115200, 38400, 57600, 19200, 4800, 230400};
constexpr unsigned GpsRateCount = sizeof(GpsRates) / sizeof(GpsRates[0]);
constexpr int GpsInputPins[] = {18, 17, 1, 2, 8, 9};
constexpr unsigned GpsInputPinCount = sizeof(GpsInputPins) / sizeof(GpsInputPins[0]);
struct GpsConfig {
  uint8_t profile = 0, port = 0, rx = 18;
  uint32_t baud = 9600;
};
inline bool validGpsConfig(const GpsConfig &c) {
  bool rate = false, pin = false;
  for (auto b : GpsRates)
    rate |= b == c.baud;
  for (auto p : GpsInputPins)
    pin |= p == c.rx;
  return c.profile < GpsPresetCount && c.port < 4 && pin && rate;
}
inline int gpsRxPin(const GpsConfig &c) {
  return c.port == 0 ? 18 : c.port == 1 ? 1 : c.port == 3 ? 8 : c.rx;
}
// Bounded, checksum-first NMEA framing. Normalize only known GNSS talkers,
// after checking the original checksum, for TinyGPSPlus's GGA/RMC decoder.
class NmeaFrame {
  char data_[128]{};
  size_t used_ = 0;
  bool collecting_ = false;
  static int hex(char c) {
    return c >= '0' && c <= '9'   ? c - '0'
           : c >= 'A' && c <= 'F' ? c - 'A' + 10
           : c >= 'a' && c <= 'f' ? c - 'a' + 10
                                  : -1;
  }

public:
  uint32_t good = 0, bad = 0;
  const char *line() const {
    return data_;
  }
  bool feed(char c) {
    if (c == '$') {
      used_ = 0;
      collecting_ = true;
      data_[used_++] = c;
      return false;
    }
    if (!collecting_)
      return false;
    if (c == '\r')
      return false;
    if (c != '\n') {
      if (c < 32 || c > 126 || used_ >= sizeof(data_) - 2) {
        collecting_ = false;
        ++bad;
      } else
        data_[used_++] = c;
      return false;
    }
    collecting_ = false;
    data_[used_] = 0;
    if (used_ < 10 || data_[used_ - 3] != '*') {
      ++bad;
      return false;
    }
    uint8_t sum = 0;
    for (size_t i = 1; i < used_ - 3; ++i)
      sum ^= data_[i];
    int hi = hex(data_[used_ - 2]), lo = hex(data_[used_ - 1]);
    if (hi < 0 || lo < 0 || sum != ((hi << 4) | lo)) {
      ++bad;
      return false;
    }
    ++good;
    bool talker = !strncmp(data_ + 1, "BD", 2) || (data_[1] == 'G' && strchr("PNLABQI", data_[2]));
    if (!talker || data_[6] != ',' ||
        (strncmp(data_ + 3, "GGA", 3) && strncmp(data_ + 3, "RMC", 3)))
      return false;
    data_[1] = 'G';
    data_[2] = 'P';
    sum = 0;
    for (size_t i = 1; i < used_ - 3; ++i)
      sum ^= data_[i];
    constexpr char digits[] = "0123456789ABCDEF";
    data_[used_ - 2] = digits[sum >> 4];
    data_[used_ - 1] = digits[sum & 15];
    data_[used_++] = '\n';
    data_[used_] = 0;
    return true;
  }
};
// A one-shot search; two checksum-valid GGA/RMC sentences establish a link,
// even before satellite lock. The caller restores the old baud on failure.
class GpsBaudSearch {
  uint32_t at_ = 0;
  unsigned index_ = 0, hits_ = 0;
  bool active_ = false;

public:
  void start(uint32_t now) {
    at_ = now;
    index_ = hits_ = 0;
    active_ = true;
  }
  bool active() const {
    return active_;
  }
  uint32_t baud() const {
    return GpsRates[index_];
  }
  void cancel() {
    active_ = false;
  }
  bool sentence() {
    if (active_ && ++hits_ >= 2) {
      active_ = false;
      return true;
    }
    return false;
  }
  // 0 wait, 1 switch rate, -1 exhausted.
  int tick(uint32_t now) {
    if (!active_ || uint32_t(now - at_) < 3500)
      return 0;
    at_ = now;
    hits_ = 0;
    if (index_ + 1 >= GpsRateCount) {
      active_ = false;
      return -1;
    }
    ++index_;
    return 1;
  }
};
} // namespace dash
