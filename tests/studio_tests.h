// Avatar Studio host tests: profile lines (DCAV1/2/3), slot bounds, style packs, NFC outfit
// cards and the gear art. Included by core_test.cpp, which supplies CHECK.
#pragma once
#include "avatar_limits.h"
#include "avatars.h"
#include "home_view.h"
#include "profile.h"
#include "studio_view.h"
#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <vector>

#define SCHECK(expr)                                                                               \
  do {                                                                                             \
    if (!(expr)) {                                                                                 \
      std::cerr << "CHECK failed at studio_tests.h:" << __LINE__ << ": " << #expr << '\n';         \
      std::exit(1);                                                                                \
    }                                                                                              \
  } while (0)

namespace studio_tests {
using namespace dash;

// What firmware 0.3 - 0.5 does with a profile line, kept verbatim so "old firmware stays
// safe" is something these tests can actually run.
struct LegacyProfile {
  unsigned base = 0, body = 0, accent = 0, accessory = 0;
  unsigned eyes = 0, eye = 0x101029, headwear = 0, eyewear = 0, neckwear = 0;
  char name[17]{};
};
inline bool legacyParse(const char *line, LegacyProfile &out) {
  if (!line || strlen(line) > 90)
    return false;
  LegacyProfile p;
  char extra;
  if (!strncmp(line, "DCAV1 ", 6)) {
    if (sscanf(line, "DCAV1 %u %6x %6x %u %16[ -~]%c", &p.base, &p.body, &p.accent, &p.accessory,
               p.name, &extra) != 5 ||
        p.accessory > 3)
      return false;
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

// A canvas that remembers the bounding box of everything drawn on it.
struct Blot {
  char kind;
  int x0, y0, x1, y1, w, h, r;
  uint16_t c;
};
struct Tracer {
  std::vector<Blot> inks;
  void fillEllipse(int x, int y, int rx, int ry, uint16_t c) {
    inks.push_back({'e', x - rx, y - ry, x + rx, y + ry, rx, ry, 0, c});
  }
  void fillCircle(int x, int y, int r, uint16_t c) {
    inks.push_back({'c', x - r, y - r, x + r, y + r, r, r, r, c});
  }
  void fillRect(int x, int y, int w, int h, uint16_t c) {
    inks.push_back({'r', x, y, x + w - 1, y + h - 1, w, h, 0, c});
  }
  void fillRoundRect(int x, int y, int w, int h, int r, uint16_t c) {
    inks.push_back({'r', x, y, x + w - 1, y + h - 1, w, h, r, c});
  }
  void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) {
    inks.push_back({'t', std::min({x0, x1, x2}), std::min({y0, y1, y2}), std::max({x0, x1, x2}),
                    std::max({y0, y1, y2}), 0, 0, 0, c});
  }
  void drawLine(int x0, int y0, int x1, int y1, uint16_t c) {
    inks.push_back(
        {'l', std::min(x0, x1), std::min(y0, y1), std::max(x0, x1), std::max(y0, y1), 0, 0, 0, c});
  }
};
// A pen that writes each call down, to compare the firmware's art with the website's copy.
struct Scribe {
  std::ostream &out;
  const char *tag;
  void oval(int x, int y, int rx, int ry, uint16_t c) {
    out << tag << " oval " << x << ' ' << y << ' ' << rx << ' ' << ry << ' ' << c << '\n';
  }
  void box(int x, int y, int w, int h, int r, uint16_t c) {
    out << tag << " box " << x << ' ' << y << ' ' << w << ' ' << h << ' ' << r << ' ' << c << '\n';
  }
  void tri(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) {
    out << tag << " tri " << x0 << ' ' << y0 << ' ' << x1 << ' ' << y1 << ' ' << x2 << ' ' << y2
        << ' ' << c << '\n';
  }
  void line2(int x0, int y0, int x1, int y1, uint16_t c, int w = 2) {
    out << tag << " line2 " << x0 << ' ' << y0 << ' ' << x1 << ' ' << y1 << ' ' << c << ' ' << w
        << '\n';
  }
  void bar(int x0, int y0, int x1, int y1, uint16_t c, int w) {
    out << tag << " bar " << x0 << ' ' << y0 << ' ' << x1 << ' ' << y1 << ' ' << c << ' ' << w
        << '\n';
  }
};

inline Look customLook(int base = 0) {
  Look look;
  look.custom = true;
  look.base = base;
  return look;
}
inline size_t inkCount(const Look &look, const Pose &pose = {}) {
  Tracer t;
  drawBuddy(t, look, 0, 0, pose);
  return t.inks.size();
}
inline std::string line(const AvatarProfile &p) {
  char text[128];
  SCHECK(formatAvatar(p, text, sizeof(text)));
  return text;
}
inline bool same(const AvatarProfile &a, const AvatarProfile &b) {
  return a.base == b.base && a.body == b.body && a.accent == b.accent && a.eyes == b.eyes &&
         a.eye == b.eye && a.headwear == b.headwear && a.eyewear == b.eyewear &&
         a.neckwear == b.neckwear && a.mark == b.mark && a.back == b.back && a.held == b.held &&
         a.aura == b.aura && a.pack == b.pack && a.persona == b.persona && !strcmp(a.name, b.name);
}

struct Slot {
  const char *name;
  unsigned AvatarProfile::*field;
  unsigned count;
};
// Every numbered field of a DCAV3 line, in the order it is written.
static const Slot Slots[] = {
    {"base", &AvatarProfile::base, limits::Bases},
    {"eyes", &AvatarProfile::eyes, limits::Eyes},
    {"headwear", &AvatarProfile::headwear, limits::Headwear},
    {"eyewear", &AvatarProfile::eyewear, limits::Eyewear},
    {"neckwear", &AvatarProfile::neckwear, limits::Neckwear},
    {"mark", &AvatarProfile::mark, limits::Marks},
    {"back", &AvatarProfile::back, limits::Backs},
    {"held", &AvatarProfile::held, limits::Held},
    {"aura", &AvatarProfile::aura, limits::Auras},
    {"pack", &AvatarProfile::pack, limits::Packs},
    {"persona", &AvatarProfile::persona, limits::Personas},
};

inline void profiles() {
  AvatarProfile p;
  // The documented example, field by field.
  SCHECK(parseAvatar("DCAV3 3 17141F B3123C F2E9FF 9 7 0 5 4 2 4 5 3 1 Nyx the 2nd", p));
  SCHECK(p.schema == 3 && p.base == 3 && p.body == 0x17141f && p.accent == 0xb3123c &&
         p.eye == 0xf2e9ff && p.eyes == 9 && p.headwear == 7 && p.eyewear == 0 && p.neckwear == 5 &&
         p.mark == 4 && p.back == 2 && p.held == 4 && p.aura == 5 && p.pack == 3 &&
         p.persona == 1 && !strcmp(p.name, "Nyx the 2nd"));
  // Every slot: its last option is accepted, one past it is refused, in a line otherwise valid.
  for (const Slot &slot : Slots) {
    AvatarProfile q;
    strcpy(q.name, "Edge");
    q.mark = 1; // Keeps the line DCAV3 whichever slot is under test.
    q.*slot.field = slot.count - 1;
    AvatarProfile back;
    SCHECK(parseAvatar(line(q).c_str(), back) && same(q, back) && back.schema == 3);
    q.*slot.field = slot.count;
    char text[128];
    SCHECK(!formatAvatar(q, text, sizeof(text)));
    // The parser must refuse it too, not just the writer: build the line by hand.
    unsigned v[11];
    for (int i = 0; i < 11; ++i)
      v[i] = &slot == &Slots[i] ? slot.count : 0;
    snprintf(text, sizeof(text), "DCAV3 %u FFFFFF 000000 123456 %u %u %u %u %u %u %u %u %u %u Edge",
             v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10]);
    SCHECK(!parseAvatar(text, back));
    v[&slot - Slots] = slot.count - 1;
    snprintf(text, sizeof(text), "DCAV3 %u FFFFFF 000000 123456 %u %u %u %u %u %u %u %u %u %u Edge",
             v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10]);
    SCHECK(parseAvatar(text, back) && back.*slot.field == slot.count - 1);
  }
  // Strict fields. A refused line leaves the caller's profile exactly as it was.
  AvatarProfile keep;
  SCHECK(parseAvatar("DCAV2 1 AABBCC 112233 2 445566 3 1 2 Keeper", keep));
  const char *refused[] = {
      "DCAV3 0 FFF FFFFFF 000000 0 0 0 0 0 0 0 0 0 0 Short hex",
      "DCAV3 0 FFFFFFF FFFFFF 000000 0 0 0 0 0 0 0 0 0 0 Long hex",
      "DCAV3 0 FFFFFG FFFFFF 000000 0 0 0 0 0 0 0 0 0 0 Not hex",
      "DCAV3 0 #FFFFF FFFFFF 000000 0 0 0 0 0 0 0 0 0 0 Hash",
      "DCAV3 0 FFFFFF FFFFFF 000000 0 0 0 0 0 0 0 0 0 0  Two spaces",
      "DCAV3 0  FFFFFF FFFFFF 000000 0 0 0 0 0 0 0 0 0 0 Two spaces",
      "DCAV3 0 FFFFFF FFFFFF 000000 0 0 0 0 0 0 0 0 0 0 Trailing ",
      "DCAV3 0 FFFFFF FFFFFF 000000 0 0 0 0 0 0 0 0 0 0 ",
      "DCAV3 0 FFFFFF FFFFFF 000000 0 0 0 0 0 0 0 0 0 0",
      "DCAV3 0 FFFFFF FFFFFF 000000 0 0 0 0 0 0 0 0 0 Missing",
      "DCAV3 0 FFFFFF FFFFFF 000000 00 0 0 0 0 0 0 0 0 0 Padded",
      "DCAV3 0 FFFFFF FFFFFF 000000 0 011 0 0 0 0 0 0 0 0 Padded",
      "DCAV3 0 FFFFFF FFFFFF 000000 +1 0 0 0 0 0 0 0 0 0 Signed",
      "DCAV3 0 FFFFFF FFFFFF 000000 -1 0 0 0 0 0 0 0 0 0 Signed",
      "DCAV3 0 FFFFFF FFFFFF 000000 0x1 0 0 0 0 0 0 0 0 0 Hex",
      "DCAV3 0 FFFFFF FFFFFF 000000 1e0 0 0 0 0 0 0 0 0 0 Exp",
      "DCAV3 0 FFFFFF FFFFFF 000000 4294967296 0 0 0 0 0 0 0 0 0 Wrap",
      "DCAV3 0 FFFFFF FFFFFF 000000 0 0 0 0 0 0 0 0 0 0 Bad;name",
      "DCAV3 0 FFFFFF FFFFFF 000000 0 0 0 0 0 0 0 0 0 0 Seventeen chars!!",
      "DCAV3 0 FFFFFF FFFFFF 000000 0 0 0 0 0 0 0 0 0 0 Tab\there",
      "DCAV3 0\tFFFFFF FFFFFF 000000 0 0 0 0 0 0 0 0 0 0 Tab",
      "dcav3 0 FFFFFF FFFFFF 000000 0 0 0 0 0 0 0 0 0 0 Lower tag",
      "DCAV4 0 FFFFFF FFFFFF 000000 0 0 0 0 0 0 0 0 0 0 Future",
      "DCAV3",
      "DCAV3 ",
      " DCAV3 0 FFFFFF FFFFFF 000000 0 0 0 0 0 0 0 0 0 0 Indent",
      "DCAV2 0 FF FF 0 0 0 0 0 Hi",               // Short hex used to be accepted.
      "DCAV2 0 FFFFFF FFFFFF 0 000000 0 0 0 Hi ", // So did a trailing space.
      "DCAV2 0 FFFFFF FFFFFF 0 000000 0 0 0  Hi", // And extra spaces before the name.
      "DCAV2 00 FFFFFF FFFFFF 0 000000 0 0 0 Hi",
      "DCAV1 0 FFFFF FF8800 1 Hi",
      "DCAV1 0 FFFFFF FF8800 01 Hi",
      "",
  };
  for (const char *bad : refused) {
    AvatarProfile q = keep;
    SCHECK(!parseAvatar(bad, q) && same(q, keep) && q.schema == 2);
  }
  SCHECK(!parseAvatar(nullptr, keep));
  std::string huge = "DCAV3 0 FFFFFF FFFFFF 000000 0 0 0 0 0 0 0 0 0 0 " + std::string(200, 'a');
  SCHECK(!parseAvatar(huge.c_str(), keep));
  // Lower-case hex and every character a name may hold are fine.
  SCHECK(parseAvatar("DCAV3 4 a0b1c2 D3e4F5 00ff00 1 2 3 4 5 6 5 6 5 5 Az09 _-x", p));
  SCHECK(p.body == 0xa0b1c2 && p.accent == 0xd3e4f5 && p.eye == 0x00ff00 && p.aura == 6 &&
         !strcmp(p.name, "Az09 _-x"));
  SCHECK(parseAvatar("DCAV3 0 000000 000000 000000 0 0 0 0 0 0 0 0 0 0 a", p)); // Shortest name.
  SCHECK(parseAvatar("DCAV3 0 000000 000000 000000 9 11 7 7 7 6 5 6 5 5 Sixteen chars ok", p));
  SCHECK(p.headwear == 11 && strlen(p.name) == limits::NameChars);

