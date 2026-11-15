// Original vector companions. Shared by firmware and the desktop SVG renderer.
#pragma once
#include "avatar_limits.h"
#include <algorithm>
#include <initializer_list>
#include <math.h>
#include <stdint.h>
namespace dash {
constexpr uint16_t Ink = 0x1085, Paper = 0xff7a, Mint = 0x87b6, Peach = 0xfdaf, Lilac = 0xc5ff,
                   Blush = 0xfbb2;
namespace art {
inline uint16_t mix(uint16_t a, uint16_t b, int n, int d) {
  int r = (((a >> 11) & 31) * (d - n) + ((b >> 11) & 31) * n) / d;
  int g = (((a >> 5) & 63) * (d - n) + ((b >> 5) & 63) * n) / d;
  int blue = ((a & 31) * (d - n) + (b & 31) * n) / d;
  return uint16_t((r << 11) | (g << 5) | blue);
}
inline bool dark(uint16_t c) {
  return ((c >> 11) & 31) * 2 + ((c >> 5) & 63) * 2 + (c & 31) < 46;
}
inline bool pale(uint16_t c) {
  return ((c >> 11) & 31) * 2 + ((c >> 5) & 63) * 2 + (c & 31) > 180;
}
// Neither dark nor bright: too dull to read on a dark surface.
inline bool dim(uint16_t c) {
  return ((c >> 11) & 31) * 2 + ((c >> 5) & 63) * 2 + (c & 31) < 110;
}
// A round-capped stroke built from fills, so it scales in the full-face view.
template <class G> void stroke(G &g, int x0, int y0, int x1, int y1, int w, uint16_t c) {
  if (w <= 1) {
    g.drawLine(x0, y0, x1, y1, c);
    return;
  }
  float dx = float(x1 - x0), dy = float(y1 - y0), len = sqrtf(dx * dx + dy * dy);
  if (len >= 1) {
    int nx = int(lroundf(-dy / len * w / 2)), ny = int(lroundf(dx / len * w / 2));
    g.fillTriangle(x0 + nx, y0 + ny, x0 - nx, y0 - ny, x1 + nx, y1 + ny, c);
    g.fillTriangle(x0 - nx, y0 - ny, x1 - nx, y1 - ny, x1 + nx, y1 + ny, c);
  }
  g.fillCircle(x0, y0, w / 2, c);
  g.fillCircle(x1, y1, w / 2, c);
}
// Draws shapes relative to the buddy's centre. With grow > 0 every shape is drawn fatter in
// one colour: running the silhouette once fat and once true gives a clean sticker outline.
template <class G> struct Pen {
  G &g;
  int cx, cy, grow;
  uint16_t line;
  uint16_t ink(uint16_t c) const {
    return grow ? line : c;
  }
  void oval(int x, int y, int rx, int ry, uint16_t c) {
    g.fillEllipse(cx + x, cy + y, rx + grow, ry + grow, ink(c));
  }
  void box(int x, int y, int w, int h, int r, uint16_t c) {
    w += 2 * grow;
    h += 2 * grow;
    // The display driver does not clamp the corner radius; an oversized one paints a blob.
    r = std::max(0, std::min(r + grow, std::min(w, h) / 2));
    g.fillRoundRect(cx + x - grow, cy + y - grow, w, h, r, ink(c));
  }
  void tri(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) {
    if (grow) {
      float mx = (x0 + x1 + x2) / 3.f, my = (y0 + y1 + y2) / 3.f;
      int *xs[] = {&x0, &x1, &x2}, *ys[] = {&y0, &y1, &y2};
      for (int i = 0; i < 3; ++i) {
        float dx = *xs[i] - mx, dy = *ys[i] - my, len = sqrtf(dx * dx + dy * dy);
        if (len > 0) {
          *xs[i] += int(lroundf(dx / len * grow * 2));
          *ys[i] += int(lroundf(dy / len * grow * 2));
        }
      }
    }
    g.fillTriangle(cx + x0, cy + y0, cx + x1, cy + y1, cx + x2, cy + y2, ink(c));
  }
  void line2(int x0, int y0, int x1, int y1, uint16_t c, int w = 2) {
    if (!grow)
      stroke(g, cx + x0, cy + y0, cx + x1, cy + y1, w, c);
  }
  // A thick stroke that is part of the silhouette, so unlike line2 it takes the outline.
  void bar(int x0, int y0, int x1, int y1, uint16_t c, int w) {
    stroke(g, cx + x0, cy + y0, cx + x1, cy + y1, w + 2 * grow, ink(c));
  }
};
// Redraws any buddy at a different size with crisp edges (used by the full-face view).
template <class G> struct Zoom {
  G &g;
  int ox, oy;
  float k;
  int X(int x) const {
    return ox + int(lroundf(x * k));
  }
  int Y(int y) const {
    return oy + int(lroundf(y * k));
  }
  int R(int r) const {
    return r <= 0 ? 0 : int(lroundf(r * k));
  }
  void fillEllipse(int x, int y, int rx, int ry, uint16_t c) {
    g.fillEllipse(X(x), Y(y), R(rx), R(ry), c);
  }
  void fillCircle(int x, int y, int r, uint16_t c) {
    g.fillCircle(X(x), Y(y), R(r), c);
  }
  // Scaled length of a span: thin details (scanlines, LED dashes) keep at least one pixel.
  int span(int from, int size) const {
    int scaled = int(lroundf((from + size) * k)) - int(lroundf(from * k));
    return size > 0 && scaled < 1 ? 1 : scaled;
  }
  void fillRect(int x, int y, int w, int h, uint16_t c) {
    g.fillRect(X(x), Y(y), span(x, w), span(y, h), c);
  }
  void fillRoundRect(int x, int y, int w, int h, int r, uint16_t c) {
    int sw = span(x, w), sh = span(y, h);
    // Width, height and radius round separately, so the radius must be re-clamped to fit.
    int sr = std::min(R(r), std::min(sw, sh) / 2);
    if (sr <= 0)
      g.fillRect(X(x), Y(y), sw, sh, c);
    else
      g.fillRoundRect(X(x), Y(y), sw, sh, sr, c);
  }
  void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) {
    g.fillTriangle(X(x0), Y(y0), X(x1), Y(y1), X(x2), Y(y2), c);
  }
  void drawLine(int x0, int y0, int x1, int y1, uint16_t c) {
    stroke(g, X(x0), Y(y0), X(x1), Y(y1), int(lroundf(k)), c);
  }
};
} // namespace art

