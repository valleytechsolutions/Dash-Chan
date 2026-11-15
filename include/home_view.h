// SPDX-License-Identifier: GPL-3.0-or-later
// The home screen, drawn from a plain snapshot of state. It is a template so the firmware
// (M5Canvas) and the PC preview (tools/preview.cpp) render exactly the same layout.
//
//   0- 23  top bar: buddy name and rank, GPS, recording state
//  24-175  stage: backdrop, buddy, feeling tag, speech bubble (+ NEARBY clues in Recon view)
// 178-203  vitals: FOOD, LOVE and XP meters, always visible
// 208-236  buttons
#pragma once
#include "avatars.h"
#include "personality.h"
#include "progression.h"
#include "themes.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace dash {
struct BuddyLook {
  int base = 0; // 0 Mochi, 1 Sprout, 2 Orbit, 3 Wisp, 4 Ember.
  bool custom = false;
  uint16_t body = Paper, accent = Blush, eye = Ink;
  int eyes = 0, headwear = 0, eyewear = 0, neckwear = 0;
};
struct HomeClue {
  const char *family = "";
  bool ble = false, fresh = false;
  int rssi = 0;
  const char *confidence = "";
};
struct HomeView {
  uint32_t now = 0;
  Palette colors{};
  Theme theme = Theme::Dark;
  unsigned backdrop = 0;
  bool face = false;
  // Buddy.
  BuddyLook look;
  const char *name = "Mochi";
  int mood = 0, gaze = 0, tiltY = 0, kit = 0; // kit: 0 none, 1 holding gear, 2 cruising.
  Feeling feeling = Feeling::Content;
  unsigned food = 85, love = 70, xp = 0, level = 1;
  // Speech and events. Ages are milliseconds since the event; UINT32_MAX means "not active".
  const char *message = "";
  bool speaking = false;
  uint32_t discoveryAge = UINT32_MAX, levelAge = UINT32_MAX;
  bool alert = false, demo = false;
  const char *alertFamily = "", *alertConfidence = "";
  // Status.
  const char *state = "WATCHING";
  bool stateUrgent = false, recording = false, moving = false, fix = false;
  unsigned sats = 0;
  uint32_t wifiSaved = 0, bleSaved = 0;
  HomeClue clues[3];
  unsigned clueCount = 0;
};