  // The two older formats keep their own, smaller bounds and default every new slot.
  SCHECK(parseAvatar("DCAV2 4 FF4D24 FFE040 5 201029 5 3 3 Old max", p));
  SCHECK(p.schema == 2 && p.eyes == 5 && p.headwear == 5 && p.eyewear == 3 && p.neckwear == 3 &&
         !p.mark && !p.back && !p.held && !p.aura && !p.pack && !p.persona);
  for (const char *over : {"DCAV2 5 FFFFFF FFFFFF 0 000000 0 0 0 No",  // base
                           "DCAV2 0 FFFFFF FFFFFF 6 000000 0 0 0 No",  // eyes: 6 is DCAV3-only
                           "DCAV2 0 FFFFFF FFFFFF 0 000000 6 0 0 No",  // headwear
                           "DCAV2 0 FFFFFF FFFFFF 0 000000 0 4 0 No",  // eyewear
                           "DCAV2 0 FFFFFF FFFFFF 0 000000 0 0 4 No"}) // neckwear
    SCHECK(!parseAvatar(over, p));
  // Digits after the gear are part of a DCAV2 name, exactly as old firmware reads them.
  SCHECK(parseAvatar("DCAV2 0 FFFFFF FFFFFF 0 000000 0 0 0 Scout 3 2", p) &&
         !strcmp(p.name, "Scout 3 2") && !p.mark);
  SCHECK(parseAvatar("DCAV1 2 112233 FFAABB 3 Old Orbit", p));
  SCHECK(p.schema == 1 && p.base == 2 && p.accessory == 3 && p.headwear == 2 && !p.neckwear &&
         p.eye == 0x84f7b5 && !p.eyes && !p.mark && !p.aura && !p.persona);
  SCHECK(parseAvatar("DCAV1 0 112233 FFAABB 2 Old Mochi", p) && p.neckwear == 1 && !p.headwear &&
         p.eye == 0x101029);
  SCHECK(!parseAvatar("DCAV1 3 112233 FFAABB 0 Wisp", p)); // DCAV1 predates Wisp and Ember.
  SCHECK(!parseAvatar("DCAV1 0 112233 FFAABB 4 Nope", p));