// Every colour a buddy needs, derived from its body colour so custom buddies shade correctly.
struct Tones {
  uint16_t body, blush, line, shade, light, trim;
};
inline Tones buddyTones(int avatar, bool custom, uint16_t customBody, uint16_t customAccent) {
  using art::mix;
  Tones t;
  t.body = custom        ? customBody
           : avatar == 0 ? Paper
           : avatar == 1 ? Mint
           : avatar == 2 ? Lilac
           : avatar == 3 ? 0x0000
                         : 0xfa64;
  t.blush = custom ? customAccent : avatar == 3 ? 0xc5ff : avatar == 4 ? 0xff28 : Blush;
  bool night = art::dark(t.body);
  // Outlines are a deep shade of the body colour; a dark body gets a soft glow rim instead.
  t.line = night ? mix(t.body, 0xc5ff, 3, 5) : mix(t.body, Ink, 13, 20);
  t.shade = night ? mix(t.body, 0xc5ff, 1, 6) : mix(t.body, Ink, 1, 5);
  t.light = mix(t.body, 0xffff, night ? 1 : 3, 5);
  t.trim = custom ? mix(t.body, Ink, 3, 10) : avatar == 1 ? 0x35d0 : avatar == 2 ? 0x8bdb : t.shade;
  return t;
}

// What a buddy looks like. Presets (custom == false) ignore everything but `base`. The slot
// numbers are the ones in a DCAV3 profile line; their counts live in avatar_limits.h.
struct Look {
  int base = 0; // 0 Mochi, 1 Sprout, 2 Orbit, 3 Wisp, 4 Ember.
  bool custom = false;
  uint16_t body = Paper, accent = Blush, eye = Ink;
  int eyes = 0, headwear = 0, eyewear = 0, neckwear = 0;
  int mark = 0, back = 0, held = 0, aura = 0;
};
// What the buddy is doing this frame.
struct Pose {
  int mood = 0, gaze = 0;
  bool blink = false;
  int bounce = 0;
  int kit = 0;   // 0 none, 1 holding the field kit, 2 cruising with it.
  int phase = 0; // Animation counter, one step every 150 ms.
};

// Everything between the web-art markers is also the website's buddy renderer:
// tools/web_art.py converts it to site/buddy_art.mjs, so the studio preview and the LCD are
// drawn by the same statements. Keep it to plain statements on a pen (oval, box, tri, line2,
// bar), int/bool/uint16_t locals, integer tables, if/else and simple for loops. No floats, no
// trigonometry (use the `turn` table), and wrap any division whose remainder matters in int().
// <web-art-begin>
// Twenty-four points round a circle, scaled by 64.
static constexpr int turn[][2] = {
    {64, 0},    {62, 17},   {55, 32},  {45, 45},  {32, 55},  {17, 62},   {0, 64},    {-17, 62},
    {-32, 55},  {-45, 45},  {-55, 32}, {-62, 17}, {-64, 0},  {-62, -17}, {-55, -32}, {-45, -45},
    {-32, -55}, {-17, -62}, {0, -64},  {17, -62}, {32, -55}, {45, -45},  {55, -32},  {62, -17}};
// A ring you can see through: glasses frames and halos.
template <class P> void hoop(P &p, int x, int y, int rx, int ry, uint16_t c, int w) {
  for (int i = 0; i < 24; ++i) {
    int j = (i + 1) % 24;
    p.bar(x + int(turn[i][0] * rx / 64), y + int(turn[i][1] * ry / 64),
          x + int(turn[j][0] * rx / 64), y + int(turn[j][1] * ry / 64), c, w);
  }
}
// A heart whose lobes have radius r; (x, y) is the notch between them.
template <class P> void heart(P &p, int x, int y, int r, uint16_t c) {
  p.oval(x - r + 1, y - int(r / 2), r, r, c);
  p.oval(x + r - 1, y - int(r / 2), r, r, c);
  p.tri(x - 2 * r + 1, y, x + 2 * r - 1, y, x, y + 2 * r, c);
}
// A five-point star of radius r.
template <class P> void star5(P &p, int x, int y, int r, uint16_t c) {
  int wide = int(r * 95 / 100), up = int(r * 3 / 10), leg = int(r * 6 / 10), drop = int(r * 8 / 10);
  p.tri(x, y - r, x - leg, y + drop, x + wide, y - up, c);
  p.tri(x, y - r, x + leg, y + drop, x - wide, y - up, c);
  p.tri(x - wide, y - up, x + wide, y - up, x, y + int(r * 4 / 10), c);
}
// A four-point twinkle.
template <class P> void spark(P &p, int x, int y, int r, uint16_t c) {
  p.box(x - r, y - 1, 2 * r + 1, 2, 0, c);
  p.box(x - 1, y - r, 2, 2 * r + 1, 0, c);
}
// Body silhouettes. Called twice by drawBuddy: once fat in the outline colour, once true.
template <class P> void buddyBody(P &p, int avatar, uint16_t body, uint16_t trim, int phase) {
  if (avatar == 0) { // Mochi: cat with a curled tail and cheek fluff.
    static constexpr int tail[][3] = {
        {44, 32, 7}, {54, 27, 6}, {61, 18, 6}, {63, 8, 5}, {60, -1, 5}};
    for (auto &t : tail)
      p.oval(t[0], t[1], t[2], t[2], body);
    p.tri(-47, -12, -43, -55, -9, -31, body);
    p.tri(47, -12, 43, -55, 9, -31, body);
    p.tri(-48, 2, -58, 9, -46, 15, body);
    p.tri(48, 2, 58, 9, 46, 15, body);
    p.oval(0, 0, 52, 42, body);
    p.oval(-26, 37, 17, 9, body);
    p.oval(26, 37, 17, 9, body);
  } else if (avatar == 1) { // Sprout: dragon with bat wings, horns, crest and a spade tail.
    for (int s : {-1, 1}) {
      p.tri(s * 40, 0, s * 69, -24, s * 60, 8, trim);
      p.tri(s * 40, 8, s * 66, 2, s * 56, 29, trim);
      p.tri(s * 35, -22, s * 37, -58, s * 14, -33, Peach);
    }
    p.tri(-7, -39, 0, -55, 7, -39, trim);
    p.tri(-21, -36, -17, -48, -10, -38, trim);
    p.tri(21, -36, 17, -48, 10, -38, trim);
    p.oval(0, 1, 51, 42, body);
    p.oval(-31, 38, 16, 9, body);
    p.oval(31, 38, 16, 9, body);
  } else if (avatar == 2) { // Orbit: boxy robot with an antenna and side pods.
    p.box(-2, -53, 4, 19, 0, body);
    p.oval(0, -57, 7, 7, Peach);
    p.box(-60, -14, 14, 32, 5, trim);
    p.box(46, -14, 14, 32, 5, trim);
    p.box(-48, -36, 96, 77, 22, body);
    p.box(-32, 37, 24, 10, 5, trim);
    p.box(8, 37, 24, 10, 5, trim);
  } else if (avatar == 3) { // Wisp: ghost with a curl, stubby arms and a scalloped hem.
    p.tri(-14, -32, 7, -61, 13, -31, body);
    p.oval(-50, 10, 9, 7, body);
    p.oval(50, 10, 9, 7, body);
    p.oval(0, -3, 48, 40, body);
    p.box(-48, -3, 97, 34, 0, body);
    for (int x : {-32, 0, 32})
      p.oval(x, 31 + (x == 0 ? 4 : 0), 16, 11, body);
  } else { // Ember: three licking flame tips over a round base.
    int f = (phase % 6 < 3) ? -2 : 2;
    p.tri(-40, 14, -35, -44 - f, -4, -12, body);
    p.tri(-24, -12, 3 + f, -64, 28, 2, body);
    p.tri(10, -4, 43, -42 + f, 46, 18, body);
    p.oval(0, 7, 47, 36, body);
    p.oval(-28, 39, 14, 8, body);
    p.oval(28, 39, 14, 8, body);
  }
}