namespace homeui {
using art::mix;
template <class G> void text(G &g, const char *s, int x, int y, uint16_t color, int size = 1) {
  g.setTextSize(size);
  g.setTextColor(color);
  g.setCursor(x, y);
  g.print(s);
}
template <class G> void centered(G &g, const char *s, int cx, int y, uint16_t color) {
  text(g, s, cx - int(strlen(s)) * 3, y, color);
}
template <class G>
void button(G &g, const HomeView &v, int x, int y, int w, int h, const char *label,
            bool active = false) {
  g.fillRoundRect(x, y, w, h, 9, active ? v.colors.accent : v.colors.panel);
  centered(g, label, x + w / 2, y + h / 2 - 3,
           active ? (v.theme == Theme::Light ? 0xffff : Ink) : v.colors.text);
}
template <class G> void heart(G &g, int x, int y, uint16_t c) {
  g.fillCircle(x - 3, y - 2, 4, c);
  g.fillCircle(x + 3, y - 2, 4, c);
  g.fillTriangle(x - 7, y, x + 7, y, x, y + 8, c);
}
template <class G> void star(G &g, int x, int y, uint16_t c, int size = 2) {
  g.fillRect(x - size * 2, y - size / 2, size * 5, size, c);
  g.fillRect(x, y - size * 2, size, size * 5, c);
}
template <class G> void cloud(G &g, int x, int y, uint16_t c) {
  g.fillRoundRect(x, y, 39, 9, 4, c);
  g.fillRoundRect(x + 8, y - 8, 17, 16, 5, c);
  g.fillRoundRect(x + 22, y - 4, 12, 11, 4, c);
}
// The little antenna-head logo beside the buddy's name.
template <class G> void mark(G &g, const Palette &colors, int x, int y, int scale = 1) {
  g.drawLine(x + 12 * scale, y + 3 * scale, x + 15 * scale, y - 2 * scale, colors.accent);
  g.fillCircle(x + 15 * scale, y - 2 * scale, 2 * scale, colors.danger);
  g.fillRoundRect(x, y + 2 * scale, 18 * scale, 15 * scale, 5 * scale, colors.accent);
  g.fillRect(x + 4 * scale, y + 6 * scale, 2 * scale, 4 * scale, Ink);
  g.fillRect(x + 11 * scale, y + 6 * scale, 2 * scale, 4 * scale, Ink);
  g.drawLine(x + 7 * scale, y + 13 * scale, x + 10 * scale, y + 13 * scale, Ink);
}
// Decorative worlds, never a map or a signal direction. `width` is the visible stage width.
template <class G> void scene(G &g, const HomeView &v, int width) {
  static constexpr uint32_t skies[][3] = {{0x25236b, 0x6553c6, 0x24dfd0},
                                          {0x812e92, 0xf78b78, 0xffd75e},
                                          {0x17275d, 0x4e3fa1, 0x87d5ff},
                                          {0x137e91, 0x69c5b5, 0xe0ff91}};
  unsigned world = v.backdrop % 4;
  uint32_t now = v.now;
  uint16_t a = rgb565(skies[world][0]), b = rgb565(skies[world][1]);
  uint16_t pop = rgb565(skies[world][2]), pink = rgb565(0xff79c6);
  uint16_t floor = rgb565(0x2a2859), rail = rgb565(0x68f9d0);
  if (v.theme == Theme::Light) {
    a = mix(a, 0xffff, 2, 5);
    b = mix(b, 0xffff, 1, 5);
  }
  if (v.theme == Theme::Matrix) {
    a = rgb565(0x061a11);
    b = rgb565(0x125333);
    pop = rgb565(0x88ff94);
    pink = rgb565(0x33bb70);
    floor = rgb565(0x082a19);
  }
  if (v.theme == Theme::Bwm) {
    a = rgb565(0x351537);
    b = rgb565(0x9d2948);
    pop = rgb565(0xffbc6d);
    pink = rgb565(0xff5d79);
    rail = rgb565(0xff6f82);
  }
  for (int y = 24; y < 240; y += 2)
    g.fillRect(0, y, 320, 2, mix(a, b, y - 24, 216));
  g.setClipRect(0, 24, width, 151);
  int scroll = v.moving ? int(now / 100) % width : 0;
  int right = width - 182; // Anchor right-hand props to the stage edge.
  if (world == 0) {
    // Neon rooftops and a tiny listening tower.
    g.fillCircle(137 + right, 56, 19, pink);
    int span = width + 68, count = span / 25;
    for (int i = 0; i < count; ++i) {
      int x = (i * 25 + width - scroll) % span - 30, y = 86 + i * 13 % 37;
      g.fillRect(x, y, 21, 49, mix(a, floor, 2, 3));
      g.fillRect(x, y, 21, 2, i % 2 ? pop : pink);
      for (int k = 0; k < 3; ++k)
        for (int j = 0; j < 2; ++j)
          g.fillRect(x + 4 + j * 9, y + 7 + k * 11, 4, 4,
                     (i + j + k) % 3 ? mix(a, pop, 2, 5) : pop);
    }
    for (int r : {8, 17, 26})
      g.drawCircle(30, 51, r, mix(a, pop, 1, 2));
    g.drawLine(30, 52, 30, 84, pop);
    g.fillCircle(30, 51, 3, pop);
    star(g, 101, 39, pop, 1);
  } else if (world == 1) {
    g.fillCircle(132 + right, 60, 27, pop);
    for (int y = 63; y < 88; y += 6)
      g.fillRect(103 + right, y, 58, 2, b);
    cloud(g, 11 + int(now / 1500 % 15), 48, mix(b, 0xffff, 1, 2));
    g.fillTriangle(-20, 130, 41, 70, 106, 130, rgb565(0x8143a7));
    g.fillTriangle(71 + right, 132, 143 + right, 91, 216 + right, 132, rgb565(0xb65baf));
    g.fillTriangle(27, 84, 41, 70, 57, 85, pink);
    g.fillRect(0, 123, width, 20, mix(a, pop, 1, 3));
    for (int i = 0; i < 6; ++i)
      g.fillRect(int((i * 37 + now / 180) % (width - 2)), 126 + i % 3 * 5, 14, 1, pop);
  } else if (world == 2) {
    for (int i = 0; i < width / 11; ++i) {
      int x = (i * 47 + 21) % (width - 8) + 4, y = (i * 29 + 9) % 103 + 30;
      uint16_t c = (i + now / 1300) % 4 ? mix(a, pop, 1, 2) : pop;
      if (i % 4)
        g.fillRect(x, y, 2, 2, c);
      else
        star(g, x, y, c, 1);
    }
    g.drawEllipse(139 + right, 56, 30, 9, pink);
    g.fillCircle(139 + right, 54, 18, pink);
    g.fillEllipse(133 + right, 48, 9, 5, mix(pink, 0xffff, 1, 2));
    g.fillCircle(146 + right, 62, 5, mix(pink, a, 1, 3));
    g.drawLine(110 + right, 60, 165 + right, 52, pop);
    g.fillCircle(22, 119, 23, mix(a, pop, 1, 3));
    g.fillCircle(15, 110, 5, mix(a, pop, 1, 5));
  } else {
    cloud(g, 111 + right - int(now / 1700 % 14), 49, rgb565(0xe4ffe1));
    cloud(g, 9, 66, mix(pop, 0xffff, 1, 2));
    g.fillCircle(31, 39, 13, rgb565(0xffe878));
    g.fillCircle(42, 140, 48, rgb565(0x2caa89));
    g.fillCircle(135 + right, 145, 52, rgb565(0x4ec599));
    if (right)
      g.fillCircle(160, 150, 46, rgb565(0x3bb891));
    for (int x : {12, 164 + right}) {
      g.fillRect(x, 109, 3, 25, rgb565(0x1e735d));
      g.fillEllipse(x - 4, 118, 6, 3, pop);
      g.fillRoundRect(x - 8, 102, 19, 11, 3, pink);
      g.fillRect(x - 3, 99, 9, 17, pink);
      g.fillRect(x - 1, 106, 5, 5, rgb565(0xffeb91));
    }
  }
  g.fillRect(0, 137, width, 38, floor);
  g.fillRect(0, 137, width, 3, rail);
  for (int x = -15; x < width + 10; x += 24) {
    int offset = v.moving ? int(now / 80 % 24) : 0;
    g.drawLine(x - offset, 141, x - offset - 12, 175, mix(floor, pop, 1, 4));
  }
  g.drawLine(0, 157, width, 157, mix(floor, pop, 1, 4));
  g.clearClipRect();
}
template <class G> void confetti(G &g, uint32_t elapsed, bool level, int width) {
  const uint16_t candy[] = {rgb565(0xffd569), rgb565(0x79f8dd), rgb565(0xff7ebd), rgb565(0xbca0ff)};
  for (int i = 0; i < (level ? 16 : 7); ++i) {
    int x = 10 + (i * 37 + 17) % (width - 20);
    int y = 35 + int((i * 23 + elapsed / 35) % 94);
    if (i % 3)
      g.fillRect(x, y, 3, 4, candy[i % 4]);
    else
      star(g, x, y, candy[i % 4], 1);
  }
}
// Word-wraps into at most `maxLines` rows of `width` characters. Returns the row count.
inline int wrapLines(const char *s, int width, char rows[][64], int maxLines) {
  int count = 0;
  while (*s && count < maxLines) {
    while (*s == ' ')
      ++s;
    int n = int(strlen(s));
    if (n > width) {
      n = width;
      while (n > 0 && s[n] != ' ')
        --n;
      if (n <= 0)
        n = width;
    }
    if (n > 63)
      n = 63;
    if (!n)
      break;
    memcpy(rows[count], s, n);
    rows[count++][n] = 0;
    s += n;
  }
  return count;
}
// A real speech bubble with its tail aimed at the buddy, so words read as the pet talking.
template <class G>
void bubble(G &g, const HomeView &v, const char *msg, int left, int right, int bottom, int tailX) {
  char rows[3][64];
  int chars = (right - left - 12) / 6;
  int lines = wrapLines(msg, chars, rows, 3);
  if (!lines)
    return;
  int widest = 0;
  for (int i = 0; i < lines; ++i)
    widest = widest > int(strlen(rows[i])) ? widest : int(strlen(rows[i]));
  int w = widest * 6 + 14, h = lines * 11 + 9;
  int x = tailX - w / 2;
  x = x < left ? left : x + w > right ? right - w : x;
  int y = bottom - h;
  uint16_t fill = v.alert ? v.colors.danger : v.theme == Theme::Light ? 0xffff : v.colors.text;
  uint16_t ink = v.alert || v.theme != Theme::Light ? Ink : v.colors.text;
  uint16_t edge = v.theme == Theme::Light ? v.colors.muted : Ink;
  g.fillRoundRect(x - 1, y - 1, w + 2, h + 2, 8, edge);
  g.fillTriangle(tailX - 7, y, tailX + 7, y, tailX + 1, y - 10, edge);
  g.fillRoundRect(x, y, w, h, 7, fill);
  g.fillTriangle(tailX - 5, y + 1, tailX + 5, y + 1, tailX + 1, y - 7, fill);
  for (int i = 0; i < lines; ++i)
    text(g, rows[i], x + 7, y + 6 + i * 11, ink);
}
template <class G> void snooze(G &g, const HomeView &v, int x, int y) {
  for (int i = 0; i < 3; ++i) {
    int rise = int((v.now / 110 + i * 9) % 27);
    int zx = x + i * 9 + rise / 5, zy = y - i * 7 - rise / 3, size = i == 2 ? 2 : 1;
    text(g, i == 2 ? "Z" : "z", zx + 1, zy + 1, Ink, size);
    text(g, i == 2 ? "Z" : "z", zx, zy, 0xffff, size);
  }
}
// Tiny pictograms for the vitals row.
template <class G> void snackIcon(G &g, int x, int y, uint16_t c, uint16_t bg) {
  g.fillCircle(x, y, 6, c); // A cookie with a bite out of it.
  g.fillCircle(x + 5, y - 5, 3, bg);
  g.fillRect(x - 3, y - 1, 2, 2, bg);
  g.fillRect(x + 1, y + 2, 2, 2, bg);
  g.fillRect(x - 1, y - 4, 2, 2, bg);
}
template <class G> void starIcon(G &g, int x, int y, uint16_t c) {
  g.fillTriangle(x, y - 7, x - 4, y + 5, x + 6, y - 2, c);
  g.fillTriangle(x, y - 7, x + 4, y + 5, x - 6, y - 2, c);
  g.fillTriangle(x - 6, y - 2, x + 6, y - 2, x, y + 3, c);
}
template <class G>
void vital(G &g, const HomeView &v, int x, const char *label, unsigned value, unsigned max,
           uint16_t color, bool low) {
  const int y = 180, barX = x + 21, barW = 74;
  bool flash = low && v.now / 500 % 2;
  uint16_t c = low ? v.colors.danger : color;
  text(g, label, barX, y + 2, low ? v.colors.danger : v.colors.muted);
  g.fillRoundRect(barX, y + 13, barW, 7, 3, v.colors.panel);
  int fill = int(uint64_t(value > max ? max : value) * barW / max);
  if (fill > 0 && !flash)
    g.fillRoundRect(barX, y + 13, fill < 6 ? 6 : fill, 7, 3, c);
}
} // namespace homeui