  // Which format a profile is written in: DCAV2 while it fits there, DCAV3 as soon as any one
  // new field or new option is used.
  AvatarProfile plain;
  strcpy(plain.name, "Plain");
  plain.base = 4;
  plain.eyes = 5;
  plain.headwear = 5;
  plain.eyewear = 3;
  plain.neckwear = 3;
  SCHECK(avatarSchema(plain) == 2 && line(plain) == "DCAV2 4 000000 000000 5 101029 5 3 3 Plain");
  for (const Slot &slot : Slots) {
    AvatarProfile q = plain;
    if (slot.field == &AvatarProfile::base)
      continue; // Every shape fits DCAV2.
    bool old = slot.field == &AvatarProfile::eyes || slot.field == &AvatarProfile::headwear ||
               slot.field == &AvatarProfile::eyewear || slot.field == &AvatarProfile::neckwear;
    q.*slot.field = old ? q.*slot.field + 1 : 1;
    if (slot.field == &AvatarProfile::pack) {
      // The style-pack number is a tag that draws nothing. Alone it does not make a design
      // unreadable to old firmware: the line stays DCAV2 and only the tag is left out, so a
      // design can always be made classic again by taking the new gear off.
      AvatarProfile back;
      SCHECK(avatarSchema(q) == 2 && line(q) == line(plain));
      SCHECK(parseAvatar(line(q).c_str(), back) && back.pack == 0 && same(plain, back));
      q.mark = 1; // With anything DCAV3 is needed for, the tag travels too.
      SCHECK(avatarSchema(q) == 3 && parseAvatar(line(q).c_str(), back) && back.pack == 1);
      continue;
    }
    SCHECK(avatarSchema(q) == 3 && line(q).rfind("DCAV3 ", 0) == 0);
  }