// The colour the eyes are really drawn in. Presets have their own; a custom colour is swapped
// when it would vanish into whatever is behind it (Orbit's screen or the body).
inline uint16_t eyeInk(int avatar, bool custom, uint16_t eyeColor, uint16_t body) {
  uint16_t eye = custom ? eyeColor : avatar == 2 ? Mint : avatar == 3 ? 0xefff : Ink;
  uint16_t surface = avatar == 2 ? 0x10c7 : body;
  if (custom && art::dark(eye) && art::dark(surface))
    eye = avatar == 2 ? Mint : 0xefff;
  else if (custom && art::dim(eye) && art::dark(surface))
    eye = art::mix(eye, 0xffff, 1, 2); // Keep the hue, lift it off the dark.
  if (custom && art::pale(eye) && art::pale(surface))
    eye = Ink;
  return eye;
}

// Inside details: cel shading, markings, face and mouth.
template <class P>
void buddyDetails(P &p, int avatar, int mood, int gaze, bool blink, bool custom, int eyeStyle,
                  uint16_t eyeColor, uint16_t body, uint16_t blush, uint16_t line, uint16_t shade,
                  uint16_t light, uint16_t trim, int phase) {
  bool night = art::dark(body);
  if (avatar == 0) {
    uint16_t stripe = custom ? shade : 0xecf0;
    p.oval(0, 0, 52, 42, art::mix(body, shade, 1, 2));
    p.oval(-2, -3, 49, 38, body);
    p.oval(60, -1, 5, 5, stripe);
    p.oval(63, 8, 5, 3, stripe);
    for (int s : {-1, 1}) {
      p.tri(s * 40, -24, s * 40, -45, s * 23, -31, blush);
      p.line2(s * 62, 2, s * 44, 6, line, 1);
      p.line2(s * 62, 15, s * 44, 11, line, 1);
      p.oval(s * 26, 37, 17, 9, body);
      p.line2(s * 21, 40, s * 21, 44, line);
      p.line2(s * 31, 40, s * 31, 44, line);
    }
    p.tri(-4, -42, 4, -42, 0, -31, stripe);
    p.tri(-16, -39, -9, -41, -11, -31, stripe);
    p.tri(16, -39, 9, -41, 11, -31, stripe);
    p.oval(-27, -25, 8, 3, light);
  } else if (avatar == 1) {
    p.oval(0, 1, 51, 42, shade);
    p.oval(-3, -3, 47, 37, body);
    p.oval(0, 27, 28, 16, custom ? light : 0xdff6);
    p.line2(-17, 24, 17, 24, shade, 1);
    p.line2(-13, 32, 13, 32, shade, 1);
    for (int s : {-1, 1}) {
      p.line2(s * 51, 2, s * 66, -19, line, 1);
      p.line2(s * 51, 10, s * 57, 24, line, 1);
      p.line2(s * 33, -35, s * 26, -31, line, 1);
      p.line2(s * 34, -45, s * 28, -42, line, 1);
      p.oval(s * 31, 38, 16, 9, body);
      for (int t : {-9, 0, 9})
        p.tri(s * 31 + t - 3, 43, s * 31 + t + 3, 43, s * 31 + t, 48, 0xffff);
    }
    p.oval(31, -22, 4, 3, shade);
    p.oval(38, -12, 3, 2, shade);
    p.oval(24, -30, 3, 2, shade);
    p.oval(-27, -25, 8, 3, light);
  } else if (avatar == 2) {
    p.box(-48, -36, 96, 77, 22, shade);
    p.box(-48, -36, 92, 73, 21, body);
    p.box(-40, -26, 80, 50, 15, Ink);
    p.box(-37, -23, 74, 44, 12, 0x10c7);
    p.line2(-33, -15, -28, -20, 0x4a9f);
    p.oval(-2, -59, 2, 2, 0xffff);
    for (int s : {-1, 1}) {
      p.oval(s * 53, 2, 3, 3, Mint);
      p.line2(s * 53, -9, s * 53, -6, line, 1);
      p.oval(s * 39, 33, 2, 2, shade);
    }
    p.oval(-9, 33, 3, 3, Mint);
    p.oval(0, 33, 3, 3, Peach);
    p.oval(9, 33, 3, 3, blush);
  } else if (avatar == 3) {
    p.oval(-14, -12, 26, 22, shade);
    p.oval(-10, -8, 26, 22, body);
    p.line2(-8, -39, 4, -53, line, 1);
    for (int s : {-1, 1}) { // Drifting will-o'-wisp sparkles.
      int y = -14 - int((phase + s + 8) % 8) * 2;
      p.box(s * 64 - 4, y - 1, 9, 2, 0, line);
      p.box(s * 64 - 1, y - 4, 2, 9, 0, line);
    }
  } else {
    int f = (phase % 6 < 3) ? -2 : 2;
    uint16_t mid = custom ? art::mix(body, blush, 1, 2) : 0xfcc3;
    p.oval(0, 7, 47, 36, shade);
    p.oval(-2, 4, 44, 33, body);
    p.tri(-15, -4, 3 + f, -45, 19, 0, mid);
    p.oval(0, 14, 36, 27, mid);
    p.tri(-12, 8, 3, -22 - f, 15, 10, blush);
    p.oval(0, 23, 26, 17, blush);
    for (int s : {-1, 1}) {
      p.oval(s * 28, 39, 14, 8, body);
      p.oval(s * 57, -10 - int((phase + s + 8) % 8) * 3, 2, 2, blush);
    }
    p.oval(38, -52 - int((phase + 3) % 8) * 2, 1, 1, mid);
  }
  // Face. Preset buddies get eyes of their own; custom ones use the chosen eye style.
  uint16_t eye = eyeInk(avatar, custom, eyeColor, body);
  uint16_t lid = avatar == 2   ? 0x10c7
                 : avatar == 4 ? (custom ? art::mix(body, blush, 1, 2) : 0xfcc3)
                               : body;
  uint16_t glow = custom        ? blush
                  : avatar == 0 ? 0xfe45
                  : avatar == 1 ? 0xffe0
                  : avatar == 4 ? 0xfc00
                                : blush;
  bool led = avatar == 2 && !custom, hollow = avatar == 3 && !custom;
  int ey = -4, ex = gaze;
  if (mood == 3) { // Love: heart eyes.
    for (int s : {-1, 1}) {
      int x = ex + s * 21;
      p.oval(x - 5, ey - 3, 6, 6, blush);
      p.oval(x + 5, ey - 3, 6, 6, blush);
      p.tri(x - 11, ey, x + 11, ey, x, ey + 12, blush);
      p.oval(x - 6, ey - 5, 2, 2, 0xffff);
    }
  } else if (mood == 5) { // Cheer: happy arches.
    for (int s : {-1, 1}) {
      int x = ex + s * 21;
      p.line2(x - 10, ey + 4, x, ey - 5, eye, 3);
      p.line2(x, ey - 5, x + 10, ey + 4, eye, 3);
    }
  } else if (blink || mood == 2) { // Closed: soft downward curves.
    for (int s : {-1, 1}) {
      int x = ex + s * 21;
      p.line2(x - 9, ey, x - 4, ey + 4, eye);
      p.line2(x - 4, ey + 4, x + 4, ey + 4, eye);
      p.line2(x + 4, ey + 4, x + 9, ey, eye);
    }
  } else if (mood == 4) { // Alarm: wide whites with pinprick pupils.
    for (int s : {-1, 1}) {
      int x = ex + s * 21;
      p.oval(x, ey, 11, 15, avatar == 2 || avatar == 3 ? eye : Ink);
      p.oval(x, ey, 9, 13, avatar == 2 ? Ink : 0xffff);
      p.oval(x + int(gaze / 2), ey, 3, 3, avatar == 2 ? eye : Ink);
    }
  } else {
    for (int s : {-1, 1}) {
      int x = ex + s * 21, tall = mood == 1 && s > 0 ? 14 : 12;
      if (led) { // LED panel eyes with scanlines.
        p.box(x - 8, ey - tall, 16, tall * 2, 6, eye);
        p.box(x - 5, ey - tall + 3, 5, 5, 2, 0xffff);
        for (int y = ey - tall + 5; y < ey + tall - 2; y += 5)
          p.box(x - 8, y, 16, 1, 0, 0x10c7);
      } else if (hollow) { // Pupil-less glowing eyes.
        p.oval(x, ey, 9, tall + 1, blush);
        p.oval(x, ey - 1, 8, tall, eye);
      } else if (eyeStyle == 0 || (eyeStyle == 5 && s < 0)) {
        p.oval(x, ey, 8, tall, eye);
        p.oval(x, ey + 4, 6, tall - 5, glow); // Under-light crescent.
        p.oval(x, ey + 1, 6, tall - 5, eye);
        p.oval(x - 2, ey - 5, 3, 3, 0xffff);
        p.oval(x + 3, ey + 6, 1, 1, 0xffff);
      } else if (eyeStyle == 1) { // Four-point sparkle.
        p.tri(x, ey - 14, x - 10, ey, x + 10, ey, eye);
        p.tri(x - 10, ey, x + 10, ey, x, ey + 14, eye);
        p.oval(x, ey, 3, 3, 0xffff);
      } else if (eyeStyle == 2) { // Relaxed lids.
        p.box(x - 10, ey - 1, 20, 5, 2, eye);
        p.oval(x, ey + 5, 5, 4, eye);
      } else if (eyeStyle == 3) { // Cat irises with a vertical pupil.
        p.oval(x, ey, 10, 12, eye);
        p.oval(x, ey, 3, 10, avatar == 2 ? Paper : Ink);
        p.oval(x - 4, ey - 5, 2, 2, 0xffff);
      } else if (eyeStyle == 4) {
        p.box(x - 9, ey - 10, 18, 20, 3, eye);
        p.box(x - 5, ey - 6, 5, 5, 0, 0xffff);
      } else if (eyeStyle == 5) { // Wink.
        p.line2(x - 9, ey + 2, x, ey - 3, eye);
        p.line2(x, ey - 3, x + 9, ey + 2, eye);
      } else if (eyeStyle == 6) { // Heart.
        heart(p, x, ey - 2, 6, eye);
        p.oval(x - 6, ey - 6, 2, 2, 0xffff);
      } else if (eyeStyle == 7) { // Star.
        star5(p, x, ey + 1, 13, eye);
        p.oval(x - 2, ey - 2, 2, 2, 0xffff);
      } else if (eyeStyle == 8) { // Glitch X: crossed-out eyes with a colour-split ghost.
        int slip = phase % 12 < 2 ? 3 : 1;
        p.line2(x - 7 + slip, ey - 8, x + 7 + slip, ey + 8, glow, 3);
        p.line2(x + 7 - slip, ey - 8, x - 7 - slip, ey + 8, Mint, 3);
        p.line2(x - 7, ey - 8, x + 7, ey + 8, eye, 4);
        p.line2(x + 7, ey - 8, x - 7, ey + 8, eye, 4);
      } else { // Half-lid: a big iris under a heavy, unbothered lid.
        p.oval(x, ey + 1, 9, 11, eye);
        p.oval(x, ey + 5, 6, 6, glow);
        p.oval(x, ey + 3, 6, 6, eye);
        p.oval(x + 3, ey + 5, 2, 2, 0xffff);
        p.box(x - 11, ey - 13, 22, 12, 0, lid);
        p.line2(x - 10, ey - 1, x + 10, ey - 1, eye, 3);
      }
      if (mood == 6) // Pout: heavy, unimpressed lids.
        p.box(x - 11, ey - 16, 22, 10, 0, lid);
    }
  }
  uint16_t brow = led || hollow ? eye : avatar == 2 || night ? eye : line;
  if (mood == 1)
    p.line2(ex + 13, ey - 20, ex + 28, ey - 24, brow);
  if (mood == 4) {
    p.line2(ex - 31, ey - 22, ex - 16, ey - 27, brow);
    p.line2(ex + 16, ey - 27, ex + 31, ey - 22, brow);
    p.tri(60, -28, 66, -12, 54, -12, 0x55ff);
    p.oval(60, -12, 6, 5, 0x55ff);
  } else if (mood == 6) {
    p.line2(ex - 31, ey - 17, ex - 15, ey - 22, brow);
    p.line2(ex + 15, ey - 22, ex + 31, ey - 17, brow);
  }
  // Mouths. Orbit talks in LED segments; everyone else gets ink.
  uint16_t lip = avatar == 2 || night ? eye : Ink;
  if (avatar == 0 && mood != 4) // Cat nose.
    p.tri(-3, 11, 3, 11, 0, 15, blush);
  if (led) {
    if (mood == 4)
      p.box(-6, 14, 12, 9, 3, eye);
    else if (mood == 3 || mood == 5) {
      p.box(-11, 14, 22, 4, 2, eye);
      p.box(-7, 18, 14, 4, 2, eye);
    } else
      for (int i = -1; i <= 1; ++i)
        p.box(i * 7 - 2, 17 + (mood == 6 ? (i ? 2 : 0) : (i ? 0 : 2)), 5, 3, 1, eye);
  } else if (mood == 4) {
    p.oval(0, 22, 7, 9, lip);
    p.oval(0, 25, 4, 4, blush);
  } else if (mood == 6) {
    p.line2(-7, 23, 0, 19, lip);
    p.line2(0, 19, 7, 23, lip);
  } else if (mood == 5 || mood == 3) {
    p.oval(0, 20, 10, 8, lip);
    p.oval(0, 24, 6, 3, blush);
    p.box(-5, 13, 10, 3, 1, 0xffff);
  } else {
    p.line2(-8, 17, -4, 21, lip);
    p.line2(-4, 21, 0, 18, lip);
    p.line2(0, 18, 4, 21, lip);
    p.line2(4, 21, 8, 17, lip);
    if (avatar == 0 || avatar == 1) // One little fang.
      p.tri(5, 21, 9, 19, 8, 25, 0xffff);
  }
}

