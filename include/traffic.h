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
// A rule that fired, and the address that was transmitting when it did: the sender of the
// disconnect frames, the access point that completed an SSID conflict, or the advert's address.
// That is not always the address the frame's identity match names (a deauth aimed at a camera is
// classified by the camera's address), so the caller lists a hit as its own device.
struct TrafficHit {
  Match match;
  uint8_t mac[6]{};
};
// True for an SSID that is all zero bytes: how many access points hide their name. Two hidden
// networks of the same length are not "the same SSID".
inline bool blankSsid(const uint8_t *ssid, size_t length) {
  for (size_t i = 0; i < length; ++i)
    if (ssid[i])
      return false;
  return true;
}
// Bounded, passive traffic hints. None of these establish a malicious actor.
class TrafficHints {
public:
  // One access point remembered for the SSID-conflict rule. The rule needs an access point seen
  // three times within 30 s, which with channel hopping takes two visits to its channel; if more
  // other access points than the table holds are heard in between, the entry has been reused and
  // the rule cannot fire. The built-in table holds OwnAccessPoints; the firmware attaches a
  // larger one from PSRAM (see attach()).
  struct AccessPoint {
    uint8_t mac[6]{}, ssid[32]{}, length = 0, security = 0, samples = 0;
    bool used = false;
    uint32_t seen = 0;
  };
  static constexpr unsigned OwnAccessPoints = 64, WideAccessPoints = 256;
  // Use caller-owned storage for the access point table (emptied here). Null goes back to the
  // built-in one.
  void attach(AccessPoint *table, unsigned count) {
    wide_ = table && count ? table : nullptr;
    wideCount_ = wide_ ? count : 0;
    for (unsigned i = 0; i < wideCount_; ++i)
      wide_[i] = AccessPoint();
  }
  unsigned accessPoints() const {
    return wide_ ? wideCount_ : OwnAccessPoints;
  }

private:
  struct Disconnect {
    uint8_t mac[6]{};
    bool used = false;
    uint32_t seen = 0;
    dragon::Burst burst, broadcast;
  };
  struct Advert {
    uint8_t mac[6]{}, kind = 0;
    bool used = false;
    uint32_t seen = 0;
  };
  Disconnect disconnects_[16];
  dragon::Burst broadcastAll_;
  AccessPoint own_[OwnAccessPoints];
  AccessPoint *wide_ = nullptr;
  unsigned wideCount_ = 0;
  Advert adverts_[64];

public:
  Match wifi(const uint8_t *data, size_t length, uint32_t now) {
    return wifiHit(data, length, now).match;
  }
  Match ble(const uint8_t *data, size_t length, const uint8_t *mac, uint32_t now) {
    return bleHit(data, length, mac, now).match;
  }
  TrafficHit wifiHit(const uint8_t *data, size_t length, uint32_t now) {
    auto f = dragon::parseWifi(data, length);
    if (!f.valid)
      return {};
    auto hit = [](const uint8_t *mac, Family family, const char *evidence) {
      TrafficHit h;
      h.match = {family, Confidence::Medium, evidence};
      memcpy(h.mac, mac, 6);
      return h;
    };
    // Retransmissions carry the retry bit and are not counted. The sequence number is NOT used to
    // spot repeats: the common deauth tools send one fixed frame over and over with the same
    // sequence number, and those are exactly the frames this rule exists for.
    if ((f.deauth || f.disassoc) && !f.retry && !f.protectedFrame) {
      // A sender keeps its counters while they can still add up: a slot is reused only when it
      // is free or has been idle longer than the longer window. With every slot busy a new
      // sender goes uncounted for now rather than wiping one that is mid-burst.
      Disconnect *slot = nullptr, *idle = nullptr;
      for (auto &d : disconnects_) {
        if (d.used && !memcmp(d.mac, f.source, 6)) {
          slot = &d;
          break;
        }
        if (!d.used) {
          if (!idle || idle->used)
            idle = &d;
        } else if (uint32_t(now - d.seen) > BroadcastWindowMs &&
                   (!idle || (idle->used && uint32_t(now - d.seen) > uint32_t(now - idle->seen))))
          idle = &d;
      }
      if (!slot && idle) {
        slot = idle;
        *slot = {};
        slot->used = true;
        memcpy(slot->mac, f.source, 6);
      }
      static const uint8_t everyone[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
      const bool broadcast = !memcmp(f.destination, everyone, 6);
      // Every counter sees the frame before any rule answers, so one rule firing cannot hide
      // frames from the others.
      bool sender = false, senderBroadcast = false;
      if (slot) {
        slot->seen = now;
        sender = slot->burst.add(now, DisconnectFrames, DisconnectWindowMs);
        senderBroadcast = broadcast && slot->broadcast.add(now, BroadcastFrames, BroadcastWindowMs);
      }
      const bool anyBroadcast =
          broadcast && broadcastAll_.add(now, BroadcastAllFrames, BroadcastWindowMs);
      if (sender)
        return hit(f.source, Family::Deauth,
                   "12 disconnect frames / 2s from one address; cause unverified");
      if (senderBroadcast)
        return hit(f.source, Family::Deauth,
                   "6 broadcast disconnect frames / 3s from one address; cause unverified");
      if (anyBroadcast)
        return hit(f.source, Family::Deauth,
                   "24 broadcast disconnect frames / 3s across senders; cause unverified");
    }
    // A Pwnagotchi's SSID is throwaway, not a network somebody could be impersonating, and a
    // hidden network's zero-filled SSID is no name at all.
    if (f.beacon && f.ssidLength && !blankSsid(f.ssid, f.ssidLength) &&
        f.match.family != dragon::Family::Pwnagotchi) {
      AccessPoint *aps = wide_ ? wide_ : own_;
      const unsigned count = accessPoints();
      // An unknown access point takes a free entry, else the one heard longest ago (which is
      // also the first to pass the 30 s limit and become useless to the rule).
      AccessPoint *slot = &aps[0];
      for (unsigned i = 0; i < count; ++i) {
        AccessPoint &a = aps[i];
        if (a.used && !memcmp(a.mac, f.bssid, 6)) {
          slot = &a;
          break;
        }
        if (slot->used && (!a.used || uint32_t(now - a.seen) > uint32_t(now - slot->seen)))
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
        for (unsigned i = 0; i < count; ++i) {
          const AccessPoint &a = aps[i];
          if (&a != slot && a.used && a.samples >= 3 && uint32_t(now - a.seen) < 30000 &&
              a.length == f.ssidLength && !memcmp(a.ssid, f.ssid, f.ssidLength) &&
              a.security != security && !sameVendor(a.mac, f.bssid))
            return hit(f.bssid, Family::EvilTwin,
                       "Same SSID, open and protected, from two makers; may be legitimate");
        }
    }
    return {};
  }
  TrafficHit bleHit(const uint8_t *data, size_t length, const uint8_t *mac, uint32_t now) {
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
    if (distinct >= 24) {
      TrafficHit h;
      h.match = {Family::BleSpam, Confidence::Medium,
                 "24 pairing addresses / 5s; crowded venue or advert burst"};
      memcpy(h.mac, mac, 6);
      return h;
    }
    return {};
  }
};
} // namespace dash