  // Round trips, and what firmware 0.3 - 0.5 makes of each line.
  std::mt19937 rng(20260);
  const char *names[] = {"a", "My buddy", "Sixteen chars ok", "x_y-z 9", "Scout 3 2", "0"};
  size_t longest = 0;
  unsigned v2 = 0, v3 = 0;
  for (int i = 0; i < 4000; ++i) {
    AvatarProfile q;
    bool modest = i % 3 == 0; // A third of the sample stays within what DCAV2 can hold.
    q.base = rng() % limits::Bases;
    q.body = rng() & 0xffffff;
    q.accent = rng() & 0xffffff;
    q.eye = rng() & 0xffffff;
    q.eyes = rng() % (modest ? limits::V2Eyes : limits::Eyes);
    q.headwear = rng() % (modest ? limits::V2Headwear : limits::Headwear);
    q.eyewear = rng() % (modest ? limits::V2Eyewear : limits::Eyewear);
    q.neckwear = rng() % (modest ? limits::V2Neckwear : limits::Neckwear);
    if (!modest) {
      q.mark = rng() % limits::Marks;
      q.back = rng() % limits::Backs;
      q.held = rng() % limits::Held;
      q.aura = rng() % limits::Auras;
      q.pack = rng() % limits::Packs;
      q.persona = rng() % limits::Personas;
    }
    strcpy(q.name, names[rng() % 6]);
    std::string text = line(q);
    longest = std::max(longest, text.size());
    AvatarProfile back, kept = q;
    if (avatarSchema(q) == 2)
      kept.pack = 0; // A pack tag on an otherwise classic design is not written.
    SCHECK(parseAvatar(text.c_str(), back) && same(kept, back) && back.schema == avatarSchema(q));
    SCHECK(line(back) == text);
    LegacyProfile old;
    if (avatarSchema(q) == 2) {
      // Plain designs still load on old firmware, with every field intact.
      ++v2;
      SCHECK(legacyParse(text.c_str(), old));
      SCHECK(old.base == q.base && old.body == q.body && old.accent == q.accent &&
             old.eyes == q.eyes && old.eye == q.eye && old.headwear == q.headwear &&
             old.eyewear == q.eyewear && old.neckwear == q.neckwear && !strcmp(old.name, q.name));
    } else {
      // Anything else is refused outright by old firmware, never half-applied.
      ++v3;
      SCHECK(!legacyParse(text.c_str(), old));
    }
  }
  SCHECK(v2 > 1000 && v3 > 2000);
  // The longest line there can be, and the buffers it has to fit: SD (128) and USB serial
  // (160, less "AVATAR SET ").
  AvatarProfile widest;
  widest.base = limits::Bases - 1;
  widest.body = widest.accent = widest.eye = 0xffffff;
  widest.eyes = limits::Eyes - 1;
  widest.headwear = limits::Headwear - 1;
  widest.eyewear = limits::Eyewear - 1;
  widest.neckwear = limits::Neckwear - 1;
  widest.mark = limits::Marks - 1;
  widest.back = limits::Backs - 1;
  widest.held = limits::Held - 1;
  widest.aura = limits::Auras - 1;
  widest.pack = limits::Packs - 1;
  widest.persona = limits::Personas - 1;
  strcpy(widest.name, "Sixteen chars ok");
  SCHECK(line(widest).size() == 66 && longest <= 66);
  SCHECK(66 <= limits::ProfileChars && limits::ProfileChars < 128 - 1 &&
         limits::ProfileChars + strlen("AVATAR SET ") < 160 - 1);
  char tight[66];
  SCHECK(!formatAvatar(widest, tight, sizeof(tight))); // One byte short: refused, not truncated.
  // A writer never emits a profile the reader would refuse.
  AvatarProfile bad = widest;
  strcpy(bad.name, " lead");
  SCHECK(!formatAvatar(bad, tight, sizeof(tight)));
  bad = widest;
  bad.body = 0x1000000;
  char text[128];
  SCHECK(!formatAvatar(bad, text, sizeof(text)));

