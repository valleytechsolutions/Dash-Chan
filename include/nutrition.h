// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
#include <string.h>

namespace dash {
// A small per-drive Bloom filter keeps repeated BSSIDs from earning food, even
// when the short-lived radio contact cache expires. Collisions can undercount
// discoveries, but never turn a repeated BSSID into an extra snack.
class WardriveFood {
  uint8_t seen[2048]{};
  uint32_t discoveries_ = 0;

public:
  static constexpr unsigned NetworksPerSnack = 5;
  void reset() {
    memset(seen, 0, sizeof(seen));
    discoveries_ = 0;
  }
  bool discover(const uint8_t mac[6]) {
    uint32_t h = 2166136261u, step = 5381;
    for (unsigned i = 0; i < 6; ++i) {
      h = (h ^ mac[i]) * 16777619u;
      step = (step * 33) ^ mac[i];
    }
    step |= 1;
    bool fresh = false;
    for (unsigned i = 0; i < 3; ++i) {
      unsigned bit = (h + i * step) & 16383;
      uint8_t mask = uint8_t(1u << (bit & 7));
      fresh |= !(seen[bit >> 3] & mask);
      seen[bit >> 3] |= mask;
    }
    if (fresh)
      ++discoveries_;
    return fresh;
  }
  uint32_t discoveries() const {
    return discoveries_;
  }
  unsigned progress() const {
    return discoveries_ % NetworksPerSnack;
  }
};
} // namespace dash
