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
// Levels carry a rank so a number on screen means something as the buddy grows up.
inline const char *rankName(uint32_t level) {
  return level >= 75   ? "Legend"
         : level >= 40 ? "Signal Sage"
         : level >= 20 ? "Pathfinder"
         : level >= 10 ? "Ranger"
         : level >= 5  ? "Scout"
         : level >= 2  ? "Rookie"
                       : "Hatchling";
}
inline void earnDiscoveryXp(uint32_t &xp) {
  if (xp < UINT32_MAX)
    ++xp;
}
} // namespace dash
