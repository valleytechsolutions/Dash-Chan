#pragma once
#include "core.h"
namespace dash {
struct AvatarProfile {
  unsigned base = 0, body = 0, accent = 0, accessory = 0;
  char name[17]{};
};
inline bool parseAvatar(const char *line, AvatarProfile &out) {
  if (!line || strlen(line) > 60)
    return false;
  AvatarProfile p;
  char extra;
  if (sscanf(line, "DCAV1 %u %6x %6x %u %16[ -~]%c", &p.base, &p.body, &p.accent, &p.accessory,
             p.name, &extra) != 5 ||
      p.base > 2 || p.accessory > 3 || !*p.name)
    return false;
  if (p.body > 0xffffff || p.accent > 0xffffff)
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