  // The sample profile shipped for the SD card loads on this firmware, as a DCAV3 line.
  {
    std::ifstream sample("sdcard/dashchan/avatar.example.cfg");
    std::string text;
    SCHECK(std::getline(sample, text));
    AvatarProfile q;
    SCHECK(parseAvatar(text.c_str(), q) && q.schema == 3 && line(q) == text);
  }
  // Saved numbers from elsewhere (NVS, a newer build) fall back to "none", never wrap.
  SCHECK(inRange(0, 6) == 0 && inRange(5, 6) == 5 && inRange(6, 6) == 0 && inRange(255, 6) == 0);
  // Persona: a custom buddy can borrow another shape's lines; presets never do.
  for (unsigned base = 0; base < limits::Bases; ++base) {
    SCHECK(speaker(true, 0, base) == base && speaker(false, 3, base) == base);
    for (unsigned persona = 1; persona < limits::Personas; ++persona)
      SCHECK(speaker(true, persona, base) == persona - 1);
    SCHECK(speaker(true, limits::Personas, base) == base && speaker(true, 255, base) == base);
  }
  static_assert(limits::Personas == limits::Bases + 1, "one persona per shape, plus 'match'");
  static_assert(limits::Schema == 3, "STATUS reports avatar_schema 3");
}

inline int luma(uint16_t c) { // The same weighting art::dark and art::pale use.
  return ((c >> 11) & 31) * 2 + ((c >> 5) & 63) * 2 + (c & 31);
}
inline void packs() {
  // Every pack is a valid look, has a name that fits a button, and suggests a real world.
  for (unsigned i = 0; i < limits::Packs; ++i) {
    const PackStyle &s = PackStyles[i];
    SCHECK(*s.name && strlen(s.name) <= 14);
    SCHECK(s.body <= 0xffffff && s.accent <= 0xffffff && s.eye <= 0xffffff);
    SCHECK(s.eyes < limits::Eyes && s.headwear < limits::Headwear && s.eyewear < limits::Eyewear &&
           s.neckwear < limits::Neckwear && s.mark < limits::Marks && s.back < limits::Backs &&
           s.held < limits::Held && s.aura < limits::Auras && s.backdrop < 4);
    AvatarProfile p;
    strcpy(p.name, "Keep me");
    p.base = 3;
    p.persona = 2;
    p.eyewear = 5;
    applyPack(p, i);
    // A pack restyles the buddy but never renames it, reshapes it or changes its voice.
    SCHECK(!strcmp(p.name, "Keep me") && p.base == 3 && p.persona == 2 && p.pack == i);
    SCHECK(p.body == s.body && p.eyes == s.eyes && p.eyewear == s.eyewear && p.aura == s.aura);
    AvatarProfile back;
    SCHECK(parseAvatar(line(p).c_str(), back) && same(p, back));
    // The eyes must show in every shape: on the body, or on Orbit's dark screen. Only Orbit
    // may need the renderer's help (a dark eye colour lifted off its screen).
    for (int base = 0; base < 5; ++base) {
      uint16_t drawn = eyeInk(base, true, rgb565(s.eye), rgb565(s.body));
      uint16_t surface = base == 2 ? 0x10c7 : rgb565(s.body);
      SCHECK(base == 2 || drawn == rgb565(s.eye));
      SCHECK(abs(luma(drawn) - luma(surface)) >= 60);
    }
  }
  SCHECK(PackStyles[0].eyes == 0 && PackStyles[0].aura == 0 && PackStyles[0].back == 0);
  AvatarProfile p;
  p.eyes = 3;
  applyPack(p, limits::Packs); // Out of range: nothing happens.
  SCHECK(p.eyes == 3 && p.pack == 0);
}

