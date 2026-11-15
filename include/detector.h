// Dragon Detector 3204e7a passive classification adapter. See docs/SOURCES.md.
#pragma once
#include "core.h"
#include "vendor/dragon/axon.h"
#include "vendor/dragon/detection.h"
namespace dash {
// Each Dragon family keeps its own Dash-Chan family: a Tile is not an AirTag and a Ring doorbell
// is not just "a camera". Fox is Dragon's own training beacon and is not a detection.
inline Match translate(dragon::Match m) {
  using F = dragon::Family;
  Family f = Family::None;
  switch (m.family) {
  case F::Axon:
    f = Family::Axon;
    break;
  case F::Flock:
    f = Family::Flock;
    break;
  case F::Alpr:
    f = Family::Alpr;
    break;
  case F::Meta:
    f = Family::Meta;
    break;
  case F::Pineapple:
    f = Family::Pineapple;
    break;
  case F::Pwnagotchi:
    f = Family::Pwnagotchi;
    break;
  case F::Flipper:
    f = Family::Flipper;
    break;
  case F::Biscuit:
    f = Family::Biscuit;
    break;
  case F::Nyanbox:
    f = Family::Nyanbox;
    break;
  case F::Halehound:
    f = Family::Halehound;
    break;
  case F::Skimmer:
    f = Family::Skimmer;
    break;
  case F::Raven:
    f = Family::Raven;
    break;
  case F::Drone:
    f = Family::Drone;
    break;
  case F::IBeacon:
    f = Family::Beacon;
    break;
  case F::Deauth:
    f = Family::Deauth;
    break;
  case F::EvilTwin:
    f = Family::EvilTwin;
    break;
  case F::BleSpam:
    f = Family::BleSpam;
    break;
  case F::Tracker:
    f = Family::Tracker;
    break;
  case F::SmartTag:
    f = Family::SamsungTag;
    break;
  case F::GoogleTag:
    f = Family::GoogleTag;
    break;
  case F::Tile:
    f = Family::Tile;
    break;
  case F::Ring:
    f = Family::Ring;
    break;
  case F::Camera:
    f = Family::Camera;
    break;
  default:
    break;
  }
  if (f == Family::None)
    return {};
  return {f, static_cast<Confidence>(m.confidence), m.evidence};
}
// Longer text (a 40-character hex drone ID, a long unit name) is cut to fit.
inline void setLabel(char *out, const char *text) {
  size_t n = 0;
  for (; text && text[n] && n < LabelChars - 1; ++n)
    out[n] = text[n];
  out[n] = 0;
}
// The registry row a public, globally administered address falls in, or null.
inline const dragon::VendorPrefix *vendorRow(const uint8_t *mac) {
  if (mac[0] & 3)
    return nullptr;
  const uint32_t prefix = (uint32_t(mac[0]) << 16) | (uint32_t(mac[1]) << 8) | mac[2];
  size_t lo = 0, hi = sizeof(dragon::vendorPrefixes) / sizeof(*dragon::vendorPrefixes);
  while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2;
    if (dragon::vendorPrefixes[mid].prefix < prefix)
      lo = mid + 1;
    else
      hi = mid;
  }
  if (lo == sizeof(dragon::vendorPrefixes) / sizeof(*dragon::vendorPrefixes) ||
      dragon::vendorPrefixes[lo].prefix != prefix)
    return nullptr;
  return &dragon::vendorPrefixes[lo];
}
// A registry row's evidence starts with the registrant ("Hikvision OUI; ...", "DJI / Sz Dji ...").
// Rows that only say "Shared radio vendor" name nobody, so they give no label.
inline void vendorLabel(char *out, const char *evidence) {
  out[0] = 0;
  if (starts(evidence, "Shared"))
    return;
  auto ends = [&](size_t i) {
    return !evidence[i] || evidence[i] == ';' || evidence[i] == ',' ||
           !strncmp(evidence + i, " OUI", 4) || !strncmp(evidence + i, " shared", 7) ||
           !strncmp(evidence + i, " /", 2) || !strncmp(evidence + i, " (", 2);
  };
  size_t n = 0;
  while (n < LabelChars - 1 && !ends(n))
    ++n;
  // A registrant too long for the label loses whole words, not half of one.
  if (!ends(n))
    while (n && evidence[n] != ' ')
      --n;
  memcpy(out, evidence, n);
  out[n] = 0;
}
// The product label of the winning Dragon match: its own sub-label, or the registrant when the
// match is the address's registry row.
inline void matchLabel(char *out, const dragon::Match &m, const uint8_t *mac) {
  out[0] = 0;
  if (m.label) {
    setLabel(out, m.label);
    return;
  }
  const auto *row = vendorRow(mac);
  if (row && row->evidence == m.evidence)
    vendorLabel(out, row->evidence);
}
// "UUUUUU major.minor", as SquachWatch shows it: the first three bytes of the proximity UUID name
// the deployment, and major.minor (big-endian in the block) name the unit.
inline void beaconLabel(char *out, const uint8_t *p, size_t n) {
  for (size_t o = 0; o < n;) {
    const uint8_t l = p[o++];
    if (!l || l > n - o)
      return;
    const uint8_t *d = p + o + 1;
    if (p[o] == 0xff && l >= 26 && d[0] == 0x4c && d[1] == 0 && d[2] == 2 && d[3] == 0x15) {
      snprintf(out, LabelChars, "%02X%02X%02X %u.%u", d[4], d[5], d[6],
               unsigned(d[20]) << 8 | d[21], unsigned(d[22]) << 8 | d[23]);
      return;
    }
    o += l;
  }
}
inline WifiFrame classifyWifi(const uint8_t *data, size_t len) {
  auto d = dragon::parseWifi(data, len);
  WifiFrame f;
  if (!d.valid)
    return f;
  if (d.beacon)
    f = parseWifi(data, len);
  else {
    f.valid = true;
    memcpy(f.mac, d.source, 6);
  }
  if (!f.valid)
    return f;
  const auto best = dragon::withAxon(d.match, dragon::inspectAxonWifi(data, len));
  f.match = translate(best);
  if (f.match.family != Family::None)
    matchLabel(f.label, best, d.source);
  if (f.match.family == Family::Drone && d.drone.valid && d.drone.hasId)
    setLabel(f.label, d.drone.id);
  // A Pwnagotchi beacon announces the unit, not a network: it never becomes a WiGLE access point,
  // and the name worth showing is the one inside its advert rather than its throwaway SSID.
  if (d.match.family == dragon::Family::Pwnagotchi) {
    f.loggable = false;
    if (f.match.family == Family::Pwnagotchi)
      setLabel(f.label, d.name);
  }
  // Receiver research prefixes are clues to an addressed device, not an AP
  // observation. Never write these headers/probes/action frames to WiGLE.
  if (!d.beacon) {
    const auto hint = dragon::addressHint(d.destination, true,
                                          {dragon::Family::Wifi, dragon::Confidence::None, ""});
    auto receiver = translate(hint);
    if (receiver.confidence > f.match.confidence) {
      f.match = receiver;
      f.match.evidence = "Receiver address vendor clue; identity not authenticated";
      matchLabel(f.label, hint, d.destination);
      memcpy(f.mac, d.destination, 6);
    }
  }
  return f;
}
inline BleFrame classifyBle(const uint8_t *data, size_t len, const uint8_t *mac,
                            bool publicAddress) {
  auto d = dragon::parseBle(data, len);
  BleFrame f;
  f.valid = d.valid;
  if (!f.valid)
    return f;
  snprintf(f.name, sizeof(f.name), "%s", d.name);
  auto m = dragon::addressHint(mac, publicAddress, d.match);
  const auto best =
      dragon::withAxon(m, dragon::inspectAxonBle(data, len, 0, mac, publicAddress ? 0 : 1));
  f.match = translate(best);
  if (f.match.family != Family::None)
    matchLabel(f.label, best, mac);
  if (f.match.family == Family::Drone && d.drone.valid && d.drone.hasId)
    setLabel(f.label, d.drone.id);
  if (f.match.family == Family::Beacon)
    beaconLabel(f.label, data, len);
  return f;
}
} // namespace dash
