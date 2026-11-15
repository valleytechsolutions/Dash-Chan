#pragma once
#include "detector.h"
namespace dash {
// Disconnect (deauth / disassoc) burst rules. Any one of them reports a burst:
//   - DisconnectFrames from one sender inside DisconnectWindowMs, whoever they are addressed to;
//   - BroadcastFrames broadcast-addressed ones from one sender inside BroadcastWindowMs;
//   - BroadcastAllFrames broadcast-addressed ones from any mix of senders inside that window,
//     which is what a "deauth everything" tool spoofing many access points looks like.
// The two broadcast rules and their numbers are SquachWatch's (docs/SOURCES.md).
constexpr uint16_t DisconnectFrames = 12, BroadcastFrames = 6, BroadcastAllFrames = 24;
constexpr uint32_t DisconnectWindowMs = 2000, BroadcastWindowMs = 3000;
// Mesh nodes and the second radio of one box derive their BSSIDs from one base address, often by
// setting the locally administered bit, so that bit is ignored when comparing makers.
inline bool sameVendor(const uint8_t *a, const uint8_t *b) {
  return (a[0] & ~2) == (b[0] & ~2) && a[1] == b[1] && a[2] == b[2];
}
// Bounded, passive traffic hints. None of these establish a malicious actor.
class TrafficHints {
  struct Disconnect {
    uint8_t mac[6]{};
    uint16_t sequence = 0;
    bool used = false;
    uint32_t seen = 0;
    dragon::Burst burst, broadcast;
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
  dragon::Burst broadcastAll_;
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
      if (!duplicate) {
        static const uint8_t everyone[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
        const bool broadcast = !memcmp(f.destination, everyone, 6);
        // Every counter sees the frame before any rule answers, so one rule firing cannot hide
        // frames from the others.
        const bool sender = slot->burst.add(now, DisconnectFrames, DisconnectWindowMs);
        const bool senderBroadcast =
            broadcast && slot->broadcast.add(now, BroadcastFrames, BroadcastWindowMs);
        const bool anyBroadcast =
            broadcast && broadcastAll_.add(now, BroadcastAllFrames, BroadcastWindowMs);
        if (sender)
          return {Family::Deauth, Confidence::Medium,
                  "12 disconnect frames / 2s from one address; cause unverified"};
        if (senderBroadcast)
          return {Family::Deauth, Confidence::Medium,
                  "6 broadcast disconnect frames / 3s from one address; cause unverified"};
        if (anyBroadcast)
          return {Family::Deauth, Confidence::Medium,
                  "24 broadcast disconnect frames / 3s across senders; cause unverified"};
      }
    }
    // A Pwnagotchi's SSID is throwaway, not a network somebody could be impersonating.
    if (f.beacon && f.ssidLength && f.match.family != dragon::Family::Pwnagotchi) {
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
      // Confirmed means: both access points seen three times within 30 s, one open and one
      // protected, from different makers. Same-maker pairs are mesh or guest networks.
      if (slot->samples >= 3)
        for (const auto &a : aps_)
          if (&a != slot && a.used && a.samples >= 3 && uint32_t(now - a.seen) < 30000 &&
              a.length == f.ssidLength && !memcmp(a.ssid, f.ssid, f.ssidLength) &&
              a.security != security && !sameVendor(a.mac, f.bssid))
            return {Family::EvilTwin, Confidence::Medium,
                    "Same SSID, open and protected, from two makers; may be legitimate"};
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