inline uint16_t feelingColor(const Palette &c, Feeling f) {
  return f == Feeling::Hungry || f == Feeling::Lonely ? c.danger
         : f == Feeling::Hyped                        ? c.accent
         : f == Feeling::Content                      ? c.accent
                                                      : c.muted;
}

template <class G>
void drawBuddyLook(G &g, const BuddyLook &l, int x, int y, int mood, int gaze, bool blink, int bob,
                   int kit, int phase) {
  drawBuddy(g, l.base, x, y, mood, gaze, blink, bob, l.body, l.accent, 0, l.custom,
            l.custom ? l.eyes : 0, l.eye, l.custom ? l.headwear : 0, l.custom ? l.eyewear : 0,
            l.custom ? l.neckwear : 0, kit, phase);
}

template <class G> void drawHome(G &g, const HomeView &v) {
  using namespace homeui;
  char s[96];
  const Palette &colors = v.colors;
  uint32_t now = v.now;
  int stage = v.face ? 320 : 182;
  scene(g, v, stage);
  bool cheering = !v.alert && v.discoveryAge < 1100;
  bool levelUp = !v.alert && v.levelAge < 5000;
  bool napping = v.feeling == Feeling::Napping && !v.alert;
  int expression = cheering ? 5 : v.mood;
  int bob = v.alert ? int(cos(now / 90.0) * 3) : int(sin(now / 820.0) * 2);
  if (cheering)
    bob -= int(fabs(sin(v.discoveryAge / 180.0)) * 5);
  int gaze = v.alert ? int(sin(now / 100.0) * 5) : v.gaze;
  bool blink = !v.alert && now % 4100 < 130;
  int phase = int(now / 150);
  int mouthX, bubbleLeft, bubbleRight;
  g.setClipRect(0, 24, stage, 151);
  if (v.face) {
    // Close-up: the same vector buddy redrawn large, so edges stay crisp.
    art::Zoom<G> zoom{g, 160, 96 + bob * 2, 2.1f};
    drawBuddyLook(zoom, v.look, 0, 0, expression, gaze, blink, 0, 0, phase);
    mouthX = 160;
    bubbleLeft = 10;
    bubbleRight = 310;
    if (v.mood == 3 && !v.alert) {
      heart(g, 38, 70 - int(now / 120 % 12), colors.heart);
      heart(g, 284, 82 - int(now / 140 % 13), colors.heart);
    }
    if (napping)
      snooze(g, v, 252, 66);
  } else {
    drawBuddyLook(g, v.look, 91, 90 + v.tiltY, expression, gaze, blink, bob, v.kit, phase);
    mouthX = 91;
    bubbleLeft = 5;
    bubbleRight = 178;
    if (v.mood == 3 && !v.alert) {
      heart(g, 26, 63 - int(now / 120 % 12), colors.heart);
      heart(g, 156, 71 - int(now / 140 % 13), colors.heart);
    } else if (v.mood == 1 && !v.alert && !cheering && !levelUp) {
      star(g, 21, 56, colors.accent, 1);
      star(g, 162, 75, colors.heart, 1);
    }
    if (napping)
      snooze(g, v, 132, 52);
  }
  if (cheering || levelUp)
    confetti(g, levelUp ? v.levelAge : v.discoveryAge, levelUp, stage);
  g.clearClipRect();
  // Feeling tag, top-left of the stage. An alert or a cheer takes its place.
  if (v.alert) {
    g.fillRoundRect(6, 29, 21, 22, 5, colors.danger);
    text(g, "!", 11, 33, Ink, 2);
  } else if (cheering || levelUp) {
    const char *badge = levelUp ? "LEVEL UP!" : "+1 XP";
    int w = int(strlen(badge)) * 6 + 12;
    g.fillRoundRect(6, 29, w, 16, 6, colors.accent);
    text(g, badge, 12, 33, Ink);
  } else {
    const char *feeling = feelingName(v.feeling);
    int w = int(strlen(feeling)) * 6 + 22;
    uint16_t c = feelingColor(colors, v.feeling);
    g.fillRoundRect(6, 29, w, 16, 8, colors.bg);
    g.fillCircle(15, 37, 3, c);
    text(g, feeling, 22, 33, colors.text);
  }
  // Drive counters sit under the buddy once there is a drive to count.
  bool counters = v.recording || v.wifiSaved || v.bleSaved;
  if (v.speaking && *v.message)
    bubble(g, v, v.message, bubbleLeft, bubbleRight, 173, mouthX);
  else if (counters) {
    int x0 = v.face ? 77 : 8;
    for (int i = 0; i < 2; ++i) {
      int x = x0 + i * 85;
      uint16_t c = i ? colors.heart : colors.accent;
      g.fillRoundRect(x, 154, 81, 18, 7, colors.bg);
      g.fillCircle(x + 9, 163, 3, c);
      snprintf(s, sizeof(s), "%s ~%lu", i ? "BLE" : "WiFi",
               (unsigned long)(i ? v.bleSaved : v.wifiSaved));
      text(g, s, x + 17, 159, colors.text);
    }
  }
  if (!v.face) {
    g.fillRoundRect(184, 29, 130, 146, 10, colors.bg);
    text(g, "NEARBY", 193, 37, colors.accent);
    if (!v.clueCount) {
      for (int r : {12, 22, 31})
        g.drawCircle(249, 95, r, colors.panel);
      float sweep = now / 1800.0f;
      g.drawLine(249, 95, 249 + int(cos(sweep) * 29), 95 + int(sin(sweep) * 29), colors.accent);
      g.fillCircle(249, 95, 2, colors.accent);
      centered(g, "All quiet", 249, 137, colors.text);
      centered(g, "Clues show up here", 249, 152, colors.muted);
    } else
      text(g, "tap for more", 234, 37, colors.muted);
    for (unsigned i = 0; i < v.clueCount && i < 3; ++i) {
      const HomeClue &c = v.clues[i];
      int y = 53 + int(i) * 40;
      g.fillRoundRect(189, y, 120, 36, 6, colors.panel);
      g.fillRoundRect(189, y, 4, 36, 2,
                      c.fresh ? (c.ble ? colors.heart : colors.accent) : colors.muted);
      snprintf(s, sizeof(s), "%.18s", c.family);
      text(g, s, 197, y + 6, c.fresh ? colors.text : colors.muted);
      snprintf(s, sizeof(s), "%s %ddB %s", c.ble ? "BLE" : "WiFi", c.rssi,
               c.fresh ? c.confidence : "OLD");
      text(g, s, 197, y + 21, c.fresh ? colors.danger : colors.muted);
    }
  }
  // Top bar.
  g.fillRect(0, 0, 320, 24, colors.bg);
  mark(g, colors, 8, 4);
  snprintf(s, sizeof(s), "%.16s", v.name);
  text(g, s, 33, 8, colors.text);
  text(g, rankName(v.level), 33 + int(strlen(s)) * 6 + 8, 8, colors.muted);
  int stateW = int(strlen(v.state)) * 6 + (v.recording ? 22 : 14), stateX = 314 - stateW;
  bool hot = v.stateUrgent || v.recording;
  g.fillRoundRect(stateX, 4, stateW, 16, 8, hot ? colors.danger : colors.panel);
  if (v.recording && now / 600 % 2)
    g.fillCircle(stateX + 9, 12, 3, Ink);
  text(g, v.state, stateX + (v.recording ? 16 : 7), 8, hot ? Ink : colors.accent);
  // GPS: three signal bars and the satellite count.
  int gpsX = stateX - 50;
  for (int i = 0; i < 3; ++i)
    g.fillRect(gpsX + i * 4, 15 - i * 3, 3, 3 + i * 3,
               v.fix || (i == 0 && v.sats) ? colors.accent : colors.panel);
  snprintf(s, sizeof(s), v.fix ? "GPS %u" : "GPS --", v.sats);
  text(g, s, gpsX + 14, 8, v.fix ? colors.text : colors.muted);
  // Vitals row.
  g.fillRoundRect(6, 178, 308, 26, 8, colors.bg);
  if (v.alert) {
    snprintf(s, sizeof(s), "%s  %.22s", v.demo ? "DEMO" : "CLUE", v.alertFamily);
    text(g, s, 15, 182, colors.danger);
    snprintf(s, sizeof(s), "Confidence: %s. Tap to dismiss.", v.alertConfidence);
    text(g, s, 15, 193, colors.text);
  } else if (levelUp) {
    snprintf(s, sizeof(s), "LEVEL %u!", v.level);
    text(g, s, 15, 182, colors.accent);
    snprintf(s, sizeof(s), "Rank: %s", rankName(v.level));
    text(g, s, 15, 193, colors.text);
    starIcon(g, 296, 191, colors.accent);
  } else {
    bool hungry = v.food < HungryBelow, lonely = v.love < LonelyBelow;
    snackIcon(g, 19, 191, hungry ? colors.danger : rgb565(0xffc56b), colors.bg);
    snprintf(s, sizeof(s), "FOOD %u%%", v.food);
    vital(g, v, 10, s, v.food, 100, rgb565(0xffc56b), hungry);
    heart(g, 121, 189, lonely ? colors.danger : colors.heart);
    snprintf(s, sizeof(s), "LOVE %u%%", v.love);
    vital(g, v, 112, s, v.love, 100, colors.heart, lonely);
    starIcon(g, 223, 191, colors.accent);
    snprintf(s, sizeof(s), "LV %u  %u/%u", v.level, v.xp, unsigned(XpPerLevel));
    vital(g, v, 214, s, v.xp, XpPerLevel, colors.accent, false);
  }
  button(g, v, 8, 208, 154, 28, v.recording ? "STOP + SAVE" : "START DRIVE", true);
  button(g, v, 168, 208, 70, 28, v.face ? "RECON" : "FACE");
  button(g, v, 244, 208, 68, 28, "MENU");
}
} // namespace dash