inline void outfitCards() {
  static_assert(sizeof(OutfitCardV1) == 34 && sizeof(OutfitCard) == 40,
                "stored NFC outfit sizes are how the two versions are told apart");
  OutfitCardV1 v1{};
  v1.version = 1;
  v1.avatar = 3;
  v1.base = 4;
  v1.eyes = 5;
  v1.head = 5;
  v1.glasses = 3;
  v1.neck = 3;
  v1.theme = 2;
  v1.scene = 1;
  v1.body = 0x1234;
  v1.accent = 0xabcd;
  v1.eye = 0x0f0f;
  strcpy(v1.name, "Card buddy");
  OutfitCard o;
  // A card linked by firmware 0.5 still dresses the buddy; the new slots start empty.
  SCHECK(readOutfitCard(&v1, sizeof(v1), o));
  SCHECK(o.version == 2 && o.avatar == 3 && o.base == 4 && o.eyes == 5 && o.head == 5 &&
         o.glasses == 3 && o.neck == 3 && o.theme == 2 && o.scene == 1 && o.body == 0x1234 &&
         o.accent == 0xabcd && o.eye == 0x0f0f && !strcmp(o.name, "Card buddy"));
  SCHECK(!o.mark && !o.back && !o.held && !o.aura && !o.pack && !o.persona);
  // Today's card round-trips with everything.
  OutfitCard now = o;
  now.eyes = 9;
  now.head = 11;
  now.glasses = 7;
  now.neck = 7;
  now.mark = 7;
  now.back = 6;
  now.held = 5;
  now.aura = 6;
  now.pack = 5;
  now.persona = 5;
  OutfitCard read;
  SCHECK(readOutfitCard(&now, sizeof(now), read) && !memcmp(&read, &now, sizeof(now)));
  // Refusals: wrong size, wrong version for its size, any field out of range, unterminated name.
  uint8_t raw[64]{};
  for (size_t size : {size_t(0), size_t(1), sizeof(v1) - 1, sizeof(v1) + 1, sizeof(now) - 1,
                      sizeof(now) + 1, sizeof(raw)})
    SCHECK(!readOutfitCard(raw, size, read));
  OutfitCardV1 wrong = v1;
  wrong.version = 2;
  SCHECK(!readOutfitCard(&wrong, sizeof(wrong), read));
  wrong = v1;
  wrong.eyes = 6; // Old cards never held the new eye styles.
  SCHECK(!readOutfitCard(&wrong, sizeof(wrong), read));
  OutfitCard bad = now;
  bad.version = 1;
  SCHECK(!readOutfitCard(&bad, sizeof(bad), read));
  struct Field {
    uint8_t OutfitCard::*field;
    unsigned count;
  };
  const Field fields[] = {
      {&OutfitCard::avatar, 6},
      {&OutfitCard::base, limits::Bases},
      {&OutfitCard::eyes, limits::Eyes},
      {&OutfitCard::head, limits::Headwear},
      {&OutfitCard::glasses, limits::Eyewear},
      {&OutfitCard::neck, limits::Neckwear},
      {&OutfitCard::theme, 4},
      {&OutfitCard::scene, 4},
      {&OutfitCard::mark, limits::Marks},
      {&OutfitCard::back, limits::Backs},
      {&OutfitCard::held, limits::Held},
      {&OutfitCard::aura, limits::Auras},
      {&OutfitCard::pack, limits::Packs},
      {&OutfitCard::persona, limits::Personas},
  };
  for (const Field &f : fields) {
    bad = now;
    bad.*f.field = uint8_t(f.count - 1);
    SCHECK(readOutfitCard(&bad, sizeof(bad), read));
    bad.*f.field = uint8_t(f.count);
    SCHECK(!readOutfitCard(&bad, sizeof(bad), read));
    bad.*f.field = 255;
    SCHECK(!readOutfitCard(&bad, sizeof(bad), read));
  }
  bad = now;
  memset(bad.name, 'x', sizeof(bad.name));
  SCHECK(!readOutfitCard(&bad, sizeof(bad), read));
  SCHECK(!memcmp(&read, &now, sizeof(now))); // Refusals never touch the output.
}

