// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <limits.h>
#include <stdint.h>
namespace dash {
constexpr uint32_t XpPerLevel = 25;
inline uint32_t petLevel(uint32_t xp) {
  return 1 + xp / XpPerLevel;
}
inline unsigned levelProgress(uint32_t xp) {
  return xp % XpPerLevel;
}
inline void earnDiscoveryXp(uint32_t &xp) {
  if (xp < UINT32_MAX)
    ++xp;
}
} // namespace dash
