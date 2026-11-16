// SPDX-License-Identifier: GPL-3.0-or-later
// Avatar profiles: the one-line text format the website writes, the style packs, and the
// outfit stored against an NFC card. Pure logic, shared with the host tests.
//
//   DCAV3 base body accent eye eyes headwear eyewear neckwear mark back held aura pack persona name
//   DCAV2 base body accent eyes eye headwear eyewear neckwear name     (firmware 0.3 - 0.5)
//   DCAV1 base body accent accessory name                              (firmware 0.2)
//
// Colours are six hex digits, numbers are plain decimals, fields are separated by exactly one
// space, and the name is whatever follows the last one. The name comes last because it may
// contain spaces; that is also why new fields needed a new tag rather than being appended.
#pragma once
#include "avatar_limits.h"
#include "core.h"
#include <stdio.h>
#include <string.h>
namespace dash {
struct AvatarProfile {
  unsigned base = 0, body = 0, accent = 0, accessory = 0;
  unsigned eyes = 0, eye = 0x101029, headwear = 0, eyewear = 0, neckwear = 0;
  unsigned mark = 0, back = 0, held = 0, aura = 0, pack = 0, persona = 0;
  unsigned schema = 0; // Which format the line was written in: 1, 2 or 3.
  char name[limits::NameChars + 1]{};
};
namespace profile_detail {
// One decimal field below `count`, followed by a space. No sign, no padding, no leading zero.
inline bool number(const char *&s, unsigned count, unsigned &out) {
  unsigned value = 0, digits = 0;
  while (*s >= '0' && *s <= '9') {
    if (++digits > 2)
      return false;
    value = value * 10 + unsigned(*s++ - '0');
  }
  if (!digits || (digits == 2 && value < 10) || *s != ' ' || value >= count)
    return false;
  ++s;
  out = value;
  return true;
}
// Exactly six hex digits, followed by a space.
inline bool colour(const char *&s, unsigned &out) {
  unsigned value = 0;
  for (int i = 0; i < 6; ++i, ++s) {
    char c = *s;
    int digit = c >= '0' && c <= '9'   ? c - '0'
                : c >= 'a' && c <= 'f' ? c - 'a' + 10
                : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                       : -1;
    if (digit < 0)
      return false;
    value = value << 4 | unsigned(digit);
  }
  if (*s != ' ')
    return false;
  ++s;
  out = value;
  return true;
}
// 1-16 letters, digits, spaces, '-' or '_', starting and ending on something visible.
inline bool validName(const char *s) {
  size_t n = strlen(s);
  if (n < 1 || n > limits::NameChars || s[0] == ' ' || s[n - 1] == ' ')
    return false;
  for (const char *c = s; *c; ++c)
    if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') ||
          *c == ' ' || *c == '-' || *c == '_'))
      return false;
  return true;
}
} // namespace profile_detail

// Every field must be in range or the whole line is refused and `out` is left untouched.
inline bool parseAvatar(const char *line, AvatarProfile &out) {
  using namespace profile_detail;
  using namespace limits;
  if (!line || strlen(line) > ProfileChars)
    return false;
  AvatarProfile p;
  const char *s = line + 6;
  if (!strncmp(line, "DCAV3 ", 6)) {
    p.schema = 3;
    if (!(number(s, Bases, p.base) && colour(s, p.body) && colour(s, p.accent) &&
          colour(s, p.eye) && number(s, Eyes, p.eyes) && number(s, Headwear, p.headwear) &&
          number(s, Eyewear, p.eyewear) && number(s, Neckwear, p.neckwear) &&
          number(s, Marks, p.mark) && number(s, Backs, p.back) && number(s, Held, p.held) &&
          number(s, Auras, p.aura) && number(s, Packs, p.pack) && number(s, Personas, p.persona)))
      return false;
  } else if (!strncmp(line, "DCAV2 ", 6)) {
    p.schema = 2;
    if (!(number(s, Bases, p.base) && colour(s, p.body) && colour(s, p.accent) &&
          number(s, V2Eyes, p.eyes) && colour(s, p.eye) && number(s, V2Headwear, p.headwear) &&
          number(s, V2Eyewear, p.eyewear) && number(s, V2Neckwear, p.neckwear)))
      return false;
  } else if (!strncmp(line, "DCAV1 ", 6)) {
    p.schema = 1;
    if (!(number(s, V1Bases, p.base) && colour(s, p.body) && colour(s, p.accent) &&
          number(s, V1Accessories, p.accessory)))
      return false;
    p.eye = p.base == 2 ? 0x84f7b5 : 0x101029;
    p.headwear = p.accessory == 1 ? 1 : p.accessory == 3 ? 2 : 0;
    p.neckwear = p.accessory == 2 ? 1 : 0;
  } else
    return false;
  if (!validName(s))
    return false;
  strcpy(p.name, s);
  out = p;
  return true;
}

