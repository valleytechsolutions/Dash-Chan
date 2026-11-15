#pragma once
#include "recon.h"
namespace dash {
// A continuing encounter stays quiet even when the general radio cache churns.
// A rotating BLE address cannot be authenticated as the same physical device;
// a separate family quiet period limits those repeated notifications.
//
// The firmware now keeps this per-device state on its ReconTable rows and decides through
// AlertPolicy (recon.h). This small stand-alone tracker remains for callers without a table; it
// shares AlertCooldowns, so the ten-second global quiet never swallows a higher-tier family.
class Encounters {
public:
  struct Entry {
    uint8_t mac[6]{};
    bool used = false, ble = false, notified = false;
    uint8_t addressType = 0;
    Family family = Family::None;
    Confidence notifiedConfidence = Confidence::None;
    uint32_t seen = 0, announced = 0;
  };
  Entry &observe(const uint8_t *mac, bool ble, uint8_t type, Family family, uint32_t now,
                 uint32_t absentMs) {
    Entry *slot = nullptr, *oldest = &entries_[0];
    for (auto &entry : entries_) {
      if (entry.used && entry.ble == ble && entry.addressType == type && entry.family == family &&
          !memcmp(entry.mac, mac, 6)) {
        if (uint32_t(now - entry.seen) >= absentMs)
          entry.notified = false;
        entry.seen = now;
        return entry;
      }
      if (!entry.used && !slot)
        slot = &entry;
      if (uint32_t(now - entry.seen) > uint32_t(now - oldest->seen))
        oldest = &entry;
    }
    auto &entry = *(slot ? slot : oldest);
    entry = {};
    entry.used = true;
    entry.ble = ble;
    entry.addressType = type;
    entry.family = family;
    entry.seen = now;
    memcpy(entry.mac, mac, 6);
    return entry;
  }
  bool eligible(const Entry &entry, Confidence confidence, uint32_t now, uint32_t familyQuietMs,
                bool repeat) const {
    bool changed = !entry.notified || confidence > entry.notifiedConfidence;
    bool reminder = repeat && uint32_t(now - entry.announced) >= 300000;
    return (changed || reminder) && cooldowns_.globalReady(entry.family, now) &&
           cooldowns_.familyReady(entry.family, now, familyQuietMs);
  }
  void announce(Entry &entry, Confidence confidence, uint32_t now) {
    entry.notified = true;
    entry.notifiedConfidence = confidence;
    entry.announced = now;
    cooldowns_.mark(entry.family, now);
  }

private:
  Entry entries_[128];
  AlertCooldowns cooldowns_;
};
} // namespace dash