// Cheeks and face marks sit on the body like ink, so they take no outline of their own.
template <class P> void buddyMark(P &p, int avatar, int mark, uint16_t ink, uint16_t blush) {
  uint16_t rosy = avatar == 4 ? 0xfa00 : blush;
  if (mark == 1) { // Blush hearts take the place of the cheek hatching.
    for (int s : {-1, 1}) {
      heart(p, s * 38, 10, 4, rosy);
      p.oval(s * 38 - 3, 8, 1, 1, 0xffff);
    }
  } else if (avatar == 0 || avatar == 1 || avatar == 4) {
    for (int s : {-1, 1}) // Hand-drawn blush hatching instead of pink ovals.
      for (int i = -1; i <= 1; ++i)
        p.line2(s * 37 + i * 5 - 2, 15, s * 37 + i * 5 + 2, 10, rosy);
  }
  if (mark == 2) { // Star cheek.
    star5(p, 39, 13, 9, Ink);
    star5(p, 39, 13, 7, 0xffe0);
  } else if (mark == 3) { // Whisker stripes.
    for (int s : {-1, 1})
      for (int y : {0, 8, 16})
        p.tri(s * 48, y - 3, s * 48, y + 3, s * 34, y + 1, ink);
  } else if (mark == 4) { // Tear.
    p.tri(-32, 14, -24, 14, -28, 4, 0x55ff);
    p.oval(-28, 16, 4, 4, 0x55ff);
    p.oval(-30, 15, 1, 1, 0xffff);
  } else if (mark == 5) { // Circuit trace.
    p.line2(8, -36, 20, -36, ink);
    p.line2(20, -36, 27, -29, ink);
    p.line2(27, -29, 35, -29, ink);
    p.line2(-46, 2, -40, 9, ink);
    p.line2(-40, 9, -40, 21, ink);
    p.oval(8, -36, 3, 3, blush);
    p.oval(35, -29, 3, 3, blush);
    p.oval(-46, 2, 3, 3, blush);
    p.oval(-40, 21, 3, 3, blush);
  } else if (mark == 6) { // Stitch scar.
    p.line2(-37, -21, -15, -31, ink);
    for (int i : {0, 1, 2}) {
      int x = -32 + i * 6, y = -23 - i * 3;
      p.line2(x - 2, y - 4, x + 2, y + 4, ink, 1);
    }
  } else if (mark == 7) { // Barcode.
    static constexpr int bars[][2] = {{28, 2}, {31, 1}, {33, 1}, {35, 3},
                                      {39, 1}, {41, 2}, {44, 1}};
    for (auto &b : bars)
      p.box(b[0], 8, b[1], 10, 0, ink);
  }
}

