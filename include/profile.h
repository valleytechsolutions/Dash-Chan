#pragma once
#include "core.h"
namespace dash {
struct AvatarProfile {
  unsigned base = 0, body = 0, accent = 0, accessory = 0;
  unsigned eyes = 0, eye = 0x101029, headwear = 0, eyewear = 0, neckwear = 0;
  char name[17]{};
};
inline bool parseAvatar(const char *line, AvatarProfile &out) {
  if (!line || strlen(line) > 90)
    return false;
  AvatarProfile p;
  char extra;
  if (!strncmp(line, "DCAV1 ", 6)) {
    if (sscanf(line, "DCAV1 %u %6x %6x %u %16[ -~]%c", &p.base, &p.body, &p.accent, &p.accessory,
               p.name, &extra) != 5 ||
        p.accessory > 3)
      return false;
    p.eye = p.base == 2 ? 0x84f7b5 : 0x101029;
    p.headwear = p.accessory == 1 ? 1 : p.accessory == 3 ? 2 : 0;
    p.neckwear = p.accessory == 2 ? 1 : 0;
  } else if (sscanf(line, "DCAV2 %u %6x %6x %u %6x %u %u %u %16[ -~]%c", &p.base, &p.body,
                    &p.accent, &p.eyes, &p.eye, &p.headwear, &p.eyewear, &p.neckwear, p.name,
                    &extra) != 9)
    return false;
  if (p.base > 4 || (!strncmp(line, "DCAV1 ", 6) && p.base > 2) || p.eyes > 5 || p.headwear > 5 ||
      p.eyewear > 3 || p.neckwear > 3 || !*p.name)
    return false;
  if (p.body > 0xffffff || p.accent > 0xffffff || p.eye > 0xffffff)
    return false;
  bool visible = false;
  for (const char *c = p.name; *c; ++c) {
    if (!isalnum((unsigned char)*c) && *c != ' ' && *c != '-' && *c != '_')
      return false;
    visible |= *c != ' ';
  }
  if (!visible)
    return false;
  out = p;
  return true;
}
} // namespace dash