inline void art() {
  struct Gear {
    const char *name;
    int Look::*slot;
    unsigned count;
  };
  const Gear gear[] = {
      {"eyes", &Look::eyes, limits::Eyes},          {"headwear", &Look::headwear, limits::Headwear},
      {"eyewear", &Look::eyewear, limits::Eyewear}, {"neckwear", &Look::neckwear, limits::Neckwear},
      {"mark", &Look::mark, limits::Marks},         {"back", &Look::back, limits::Backs},
      {"held", &Look::held, limits::Held},          {"aura", &Look::aura, limits::Auras},
  };
  static_assert(sizeof(motes) / sizeof(motes[0]) == limits::AuraParticles,
                "an aura draws one particle per row of the motes table");
  for (int base = 0; base < 5; ++base) {
    Look bare = customLook(base);
    Look preset;
    preset.base = base;
    for (const Gear &g : gear) {
      std::set<std::string> seen;
      for (unsigned option = 0; option < g.count; ++option) {
        Look look = bare;
        look.*g.slot = int(option);
        // Every option draws, and draws something of its own: no two options of a slot may
        // come out identical (a missing branch would silently repeat "none").
        Tracer t;
        drawBuddy(t, look, 0, 0);
        std::string print;
        for (const Blot &i : t.inks)
          print += std::string(1, i.kind) + std::to_string(i.x0) + "," + std::to_string(i.y0) +
                   "," + std::to_string(i.x1) + "," + std::to_string(i.y1) + "," +
                   std::to_string(i.c) + ";";
        SCHECK(seen.insert(print).second);
        // Gear stays on the Recon stage: 91 px either side of the buddy and 66 px above it,
        // so nothing is cut off by the NEARBY panel or the top bar.
        // Rising aura particles may leave by the top, where the stage clips them.
        for (const Blot &i : t.inks)
          SCHECK(i.x0 >= -91 && i.x1 <= 91 && i.y1 <= 60 && (i.y0 >= -68 || g.slot == &Look::aura));
        // Presets wear nothing, whatever a stale setting says.
        Look dressed = preset;
        dressed.*g.slot = int(option);
        if (g.slot != &Look::eyes)
          SCHECK(inkCount(dressed) == inkCount(preset));
        // No zoom level may hand the display driver an oversized corner radius or a
        // zero-sized box.
        for (float k : {0.62f, 1.0f, 1.6f, 2.1f}) {
          Tracer z;
          art::Zoom<Tracer> zoom{z, 100, 100, k};
          for (int mood = 0; mood < 7; ++mood) {
            Pose pose;
            pose.mood = mood;
            pose.phase = mood * 5 + int(option);
            drawBuddy(zoom, look, 0, 0, pose);
          }
          for (const Blot &i : z.inks)
            if (i.kind == 'r')
              SCHECK(i.w >= 1 && i.h >= 1 && i.r >= 0 && 2 * i.r <= i.w && 2 * i.r <= i.h);
        }
      }
    }
    // The aura is bounded: at most AuraParticles particles, however long it runs. Each
    // particle is at most three shapes, drawn once true and once as a thin edge.
    for (int aura = 1; aura < int(limits::Auras); ++aura)
      for (int phase = 0; phase < 64; ++phase) {
        Look look = bare;
        Pose pose;
        pose.phase = phase;
        size_t without = inkCount(look, pose);
        look.aura = aura;
        size_t with = inkCount(look, pose);
        SCHECK(with > without && with - without <= limits::AuraParticles * 3 * 2);
      }
    // The field kit takes the place of whatever the buddy was holding.
    for (int kit : {1, 2}) {
      Pose working;
      working.kit = kit;
      Look holding = bare;
      holding.held = 3;
      SCHECK(inkCount(holding, working) == inkCount(bare, working));
      SCHECK(inkCount(bare, working) > inkCount(bare));
    }
  }
  // Glasses lift onto the forehead for love, alarm and cheers, whichever pair it is.
  for (int eyewear = 1; eyewear < int(limits::Eyewear); ++eyewear) {
    auto top = [&](int mood) {
      Tracer a;
      art::Pen<Tracer> pa{a, 0, 0, 0, 0};
      buddyEyewear(pa, eyewear, mood == 3 || mood == 4 || mood == 5 ? -25 : -4, Blush, Ink, 0);
      int y = INT_MAX;
      for (const Blot &i : a.inks)
        y = std::min(y, i.y0);
      return y;
    };
    SCHECK(top(3) == top(0) - 21 && top(4) == top(3) && top(5) == top(3) && top(6) == top(0));
  }
  // A hoop is a closed loop of strokes at any zoom: each stroke starts where the last ended.
  for (float k : {1.0f, 1.6f, 2.1f}) {
    Tracer t;
    art::Zoom<Tracer> zoom{t, 0, 0, k};
    art::Pen<art::Zoom<Tracer>> pen{zoom, 0, 0, 0, 0};
    hoop(pen, 21, -4, 13, 14, Blush, 2);
    std::vector<Blot> caps;
    for (const Blot &i : t.inks)
      if (i.kind == 'c')
        caps.push_back(i);
    SCHECK(caps.size() == 48);
    for (size_t i = 0; i < 24; ++i) {
      const Blot &end = caps[i * 2 + 1], &next = caps[(i * 2 + 2) % 48];
      SCHECK(end.x0 == next.x0 && end.y0 == next.y0 && end.r >= 1);
    }
  }
  // Full Face: any custom hat pulls the camera back so the whole hat, outline included, and the
  // mouth (down to y 25) stay on the stage (screen y 24..174).
  for (int hat = 0; hat < int(limits::Headwear); ++hat) {
    Look look = customLook();
    look.headwear = hat;
    FaceFit fit = faceFit(look);
    SCHECK(hat ? fit.k == 1.55f : fit.k == 2.1f);
    SCHECK(fit.oy + 25 * fit.k <= 174);
    Tracer t;
    art::Pen<Tracer> pen{t, 0, 0, 2, 0};
    for (int phase = 0; phase < 16; ++phase)
      buddyHead(pen, hat, Blush, Ink, phase);
    for (const Blot &i : t.inks)
      SCHECK(fit.oy + i.y0 * fit.k >= 24);
  }
  // Eye colours: a dull colour on a dark face is lifted, keeping its hue; extremes still swap.
  SCHECK(art::dim(0x6a14) && !art::dim(Mint) && !art::dim(0xfd20) && art::dim(Ink));
  SCHECK(eyeInk(2, true, 0x6a14, Lilac) == art::mix(0x6a14, 0xffff, 1, 2));  // Orbit's screen.
  SCHECK(eyeInk(0, true, 0x6a14, 0x0000) == art::mix(0x6a14, 0xffff, 1, 2)); // A black body.
  SCHECK(eyeInk(0, true, 0x6a14, Paper) == 0x6a14);                          // Fine on cream.
  SCHECK(eyeInk(2, true, Ink, Lilac) == Mint && eyeInk(0, true, Ink, 0x0000) == 0xefff);
  SCHECK(eyeInk(0, true, 0xffff, Paper) == Ink && eyeInk(2, false, 0x6a14, Lilac) == Mint);
}