// Gear worn behind the body: wings, capes and packs.
template <class P> void buddyBack(P &p, int back, uint16_t blush, uint16_t dusk, int phase) {
  if (back == 1) { // Angel wings: three white feathers a side.
    for (int s : {-1, 1}) {
      p.oval(s * 64, -14, 20, 11, 0xffff);
      p.oval(s * 66, 0, 16, 9, 0xffff);
      p.oval(s * 61, 12, 12, 7, 0xffff);
      p.line2(s * 58, -12, s * 79, -15, 0xc65f, 1);
      p.line2(s * 58, 1, s * 77, 0, 0xc65f, 1);
      p.line2(s * 56, 12, s * 69, 13, 0xc65f, 1);
    }
  } else if (back == 2) { // Bat wings: dark webbing with accent ribs.
    for (int s : {-1, 1}) {
      p.tri(s * 40, -6, s * 84, -34, s * 80, 2, dusk);
      p.tri(s * 40, -2, s * 80, -4, s * 70, 22, dusk);
      p.tri(s * 40, 4, s * 70, 14, s * 58, 30, dusk);
      p.line2(s * 52, -10, s * 81, -30, blush, 1);
      p.line2(s * 52, -2, s * 76, 6, blush, 1);
      p.line2(s * 52, 6, s * 63, 22, blush, 1);
    }
  } else if (back == 3) { // Cape with a tall collar.
    p.tri(-36, -10, -74, 44, 0, 44, blush);
    p.tri(36, -10, 74, 44, 0, 44, blush);
    for (int s : {-1, 1}) {
      p.tri(s * 34, 4, s * 66, -30, s * 60, 8, dusk);
      p.line2(s * 60, 22, s * 66, 40, dusk, 1);
    }
  } else if (back == 4) { // Backpack with a whip antenna and a blinking tip.
    p.box(38, -30, 38, 56, 10, dusk);
    p.box(62, -12, 9, 18, 3, blush);
    p.line2(58, 14, 72, 14, blush, 1);
    p.bar(62, -30, 70, -58, Paper, 2);
    p.oval(70, -60, 4, 4, phase % 8 < 4 ? 0xf9c7 : blush);
  } else if (back == 5) { // Jet thrusters with flickering exhaust.
    int f = phase % 4 < 2 ? 0 : 3;
    for (int s : {-1, 1}) {
      p.tri(s * 60 - 8, 30, s * 60 + 8, 30, s * 60, 43 + f, 0xfc00);
      p.box(s * 60 - 9, -2, 18, 34, 6, 0xd6ba);
      p.box(s * 60 - 9, 5, 18, 4, 0, blush);
      p.tri(s * 60 - 4, 33, s * 60 + 4, 33, s * 60, 39 + f, 0xffe0);
    }
  } else if (back == 6) { // Fairy wings: two glassy lobes a side.
    uint16_t glass = art::mix(blush, 0xffff, 3, 5);
    for (int s : {-1, 1}) {
      p.oval(s * 64, -22, 22, 14, glass);
      p.oval(s * 60, 7, 15, 11, glass);
      p.oval(s * 67, -23, 13, 7, 0xffff);
      p.oval(s * 62, 7, 8, 5, 0xffff);
      p.line2(s * 50, -16, s * 82, -26, blush, 1);
      p.line2(s * 50, 4, s * 72, 10, blush, 1);
    }
  }
}

