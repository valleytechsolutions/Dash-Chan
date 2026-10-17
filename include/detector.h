// Dragon Detector 3204e7a passive classification adapter. See docs/SOURCES.md.
#pragma once
#include "core.h"
#include "vendor/dragon/axon.h"
#include "vendor/dragon/detection.h"
namespace dash {
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
  case F::SmartTag:
  case F::GoogleTag:
  case F::Tile:
    f = Family::Tracker;
    break;
  case F::Ring:
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
  f.match = translate(dragon::withAxon(d.match, dragon::inspectAxonWifi(data, len)));
  // Receiver research prefixes are clues to an addressed device, not an AP
  // observation. Never write these headers/probes/action frames to WiGLE.
  if (!d.beacon) {
    auto receiver = translate(dragon::addressHint(
        d.destination, true, {dragon::Family::Wifi, dragon::Confidence::None, ""}));
    if (receiver.confidence > f.match.confidence) {
      f.match = receiver;
      f.match.evidence = "Receiver address vendor clue; identity not authenticated";
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
  f.match = translate(
      dragon::withAxon(m, dragon::inspectAxonBle(data, len, 0, mac, publicAddress ? 0 : 1)));
  return f;
}
} // namespace dash
