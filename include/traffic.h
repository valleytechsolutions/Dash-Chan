#pragma once
#include "detector.h"
namespace dash {
// Bounded, passive traffic hints. None of these establish a malicious actor.
class TrafficHints {
  struct Disconnect {
    uint8_t mac[6]{};
    uint16_t sequence = 0;
    bool used = false;
    uint32_t seen = 0;
    dragon::Burst burst;
  };
  struct AccessPoint {
    uint8_t mac[6]{}, ssid[32]{}, length = 0, security = 0, samples = 0;
    bool used = false;
    uint32_t seen = 0;
  };
  struct Advert {
    uint8_t mac[6]{}, kind = 0;
    bool used = false;
    uint32_t seen = 0;
  };
  Disconnect disconnects_[16];
  AccessPoint aps_[64];
  Advert adverts_[64];

public:
  Match wifi(const uint8_t *data, size_t length, uint32_t now) {
    auto f = dragon::parseWifi(data, length);
    if (!f.valid)
      return {};
    if ((f.deauth || f.disassoc) && !f.retry && !f.protectedFrame) {
      Disconnect *slot = &disconnects_[0];
      for (auto &d : disconnects_) {
        if (d.used && !memcmp(d.mac, f.source, 6)) {
          slot = &d;
          break;
        }
        if (!d.used || uint32_t(now - d.seen) > uint32_t(now - slot->seen))
          slot = &d;
      }
      if (!slot->used || memcmp(slot->mac, f.source, 6)) {
        *slot = {};
        slot->used = true;
        memcpy(slot->mac, f.source, 6);
      }
      bool duplicate = slot->seen && slot->sequence == f.sequence;
      slot->seen = now;
      slot->sequence = f.sequence;
      if (!duplicate && slot->burst.add(now, 12, 2000))
        return {Family::Deauth, Confidence::Medium,
                "12 disconnect frames / 2s from one address; cause unverified"};
    }
    if (f.beacon && f.ssidLength) {
      AccessPoint *slot = &aps_[0];
      for (auto &a : aps_) {
        if (a.used && !memcmp(a.mac, f.bssid, 6)) {
          slot = &a;
          break;
        }
        if (!a.used || uint32_t(now - a.seen) > uint32_t(now - slot->seen))
          slot = &a;
      }
      uint8_t security = f.security ? 1 : 0;
      bool same = slot->used && !memcmp(slot->mac, f.bssid, 6) && slot->length == f.ssidLength &&
                  !memcmp(slot->ssid, f.ssid, f.ssidLength) && slot->security == security &&
                  uint32_t(now - slot->seen) < 30000;
      if (!same) {
        *slot = {};
        slot->used = true;
        memcpy(slot->mac, f.bssid, 6);
        memcpy(slot->ssid, f.ssid, f.ssidLength);
        slot->length = f.ssidLength;
        slot->security = security;
      }
      slot->seen = now;
      if (slot->samples < 3)
        ++slot->samples;
      if (slot->samples >= 3)
        for (const auto &a : aps_)
          if (&a != slot && a.used && a.samples >= 3 && uint32_t(now - a.seen) < 30000 &&
              a.length == f.ssidLength && !memcmp(a.ssid, f.ssid, f.ssidLength) &&
              a.security != security)
            return {Family::EvilTwin, Confidence::Low,
                    "Same SSID has open and protected APs; may be legitimate"};
    }
    return {};
  }
  Match ble(const uint8_t *data, size_t length, const uint8_t *mac, uint32_t now) {
    auto f = dragon::parseBle(data, length);
    if (!f.valid)
      return {};
    uint8_t kind = f.applePopup ? 1 : f.swiftPair ? 2 : f.fastPair ? 3 : 0;
    if (!kind)
      return {};
    Advert *slot = &adverts_[0];
    for (auto &a : adverts_) {
      if (a.used && a.kind == kind && !memcmp(a.mac, mac, 6)) {
        slot = &a;
        break;
      }
      if (!a.used || uint32_t(now - a.seen) > uint32_t(now - slot->seen))
        slot = &a;
    }
    slot->used = true;
    slot->kind = kind;
    slot->seen = now;
    memcpy(slot->mac, mac, 6);
    unsigned distinct = 0;
    for (const auto &a : adverts_)
      if (a.used && a.kind == kind && uint32_t(now - a.seen) < 5000)
        ++distinct;
    if (distinct >= 24)
      return {Family::BleSpam, Confidence::Medium,
              "24 pairing addresses / 5s; crowded venue or advert burst"};
    return {};
  }
};
} // namespace dash