template <class P> void buddyNeck(P &p, int neckwear, uint16_t blush, uint16_t dusk) {
  if (neckwear == 1) { // Scarf.
    p.box(-37, 31, 74, 8, 3, blush);
    p.box(22, 31, 12, 22, 4, blush);
    p.line2(24, 45, 32, 45, Paper, 1);
    p.line2(24, 49, 32, 49, Paper, 1);
  } else if (neckwear == 2) { // Bandana.
    p.tri(-30, 30, 30, 30, 0, 48, blush);
    p.oval(0, 36, 3, 3, Paper);
    p.oval(-12, 33, 1, 1, Paper);
    p.oval(12, 33, 1, 1, Paper);
  } else if (neckwear == 3) { // Bow tie.
    p.tri(-4, 33, -21, 24, -21, 42, blush);
    p.tri(4, 33, 21, 24, 21, 42, blush);
    p.oval(0, 33, 5, 5, Peach);
  } else if (neckwear == 4) { // Choker with a heart charm.
    p.box(-28, 30, 56, 5, 2, Ink);
    heart(p, 0, 37, 3, blush);
  } else if (neckwear == 5) { // Spiked collar.
    for (int x : {-24, -12, 0, 12, 24})
      p.tri(x - 4, 34, x + 4, 34, x, 45, 0xd6ba);
    p.box(-31, 29, 62, 7, 3, Ink);
    for (int x : {-24, -12, 0, 12, 24})
      p.oval(x, 32, 1, 1, 0xd6ba);
  } else if (neckwear == 6) { // Bell collar.
    p.box(-30, 30, 60, 6, 3, blush);
    p.oval(0, 40, 6, 6, 0xfe60);
    p.line2(-4, 40, 4, 40, Ink, 1);
    p.oval(0, 43, 1, 1, Ink);
  } else if (neckwear == 7) { // Headphones resting round the neck.
    p.bar(-36, 28, -20, 39, dusk, 4);
    p.bar(-20, 39, 0, 42, dusk, 4);
    p.bar(0, 42, 20, 39, dusk, 4);
    p.bar(20, 39, 36, 28, dusk, 4);
    for (int s : {-1, 1}) {
      p.oval(s * 39, 25, 9, 11, blush);
      p.oval(s * 39, 25, 5, 7, dusk);
    }
  }
}

// `gy` is the height the glasses sit at: they lift onto the forehead for big emotions.
template <class P>
void buddyEyewear(P &p, int eyewear, int gy, uint16_t blush, uint16_t dusk, int phase) {
  if (eyewear == 1) { // Round glasses.
    for (int s : {-1, 1})
      hoop(p, s * 21, gy, 13, 14, blush, 2);
    p.bar(-8, gy, 8, gy, blush, 2);
  } else if (eyewear == 2 || eyewear == 3) {
    p.box(-39, gy - 12, 78, 24, 6, blush);
    if (eyewear == 2) { // Shades.
      p.box(-35, gy - 8, 31, 17, 4, Ink);
      p.box(4, gy - 8, 31, 17, 4, Ink);
      p.line2(-28, gy - 5, -20, gy + 3, Paper, 1);
      p.line2(12, gy - 5, 20, gy + 3, Paper, 1);
    } else { // Cyber visor.
      p.box(-35, gy - 8, 70, 16, 4, Ink);
      for (int x = -27; x <= 27; x += 9)
        p.box(x, gy - 3, 4, 6, 0, Mint);
    }
  } else if (eyewear == 4) { // Heart shades.
    p.bar(-7, gy - 3, 7, gy - 3, blush, 2);
    for (int s : {-1, 1}) {
      heart(p, s * 21, gy - 3, 8, blush);
      heart(p, s * 21, gy - 2, 6, dusk);
      p.line2(s * 21 - 7, gy - 4, s * 21 - 3, gy - 8, 0xffff, 1);
    }
  } else if (eyewear == 5) { // Monocle on a chain.
    hoop(p, 21, gy, 13, 14, 0xfe60, 2);
    p.line2(13, gy - 6, 17, gy - 10, 0xffff, 1);
    p.line2(31, gy + 10, 36, gy + 22, 0xfe60, 1);
    p.line2(36, gy + 22, 44, gy + 30, 0xfe60, 1);
  } else if (eyewear == 6) { // Eyepatch.
    p.bar(-48, gy - 13, -21, gy - 8, Ink, 2);
    p.bar(-21, gy - 8, 46, gy - 24, Ink, 2);
    p.box(-33, gy - 11, 24, 16, 5, Ink);
    p.tri(-33, gy + 1, -9, gy + 1, -21, gy + 15, Ink);
    p.line2(-25, gy - 4, -17, gy + 4, blush, 1);
    p.line2(-17, gy - 4, -25, gy + 4, blush, 1);
  } else if (eyewear == 7) { // Scan-line HUD: a see-through display with a moving sweep.
    p.box(-41, gy - 17, 82, 4, 2, blush);
    for (int s : {-1, 1})
      p.box(s * 38 - 4, gy - 15, 8, 23, 3, blush);
    for (int y = gy - 8; y <= gy + 10; y += 6)
      p.line2(-33, y, 33, y, Mint, 1);
    int sweep = gy - 9 + phase % 8 * 3;
    p.line2(-33, sweep, 33, sweep, 0xffff, 1);
    p.oval(38, gy - 2, 2, 2, phase % 8 < 4 ? 0xf9c7 : Mint);
  }
}