// The oldest format that can carry this profile. Plain designs stay DCAV2 so they still load on
// firmware that has never heard of DCAV3. The style-pack number is only a tag and draws nothing,
// so it does not force DCAV3 by itself: a design that otherwise fits is written as DCAV2 and the
// tag is the one thing left out. (Otherwise a design could never be made "classic" again once a
// pack had been tried, however much gear was taken off.)
inline unsigned avatarSchema(const AvatarProfile &p) {
  using namespace limits;
  bool plain = p.eyes < V2Eyes && p.headwear < V2Headwear && p.eyewear < V2Eyewear &&
               p.neckwear < V2Neckwear && !p.mark && !p.back && !p.held && !p.aura && !p.persona;
  return plain ? 2 : 3;
}
// Writes the profile as one line, exactly as the website does. False if it is not valid.
inline bool formatAvatar(const AvatarProfile &p, char *out, size_t size) {
  using namespace limits;
  if (p.base >= Bases || p.eyes >= Eyes || p.headwear >= Headwear || p.eyewear >= Eyewear ||
      p.neckwear >= Neckwear || p.mark >= Marks || p.back >= Backs || p.held >= Held ||
      p.aura >= Auras || p.pack >= Packs || p.persona >= Personas || p.body > 0xffffff ||
      p.accent > 0xffffff || p.eye > 0xffffff || !profile_detail::validName(p.name))
    return false;
  int n = avatarSchema(p) == 2
              ? snprintf(out, size, "DCAV2 %u %06X %06X %u %06X %u %u %u %s", p.base, p.body,
                         p.accent, p.eyes, p.eye, p.headwear, p.eyewear, p.neckwear, p.name)
              : snprintf(out, size, "DCAV3 %u %06X %06X %06X %u %u %u %u %u %u %u %u %u %u %s",
                         p.base, p.body, p.accent, p.eye, p.eyes, p.headwear, p.eyewear, p.neckwear,
                         p.mark, p.back, p.held, p.aura, p.pack, p.persona, p.name);
  return n > 0 && size_t(n) < size;
}

// Style packs: a whole look in one tap. A pack dresses the current shape and never renames it.
// The website's packs (site/profile.mjs) must list the same signature looks; a test checks.
struct PackStyle {
  const char *name;
  uint32_t body, accent, eye;
  uint8_t eyes, headwear, eyewear, neckwear, mark, back, held, aura;
  uint8_t backdrop; // Suggested world; applied on the robot only when you cycle packs there.
};
// clang-format off
constexpr PackStyle PackStyles[limits::Packs] = {
    {"None",           0xfff3d2, 0xff88b6, 0x101029, 0, 0,  0, 0, 0, 0, 0, 0, 0},
    {"Kawaii pastel",  0xffd9ec, 0xff6fae, 0x6b3fa0, 1, 1,  0, 6, 1, 6, 0, 2, 3},
    {"Cyberpunk neon", 0x1a1433, 0xff2bd6, 0x2bf5ff, 4, 6,  7, 0, 5, 5, 0, 3, 0},
    {"Goth",           0x17141f, 0xb3123c, 0xf2e9ff, 9, 7,  0, 5, 4, 2, 4, 5, 2},
    {"Vaporwave",      0xb967ff, 0x01cdfe, 0xfffb96, 7, 4,  0, 0, 2, 1, 1, 1, 1},
    {"Retro terminal", 0x0d1f12, 0x33ff66, 0x8dffa8, 4, 10, 0, 0, 7, 4, 3, 3, 0},
};
// clang-format on
// Applies a pack's signature look. Shape, name and persona are the owner's and are kept.
inline void applyPack(AvatarProfile &p, unsigned pack) {
  if (pack >= limits::Packs)
    return;
  const PackStyle &s = PackStyles[pack];
  p.body = s.body;
  p.accent = s.accent;
  p.eye = s.eye;
  p.eyes = s.eyes;
  p.headwear = s.headwear;
  p.eyewear = s.eyewear;
  p.neckwear = s.neckwear;
  p.mark = s.mark;
  p.back = s.back;
  p.held = s.held;
  p.aura = s.aura;
  p.pack = pack;
}

// What an NFC outfit card stores. Version 1 cards (firmware up to 0.5) are still read: they
// are a prefix of today's record, so the new slots simply start out empty.
struct OutfitCardV1 {
  uint8_t version, avatar, base, eyes, head, glasses, neck, theme, scene;
  uint16_t body, accent, eye;
  char name[limits::NameChars + 1];
};
struct OutfitCard {
  uint8_t version = 2, avatar = 0, base = 0, eyes = 0, head = 0, glasses = 0, neck = 0, theme = 0,
          scene = 0;
  uint16_t body = 0, accent = 0, eye = 0;
  char name[limits::NameChars + 1]{};
  uint8_t mark = 0, back = 0, held = 0, aura = 0, pack = 0, persona = 0;
};
// Reads a stored card of either size. False if the bytes are not a card this firmware trusts.
inline bool readOutfitCard(const void *bytes, size_t size, OutfitCard &out) {
  using namespace limits;
  OutfitCard o;
  if (size == sizeof(OutfitCardV1)) {
    OutfitCardV1 v1;
    memcpy(&v1, bytes, sizeof(v1));
    if (v1.version != 1 || v1.eyes >= V2Eyes || v1.head >= V2Headwear || v1.glasses >= V2Eyewear ||
        v1.neck >= V2Neckwear)
      return false;
    o.avatar = v1.avatar;
    o.base = v1.base;
    o.eyes = v1.eyes;
    o.head = v1.head;
    o.glasses = v1.glasses;
    o.neck = v1.neck;
    o.theme = v1.theme;
    o.scene = v1.scene;
    o.body = v1.body;
    o.accent = v1.accent;
    o.eye = v1.eye;
    memcpy(o.name, v1.name, sizeof(o.name));
  } else if (size == sizeof(OutfitCard)) {
    memcpy(&o, bytes, sizeof(o));
    if (o.version != 2)
      return false;
  } else
    return false;
  o.version = 2;
  // Avatar 0-5 (3 is Custom), four themes, four worlds: the same checks as ever.
  if (o.avatar > 5 || o.base >= Bases || o.eyes >= Eyes || o.head >= Headwear ||
      o.glasses >= Eyewear || o.neck >= Neckwear || o.theme > 3 || o.scene > 3 || o.mark >= Marks ||
      o.back >= Backs || o.held >= Held || o.aura >= Auras || o.pack >= Packs ||
      o.persona >= Personas || o.name[NameChars])
    return false;
  out = o;
  return true;
}
} // namespace dash
