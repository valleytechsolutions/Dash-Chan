// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <ctype.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>

namespace dash {
enum class Family : uint8_t {
  None,
  Axon,
  Flock,
  Alpr,
  Meta,
  Tracker,
  Flipper,
  Biscuit,
  Pineapple,
  Pwnagotchi,
  Nyanbox,
  Halehound,
  Skimmer,
  Raven,
  Drone,
  Camera,
  Beacon,
  Deauth,
  EvilTwin,
  BleSpam,
  Watchlist
};
enum class Confidence : uint8_t { None, Low, Medium, High };
struct Match {
  Family family = Family::None;
  Confidence confidence = Confidence::None;
  const char *evidence = "No specific signature";
};
inline const char *familyName(Family f) {
  const char *names[] = {
      "Unclassified",     "Axon / body camera", "Flock camera",    "ALPR vendor",
      "Camera glasses",   "Find My / tracker",  "Flipper Zero",    "Biscuit family",
      "WiFi Pineapple",   "Pwnagotchi",         "Nyanbox",         "HaleHound",
      "Serial / skimmer", "Raven candidate",    "Remote ID drone", "Camera / Ring vendor",
      "Proximity beacon", "Disconnect burst",   "SSID conflict",   "BLE advert burst",
      "My watchlist"};
  return names[static_cast<unsigned>(f)];
}
inline const char *confidenceName(Confidence c) {
  const char *n[] = {"none", "low", "medium", "high"};
  return n[static_cast<unsigned>(c)];
}
inline bool contains(const char *h, const char *n) {
  for (; *h; ++h) {
    size_t i = 0;
    while (n[i] && h[i] && tolower((unsigned char)h[i]) == tolower((unsigned char)n[i]))
      ++i;
    if (!n[i])
      return true;
  }
  return !*n;
}
inline bool starts(const char *s, const char *p) {
  while (*p) {
    if (tolower((unsigned char)*s++) != tolower((unsigned char)*p++))
      return false;
  }
  return true;
}
inline void safeText(char *out, size_t cap, const uint8_t *in, size_t n) {
  if (!cap)
    return;
  size_t i = 0;
  for (; i < n && i < cap - 1; ++i)
    out[i] = in[i] >= 32 && in[i] < 127 ? in[i] : '?';
  out[i] = 0;
}
inline uint16_t le16(const uint8_t *p) {
  return p[0] | (uint16_t(p[1]) << 8);
}
inline void promote(Match &a, Match b) {
  if (b.confidence > a.confidence)
    a = b;
}
struct WifiFrame {
  bool valid = false;
  bool loggable = false;
  uint8_t mac[6]{}, channel = 0;
  char name[33]{};
  char auth[64] = "[UNKNOWN][ESS]";
  Match match;
};
struct BleFrame {
  bool valid = false;
  char name[64]{};
  Match match;
};
inline bool parseRsn(const uint8_t *p, size_t n, bool &psk, bool &sae, bool &enterprise,
                     bool &owe) {
  if (n < 8 || le16(p) != 1)
    return false;
  size_t pos = 6;
  size_t count = le16(p + pos);
  pos += 2;
  if (count > (n - pos) / 4)
    return false;
  pos += count * 4;
  if (n - pos < 2)
    return false;
  count = le16(p + pos);
  pos += 2;
  if (!count || count > (n - pos) / 4)
    return false;
  for (size_t i = 0; i < count; ++i, pos += 4)
    if (p[pos] == 0 && p[pos + 1] == 0x0f && p[pos + 2] == 0xac) {
      uint8_t a = p[pos + 3];
      psk |= a == 2 || a == 4 || a == 6;
      sae |= a == 8 || a == 9;
      enterprise |= a == 1 || a == 3 || a == 5 || a == 11 || a == 12 || a == 13;
      owe |= a == 18;
    }
  return true;
}
inline WifiFrame parseWifi(const uint8_t *p, size_t n) {
  WifiFrame f;
  if (!p || n < 36 || (p[0] != 0x80 && p[0] != 0x50))
    return f;
  memcpy(f.mac, p + 16, 6);
  bool privacy = p[34] & 0x10, rsn = false, wpa = false, psk = false, sae = false,
       enterprise = false, owe = false, badRsn = false;
  for (size_t o = 36; o < n;) {
    if (n - o < 2)
      return f;
    uint8_t id = p[o++], len = p[o++];
    if (len > n - o)
      return f;
    const uint8_t *d = p + o;
    if (id == 0) {
      if (len > 32)
        return f;
      safeText(f.name, sizeof(f.name), d, len);
    }
    if (id == 3 && len == 1)
      f.channel = d[0];
    if (id == 48) {
      rsn = true;
      badRsn |= !parseRsn(d, len, psk, sae, enterprise, owe);
    }
    if (id == 221 && len >= 4 && !memcmp(d, "\x00\x50\xf2\x01", 4))
      wpa = true;
    o += len;
  }
  const char *auth = "[OPEN][ESS]";
  if (rsn)
    auth = badRsn         ? "[UNKNOWN][ESS]"
           : (sae && psk) ? "[WPA2-PSK][WPA3-SAE][ESS]"
           : sae          ? "[WPA3-SAE][ESS]"
           : psk          ? "[WPA2-PSK][ESS]"
           : enterprise   ? "[WPA2-EAP][ESS]"
           : owe          ? "[OWE][ESS]"
                          : "[UNKNOWN][ESS]";
  else if (wpa)
    auth = "[WPA][ESS]";
  else if (privacy)
    auth = "[WEP][ESS]";
  snprintf(f.auth, sizeof(f.auth), "%s", auth);
  f.valid = true;
  f.loggable = !(f.mac[0] & 1);
  return f;
}
struct Fix {
  bool valid = false;
  double lat = 0, lon = 0, alt = 0, hdop = 99;
  uint32_t age = 0;
  uint8_t sats = 0;
  char utc[20]{};
};
inline bool usableFix(const Fix &f) {
  return f.valid && f.age <= 2000 && f.sats >= 4 && f.hdop > 0 && f.hdop <= 5 && isfinite(f.lat) &&
         isfinite(f.lon) && isfinite(f.alt) && f.lat >= -90 && f.lat <= 90 && f.lon >= -180 &&
         f.lon <= 180 && strlen(f.utc) == 19;
}
inline std::string csv(const char *s) {
  std::string r = "\"";
  for (; *s; ++s) {
    if (*s == '\"')
      r += '\"';
    r += *s;
  }
  return r + '\"';
}
inline const char *wigleHeader() {
  return "WigleWifi-1.4,appRelease=0.3.0-beta,model=StackChan,release=Dash-Chan,device=ESP32-S3,"
         "display=ILI9342,board=K151,brand=M5Stack\nMAC,SSID,AuthMode,FirstSeen,Channel,RSSI,"
         "CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,Type\n";
}
inline std::string wigleRow(const uint8_t *mac, const char *name, const char *auth, int channel,
                            int rssi, bool ble, const Fix &f) {
  if (!usableFix(f))
    return {};
  char a[18], tail[200];
  snprintf(a, sizeof(a), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4],
           mac[5]);
  // HDOP is dimensionless: estimate horizontal accuracy using 5 m UERE; document this estimate.
  snprintf(tail, sizeof(tail), ",%s,%d,%d,%.7f,%.7f,%.1f,%.1f,%s\n", f.utc, ble ? 0 : channel, rssi,
           f.lat, f.lon, f.alt, f.hdop * 5.0, ble ? "BLE" : "WIFI");
  return std::string(a) + "," + csv(name) + "," + csv(auth) + tail;
}
struct Cooldown {
  uint32_t last = 0;
  bool used = false;
  bool ready(uint32_t now, uint32_t interval) const {
    return !used || uint32_t(now - last) >= interval;
  }
  void mark(uint32_t now) {
    last = now;
    used = true;
  }
};
struct Session {
  bool armed = false;
  uint32_t generation = 0;
  void start() {
    armed = true;
    ++generation;
  }
  void stop() {
    armed = false;
  }
  bool accepts(uint32_t captureGeneration) const {
    return armed && captureGeneration == generation;
  }
};
} // namespace dash