template <class P> void buddyHead(P &p, int headwear, uint16_t blush, uint16_t dusk, int phase) {
  if (headwear == 1) { // A tiny bow, never obscuring the eyes.
    p.tri(-6, -34, -23, -44, -23, -24, blush);
    p.tri(6, -34, 23, -44, 23, -24, blush);
    p.oval(0, -34, 6, 6, blush);
    p.oval(0, -34, 3, 3, dusk);
  } else if (headwear == 2) { // Crown.
    p.box(-22, -42, 44, 10, 0, Peach);
    p.tri(-22, -42, -22, -59, -8, -42, Peach);
    p.tri(-10, -42, 0, -61, 10, -42, Peach);
    p.tri(8, -42, 22, -59, 22, -42, Peach);
    p.oval(0, -37, 3, 3, blush);
    p.oval(-13, -37, 2, 2, Mint);
    p.oval(13, -37, 2, 2, Mint);
  } else if (headwear == 3 || headwear == 5) { // Beanie, or the scout cap with its peak.
    p.oval(0, -38, 35, 17, blush);
    p.box(-38, -38, headwear == 5 ? 83 : 76, 10, 4, headwear == 5 ? blush : dusk);
    if (headwear == 3)
      p.oval(0, -57, 7, 7, Peach);
    else
      p.oval(8, -44, 5, 5, Paper);
  } else if (headwear == 4) { // Flower.
    static constexpr int petals[][2] = {{9, 0}, {2, 8}, {-7, 5}, {-7, -5}, {2, -8}};
    for (auto &q : petals)
      p.oval(29 + q[0], -33 + q[1], 6, 6, blush);
    p.oval(29, -33, 5, 5, Peach);
  } else if (headwear == 6) { // Cat-ear headphones.
    static constexpr int band[][2] = {{-52, -8}, {-47, -29}, {-32, -44}, {-12, -51},
                                      {12, -51}, {32, -44},  {47, -29},  {52, -8}};
    for (int i = 0; i < 7; ++i)
      p.bar(band[i][0], band[i][1], band[i + 1][0], band[i + 1][1], dusk, 4);
    for (int s : {-1, 1}) {
      p.tri(s * 40, -38, s * 34, -62, s * 18, -49, blush);
      p.tri(s * 35, -44, s * 32, -55, s * 24, -49, Peach);
      p.box(s * 54 - 8, -18, 16, 28, 7, blush);
      p.box(s * 54 - 3, -12, 6, 16, 3, dusk);
    }
  } else if (headwear == 7) { // Witch hat with a crooked tip.
    p.tri(-21, -40, 21, -40, 6, -59, dusk);
    p.tri(-2, -53, 8, -62, 20, -56, dusk);
    p.oval(0, -38, 43, 7, dusk);
    p.box(-17, -47, 34, 6, 0, blush);
    p.box(-4, -48, 9, 8, 1, 0xfe60);
    p.box(-2, -46, 5, 4, 0, dusk);
  } else if (headwear == 8) { // Halo.
    hoop(p, 0, -55, 24, 6, 0xfe60, 3);
    p.line2(-18, -58, -8, -60, 0xffff, 1);
  } else if (headwear == 9) { // Horns.
    for (int s : {-1, 1}) {
      p.tri(s * 11, -39, s * 29, -35, s * 26, -52, blush);
      p.tri(s * 17, -45, s * 28, -47, s * 33, -63, blush);
      p.line2(s * 18, -42, s * 26, -41, dusk, 1);
      p.line2(s * 22, -48, s * 28, -48, dusk, 1);
    }
  } else if (headwear == 10) { // Antenna headband: two bobbles on springs.
    int sway = phase % 8 < 4 ? 0 : 2;
    p.bar(-36, -33, -19, -43, dusk, 3);
    p.bar(-19, -43, 0, -46, dusk, 3);
    p.bar(0, -46, 19, -43, dusk, 3);
    p.bar(19, -43, 36, -33, dusk, 3);
    for (int s : {-1, 1}) {
      p.bar(s * 19, -43, s * 24 + sway, -57, Paper, 2);
      p.oval(s * 24 + sway, -59, 5, 5, s < 0 ? blush : Peach);
      p.oval(s * 24 + sway - 2, -61, 1, 1, 0xffff);
    }
  } else if (headwear == 11) { // Hood: a soft ring round the face with a drooping point.
    p.tri(18, -46, 50, -62, 44, -32, blush);
    p.oval(0, -40, 44, 13, blush);
    for (int i = 11; i < 25; ++i) {
      int a = i % 24, b = (i + 1) % 24;
      p.bar(int(turn[a][0] * 51 / 64), int(turn[a][1] * 45 / 64) - 3, int(turn[b][0] * 51 / 64),
            int(turn[b][1] * 45 / 64) - 3, blush, 11);
    }
    p.oval(49, -60, 3, 3, Peach);
    for (int s : {-1, 1}) {
      p.line2(s * 44, 14, s * 40, 30, dusk, 1);
      p.oval(s * 40, 31, 2, 2, Peach);
    }
  }
}

// Something to carry. The field kit takes its place while the buddy is working.
template <class P>
void buddyHeld(P &p, int held, uint16_t body, uint16_t blush, uint16_t dusk, int phase) {
  if (held == 1) { // Boba tea.
    static constexpr int pearls[][2] = {{52, 39}, {58, 40}, {63, 38}, {55, 34}, {61, 33}};
    p.bar(60, 10, 65, -7, blush, 3);
    p.box(46, 10, 22, 35, 5, 0xffbd);
    p.box(48, 22, 18, 21, 3, 0xbc49);
    p.box(44, 8, 26, 5, 2, blush);
    for (auto &q : pearls)
      p.oval(q[0], q[1], 2, 2, Ink);
    p.oval(42, 31, 9, 8, body);
  } else if (held == 2) { // Lollipop.
    p.bar(58, 18, 58, 43, Paper, 2);
    p.oval(58, 5, 13, 13, blush);
    p.oval(58, 5, 9, 9, 0xffff);
    p.oval(58, 5, 5, 5, blush);
    p.oval(58, 5, 2, 2, 0xffff);
    p.oval(53, 36, 9, 7, body);
  } else if (held == 3) { // Game pad.
    p.box(38, 20, 36, 22, 9, dusk);
    p.box(44, 29, 9, 3, 0, Paper);
    p.box(47, 26, 3, 9, 0, Paper);
    p.oval(63, 33, 2, 2, Mint);
    p.oval(68, 28, 2, 2, blush);
    p.oval(39, 37, 8, 7, body);
  } else if (held == 4) { // Lantern.
    uint16_t glow = phase % 6 < 3 ? 0xffe0 : 0xfec0;
    p.bar(52, 6, 58, -1, dusk, 2);
    p.bar(58, -1, 64, 6, dusk, 2);
    p.box(49, 6, 18, 5, 2, dusk);
    p.box(50, 11, 16, 24, 3, glow);
    p.oval(58, 25, 4, 6, 0xfc00);
    p.oval(58, 27, 2, 3, 0xffff);
    p.box(49, 35, 18, 5, 2, dusk);
    p.oval(42, 30, 9, 8, body);
  } else if (held == 5) { // Plush bear.
    uint16_t fur = art::mix(blush, 0xffff, 1, 2);
    p.oval(51, 13, 4, 4, fur);
    p.oval(65, 13, 4, 4, fur);
    p.oval(58, 37, 9, 8, fur);
    p.oval(58, 22, 10, 9, fur);
    p.oval(58, 25, 4, 3, 0xffff);
    p.oval(54, 20, 1, 1, Ink);
    p.oval(62, 20, 1, 1, Ink);
    p.oval(58, 24, 1, 1, Ink);
    p.oval(45, 35, 8, 7, body);
  }
}