// Writes what the firmware's art does for a spread of looks. tests/studio_test.mjs replays the
// same looks through the generated site/buddy_art.mjs and expects the very same calls.
inline void trace() {
  std::ofstream file("build/art-trace.txt");
  const uint32_t bodies[] = {0xfff3d2, 0x17141f, 0xa5edc0, 0x1a1433, 0xff9ad5, 0x000000, 0xffffff};
  const uint32_t accents[] = {0xff88b6, 0xb3123c, 0x33ff66, 0x35e6d0, 0xffe040};
  const uint32_t eyes[] = {0x101029, 0xf2e9ff, 0x2bf5ff, 0x000000, 0xffffff, 0x4b2a9a};
  for (int n = 0; n < 480; ++n) {
    Look look;
    look.base = n % 5;
    look.custom = n % 9 != 8;
    look.body = rgb565(bodies[n % 7]);
    look.accent = rgb565(accents[n % 5]);
    look.eye = rgb565(eyes[n % 6]);
    look.eyes = n % int(limits::Eyes);
    look.headwear = n % int(limits::Headwear);
    look.eyewear = n * 3 % int(limits::Eyewear);
    look.neckwear = n * 5 % int(limits::Neckwear);
    look.mark = n * 7 % int(limits::Marks);
    look.back = n * 3 % int(limits::Backs);
    look.held = n * 5 % int(limits::Held);
    look.aura = n * 2 % int(limits::Auras);
    int mood = n % 7, gaze = n % 11 - 5, kit = n % 4 == 3 ? 1 + n % 2 : 0, phase = n * 7 % 97;
    bool blink = n % 13 == 0;
    file << "look " << look.base << ' ' << look.custom << ' ' << look.body << ' ' << look.accent
         << ' ' << look.eye << ' ' << look.eyes << ' ' << look.headwear << ' ' << look.eyewear
         << ' ' << look.neckwear << ' ' << look.mark << ' ' << look.back << ' ' << look.held << ' '
         << look.aura << ' ' << mood << ' ' << gaze << ' ' << blink << ' ' << kit << ' ' << phase
         << '\n';
    Tones t = buddyTones(look.base, look.custom, look.body, look.accent);
    Scribe p{file, "p"}, rim{file, "rim"}, edge{file, "edge"};
    buddyArt(p, rim, edge, look, t, mood, gaze, blink, kit, phase);
  }
  SCHECK(file.good());
}

// The on-device studio: what is drawn is what listens, and every label fits its button.
inline void deviceStudio() {
  namespace sv = studioview;
  std::vector<sv::Box> boxes;
  for (int row = 0; row < sv::Rows; ++row)
    boxes.push_back(sv::option(row));
  for (int page = 0; page < sv::Pages; ++page)
    boxes.push_back(sv::tab(page));
  boxes.push_back(sv::Done);
  boxes.push_back(sv::Preview);
  for (size_t i = 0; i < boxes.size(); ++i) {
    const sv::Box &a = boxes[i];
    // On the screen, below the heading and its BACK button (y < 34), and big enough to tap.
    SCHECK(a.x >= 0 && a.y >= 36 && a.x + a.w <= 320 && a.y + a.h <= 240 && a.w >= 60 && a.h >= 32);
    for (size_t j = i + 1; j < boxes.size(); ++j) {
      const sv::Box &b = boxes[j];
      SCHECK(a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y);
    }
  }
  // Every pixel of the screen lands on exactly the control drawn there, or on nothing.
  for (int y = 0; y < 240; ++y)
    for (int x = 0; x < 320; ++x) {
      sv::Target t = sv::tap(x, y);
      sv::Hit want = sv::Hit::None;
      int index = 0;
      for (int row = 0; row < sv::Rows; ++row)
        if (sv::option(row).has(x, y)) {
          want = sv::Hit::Option;
          index = row;
        }
      for (int page = 0; page < sv::Pages; ++page)
        if (sv::tab(page).has(x, y)) {
          want = sv::Hit::Tab;
          index = page;
        }
      if (sv::Done.has(x, y))
        want = sv::Hit::Done;
      SCHECK(t.hit == want && t.index == index);
      SCHECK(!sv::Preview.has(x, y) || t.hit == sv::Hit::None);
    }
  // Each field appears on exactly one page, and every choice has a name that fits: a button
  // is 172 px wide, 6 px a character.
  std::set<int> placed;
  for (int page = 0; page < sv::Pages; ++page) {
    SCHECK(strlen(sv::PageNames[page]) * 6 <= size_t(sv::tab(page).w - 8));
    for (int row = 0; row < sv::Rows; ++row) {
      sv::Field f = sv::PageFields[page][row];
      SCHECK(placed.insert(int(f)).second);
      std::set<std::string> names;
      for (unsigned v = 0; v < sv::choices(f); ++v) {
        char text[64];
        sv::label(text, sizeof(text), f, v);
        SCHECK(strlen(text) <= sv::ButtonChars &&
               strlen(text) * 6 <= size_t(sv::option(row).w - 4));
        SCHECK(*sv::name(f, v) && names.insert(sv::name(f, v)).second);
        for (const char *c = text; *c; ++c)
          SCHECK(*c >= 32 && *c < 127); // The LCD font is ASCII only.
        SCHECK(sv::next(f, v) == (v + 1) % sv::choices(f));
      }
      SCHECK(!strcmp(sv::name(f, sv::choices(f)), "?"));
    }
  }
  SCHECK(placed.size() == 12);
  SCHECK(sv::choices(sv::Field::Head) == limits::Headwear &&
         sv::choices(sv::Field::Persona) == limits::Personas &&
         sv::choices(sv::Field::Pack) == limits::Packs && sv::choices(sv::Field::Kit) == 3);
  // The pack names on the robot are the pack names everywhere else ("None" reads "My own look").
  for (unsigned i = 1; i < limits::Packs; ++i)
    SCHECK(!strcmp(sv::PackNames[i], PackStyles[i].name));
  // The preview is big enough for the whole buddy at the scale the studio draws it, hats and
  // wings included: 88 units either side, 68 above and 53 below its centre at y + 72, with
  // room under it for the name.
  SCHECK(sv::Preview.w / 2 >= int(88 * sv::PreviewScale) && 72 >= int(68 * sv::PreviewScale) + 1 &&
         sv::Preview.h - 72 >= int(53 * sv::PreviewScale) + 18);
}

inline void run() {
  deviceStudio();
  profiles();
  packs();
  outfitCards();
  art();
  trace();
}
} // namespace studio_tests