// A toy Yagi and handheld receiver, with paws wrapped around both.
// Animation is decorative: never infer bearing from antenna orientation.
template <class P>
void buddyKit(P &p, int kit, int mood, uint16_t body, uint16_t blush, int phase) {
  uint16_t signal = mood == 4 ? 0xf9c7 : Mint;
  int lift = kit == 2 ? (phase % 6 < 3 ? -2 : 1) : 0;
  int ax = -61, ay = 6 + lift;
  p.bar(ax + 11, ay + 21, ax - 5, ay - 14, Paper, 2);
  p.bar(ax - 16, ay - 10, ax + 12, ay - 20, Paper, 2);
  p.bar(ax - 12, ay - 2, ax + 15, ay - 12, Paper, 2);
  p.bar(ax - 7, ay + 6, ax + 17, ay - 4, Paper, 2);
  p.box(ax + 6, ay + 15, 8, 18, 3, blush);
  p.oval(-45, 29 + lift, 10, 8, body);
  p.bar(52, -3, 58, 13, Paper, 2);
  p.box(39, 10, 32, 37, 5, blush);
  p.box(43, 14, 24, 23, 4, Ink);
  // Sweeping screen and equalizer stand in for listening gear.
  int sweep = phase % 8;
  p.line2(55, 32, 46 + sweep * 2, 17, signal, 1);
  p.oval(59, 23, 2, 2, signal);
  p.oval(49, 41, 2, 2, Paper);
  p.oval(59, 41, 2, 2, signal);
  p.bar(66, 11, 59, -9, blush, 2);
  p.box(49, -17, 10, 19, 4, Ink);
  p.box(52, -13, 4, 11, 0, blush);
  p.oval(40, 31, 9, 8, body);
}

// Where the aura's particles live: x, y, and 1 for the ones that pass in front of the buddy.
// Eight rows: an aura never draws more than limits::AuraParticles particles.
static constexpr int motes[][3] = {{-70, -32, 0}, {66, -46, 0}, {-60, 24, 0}, {72, 14, 0},
                                   {-74, -4, 1},  {52, -58, 1}, {74, -18, 1}, {-30, -60, 1}};
// Drifting particles, animated only from the phase counter: no state, no allocation.
template <class P> void buddyAura(P &p, int aura, int front, uint16_t blush, int phase) {
  for (int i = 0; i < 8; ++i) {
    if (aura == 0 || motes[i][2] != front)
      continue;
    int x = motes[i][0], y = motes[i][1], t = (phase + i * 5) % 16;
    if (aura == 1) { // Sparkles swell and fade in place.
      spark(p, x, y, t < 4 ? 2 + t : t < 8 ? 9 - t : 1, i % 2 ? 0xffe0 : 0xffff);
    } else if (aura == 2) { // Hearts float upward.
      heart(p, x, y + 8 - t, i % 2 ? 2 : 3, blush);
    } else if (aura == 3) { // Glitch pixels jump about.
      int hop = int(phase / 2) * 7 + i * 13;
      p.box(x + hop % 11 - 5, y + hop % 7 - 3, 4 + i % 3 * 3, 4, 0,
            i % 3 == 0   ? Mint
            : i % 3 == 1 ? blush
                         : 0xffff);
    } else if (aura == 4) { // Embers rise and cool.
      p.oval(x + (t % 4 < 2 ? 0 : 1), y + 12 - t * 2, t < 10 ? 2 : 1, t < 10 ? 2 : 1,
             t < 6 ? 0xffe0 : 0xfc00);
    } else if (aura == 5) { // Bats flap on the spot.
      int flap = (phase + i) % 4 < 2 ? -5 : 2, by = y + (t < 8 ? t : 16 - t) - 4;
      p.tri(x, by, x - 10, by + flap, x - 4, by + 4, blush);
      p.tri(x, by, x + 10, by + flap, x + 4, by + 4, blush);
      p.oval(x, by + 1, 3, 3, blush);
    } else { // Petals drift down and sideways.
      p.oval(x + (i % 2 ? 1 : -1) * int(t / 2), y - 8 + t, 4, 2, i % 2 ? blush : 0xffff);
    }
  }
}

// The whole buddy, back to front. `rim` draws fat in the outline colour and `p` draws true,
// so running a layer through both gives it a sticker outline; `edge` is a thinner rim for
// small particles. Presets wear nothing: their look is their own.
template <class P>
void buddyArt(P &p, P &rim, P &edge, const Look &look, const Tones &t, int mood, int gaze,
              bool blink, int kit, int phase) {
  int avatar = look.base;
  bool custom = look.custom;
  int eyes = custom ? look.eyes : 0, headwear = custom ? look.headwear : 0;
  int eyewear = custom ? look.eyewear : 0, neckwear = custom ? look.neckwear : 0;
  int mark = custom ? look.mark : 0, back = custom ? look.back : 0;
  int held = custom ? look.held : 0, aura = custom ? look.aura : 0;
  uint16_t dusk = art::mix(t.blush, Ink, 13, 20);
  uint16_t eye = eyeInk(avatar, custom, look.eye, t.body);
  // Glasses lift onto the forehead for love, alarm and cheers so the emotion stays readable.
  int gy = mood == 3 || mood == 4 || mood == 5 ? -25 : -4;
  p.oval(0, 47, 50, 6, 0x18e7);
  if (aura != 3 && aura != 4)
    buddyAura(edge, aura, 0, t.blush, phase);
  buddyAura(p, aura, 0, t.blush, phase);
  buddyBack(rim, back, t.blush, dusk, phase);
  buddyBack(p, back, t.blush, dusk, phase);
  buddyBody(rim, avatar, t.body, t.trim, phase);
  buddyBody(p, avatar, t.body, t.trim, phase);
  buddyDetails(p, avatar, mood, gaze, blink, custom, eyes, look.eye, t.body, t.blush, t.line,
               t.shade, t.light, t.trim, phase);
  buddyMark(p, avatar, mark, avatar == 2 || art::dark(t.body) ? eye : t.line, t.blush);
  buddyNeck(rim, neckwear, t.blush, dusk);
  buddyNeck(p, neckwear, t.blush, dusk);
  buddyEyewear(rim, eyewear, gy, t.blush, dusk, phase);
  buddyEyewear(p, eyewear, gy, t.blush, dusk, phase);
  buddyHead(rim, headwear, t.blush, dusk, phase);
  buddyHead(p, headwear, t.blush, dusk, phase);
  if (kit) {
    buddyKit(rim, kit, mood, t.body, t.blush, phase);
    buddyKit(p, kit, mood, t.body, t.blush, phase);
  } else {
    buddyHeld(rim, held, t.body, t.blush, dusk, phase);
    buddyHeld(p, held, t.body, t.blush, dusk, phase);
  }
  if (aura != 3 && aura != 4)
    buddyAura(edge, aura, 1, t.blush, phase);
  buddyAura(p, aura, 1, t.blush, phase);
}
// <web-art-end>
static_assert(sizeof(motes) / sizeof(motes[0]) == limits::AuraParticles,
              "buddyAura draws one particle per row of the motes table");

// Draws a buddy centred on (cx, cy). G is any canvas with the M5GFX fill calls: the LCD sprite,
// art::Zoom for other sizes, or a test recorder.
template <class G> void drawBuddy(G &g, const Look &look, int cx, int cy, const Pose &pose = {}) {
  cy += pose.bounce;
  Tones t = buddyTones(look.base, look.custom, look.body, look.accent);
  art::Pen<G> p{g, cx, cy, 0, t.line}, rim{g, cx, cy, 2, t.line}, edge{g, cx, cy, 1, t.line};
  buddyArt(p, rim, edge, look, t, pose.mood, pose.gaze, pose.blink, pose.kit, pose.phase);
}
} // namespace dash
