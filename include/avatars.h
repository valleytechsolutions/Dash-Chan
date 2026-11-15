// Original vector companions. Shared by firmware and the desktop SVG renderer.
#pragma once
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

// The two functions between the web-art markers are also the website's buddy renderer:
// tools/web_art.py converts them to site/buddy_art.mjs. Keep them to plain statements on `p`
// (oval, box, tri, line2), int/bool/uint16_t locals, if/else, and simple for loops.
// <web-art-begin>
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
  uint16_t eye = custom ? eyeColor : avatar == 2 ? Mint : avatar == 3 ? 0xefff : Ink;
  // A custom face must stay readable on whatever is behind it (Orbit's screen or the body).
  uint16_t surface = avatar == 2 ? 0x10c7 : body;
  if (custom && art::dark(eye) && art::dark(surface))
    eye = avatar == 2 ? Mint : 0xefff;
  if (custom && art::pale(eye) && art::pale(surface))
    eye = Ink;
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
      p.oval(x + gaze / 2, ey, 3, 3, avatar == 2 ? eye : Ink);
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
      } else { // Wink.
        p.line2(x - 9, ey + 2, x, ey - 3, eye);
        p.line2(x, ey - 3, x + 9, ey + 2, eye);
      }
      if (mood == 6) // Pout: heavy, unimpressed lids.
        p.box(x - 11, ey - 16, 22, 10, 0, lid);
    }
  }
  if (avatar == 0 || avatar == 1 || avatar == 4)
    for (int s : {-1, 1}) // Hand-drawn blush hatching instead of pink ovals.
      for (int i = -1; i <= 1; ++i)
        p.line2(s * 37 + i * 5 - 2, 15, s * 37 + i * 5 + 2, 10, avatar == 4 ? 0xfa00 : blush);
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
// <web-art-end>

template <class G>
void drawBuddy(G &g, int avatar, int cx, int cy, int mood, int gaze, bool blink, int bounce = 0,
               uint16_t customBody = 0, uint16_t customAccent = Blush, int accessory = 0,
               bool custom = false, int eyeStyle = 0, uint16_t eyeColor = Ink, int headwear = -1,
               int eyewear = 0, int neckwear = -1, int field = 0, int phase = 0) {
  cy += bounce;
  Tones t = buddyTones(avatar, custom, customBody, customAccent);
  uint16_t body = t.body, blush = t.blush;
  g.fillEllipse(cx, cy + 47, 50, 6, 0x18e7);
  art::Pen<G> rim{g, cx, cy, 2, t.line}, p{g, cx, cy, 0, t.line};
  buddyBody(rim, avatar, body, t.trim, phase);
  buddyBody(p, avatar, body, t.trim, phase);
  buddyDetails(p, avatar, mood, gaze, blink, custom, eyeStyle, eyeColor, body, blush, t.line,
               t.shade, t.light, t.trim, phase);
  if (headwear < 0)
    headwear = accessory == 1 ? 1 : accessory == 3 ? 2 : 0;
  if (neckwear < 0)
    neckwear = accessory == 2 ? 1 : 0;
  // Cosmetic glasses lift onto the forehead for panic/love so emotions stay readable.
  int glassesY = cy + ((mood == 3 || mood == 4 || mood == 5) ? -25 : -4);
  if (eyewear == 1) {
    for (int side : {-1, 1}) {
      int x = cx + side * 21;
      for (int a = 0; a < 48; ++a) { // Dense enough to read as a solid frame at any zoom.
        double angle = a * 6.2831853 / 48;
        g.fillCircle(x + int(lround(cos(angle) * 13)), glassesY + int(lround(sin(angle) * 14)), 1,
                     blush);
      }
    }
    g.drawLine(cx - 8, glassesY, cx + 8, glassesY, blush);
  } else if (eyewear == 2 || eyewear == 3) {
    g.fillRoundRect(cx - 39, glassesY - 12, 78, 24, 6, blush);
    if (eyewear == 2) {
      g.fillRoundRect(cx - 35, glassesY - 8, 31, 17, 4, Ink);
      g.fillRoundRect(cx + 4, glassesY - 8, 31, 17, 4, Ink);
      g.drawLine(cx - 28, glassesY - 5, cx - 20, glassesY + 3, Paper);
      g.drawLine(cx + 12, glassesY - 5, cx + 20, glassesY + 3, Paper);
    } else {
      g.fillRoundRect(cx - 35, glassesY - 8, 70, 16, 4, Ink);
      for (int x = -27; x <= 27; x += 9)
        g.fillRect(cx + x, glassesY - 3, 4, 6, Mint);
    }
  }
  if (headwear == 1) { // A tiny bow, never obscuring the eyes.
    g.fillTriangle(cx - 6, cy - 34, cx - 23, cy - 44, cx - 23, cy - 24, blush);
    g.fillTriangle(cx + 6, cy - 34, cx + 23, cy - 44, cx + 23, cy - 24, blush);
    g.fillCircle(cx, cy - 34, 6, blush);
  } else if (headwear == 2) {
    g.fillRect(cx - 22, cy - 42, 44, 10, Peach);
    g.fillTriangle(cx - 22, cy - 42, cx - 22, cy - 59, cx - 8, cy - 42, Peach);
    g.fillTriangle(cx - 10, cy - 42, cx, cy - 61, cx + 10, cy - 42, Peach);
    g.fillTriangle(cx + 8, cy - 42, cx + 22, cy - 59, cx + 22, cy - 42, Peach);
  } else if (headwear == 3 || headwear == 5) {
    g.fillEllipse(cx, cy - 38, 35, 17, blush);
    g.fillRoundRect(cx - 38, cy - 38, headwear == 5 ? 83 : 76, 10, 4, blush);
    if (headwear == 3)
      g.fillCircle(cx, cy - 57, 7, Peach);
    else
      g.fillCircle(cx + 8, cy - 44, 5, Paper);
  } else if (headwear == 4) {
    for (int i = 0; i < 5; ++i) {
      double a = i * 6.2831853 / 5;
      g.fillCircle(cx + 29 + int(cos(a) * 9), cy - 33 + int(sin(a) * 9), 6, blush);
    }
    g.fillCircle(cx + 29, cy - 33, 5, Peach);
  }
  if (neckwear == 1) {
    g.fillRect(cx - 37, cy + 31, 74, 8, blush);
    g.fillRoundRect(cx + 22, cy + 31, 12, 22, 4, blush);
  } else if (neckwear == 2) {
    g.fillTriangle(cx - 30, cy + 30, cx + 30, cy + 30, cx, cy + 48, blush);
    g.fillCircle(cx, cy + 36, 3, Paper);
  } else if (neckwear == 3) {
    g.fillTriangle(cx - 4, cy + 33, cx - 21, cy + 24, cx - 21, cy + 42, blush);
    g.fillTriangle(cx + 4, cy + 33, cx + 21, cy + 24, cx + 21, cy + 42, blush);
    g.fillCircle(cx, cy + 33, 5, Peach);
  }
  if (field) {
    // A toy Yagi and handheld receiver, with paws wrapped around both.
    // Animation is decorative: never infer bearing from antenna orientation.
    uint16_t signal = mood == 4 ? 0xf9c7 : Mint;
    int lift = field == 2 ? (phase % 6 < 3 ? -2 : 1) : 0;
    int ax = cx - 61, ay = cy + 6 + lift;
    g.drawLine(ax + 11, ay + 21, ax - 5, ay - 14, Paper);
    g.drawLine(ax - 16, ay - 10, ax + 12, ay - 20, Paper);
    g.drawLine(ax - 12, ay - 2, ax + 15, ay - 12, Paper);
    g.drawLine(ax - 7, ay + 6, ax + 17, ay - 4, Paper);
    g.fillRoundRect(ax + 6, ay + 15, 8, 18, 3, blush);
    g.fillEllipse(cx - 45, cy + 29 + lift, 10, 8, body);
    g.drawLine(cx + 52, cy - 3, cx + 58, cy + 13, Paper);
    g.fillRoundRect(cx + 39, cy + 10, 32, 37, 5, blush);
    g.fillRoundRect(cx + 43, cy + 14, 24, 23, 4, Ink);
    // Sweeping screen and equalizer stand in for listening gear.
    int sweep = phase % 8;
    g.drawLine(cx + 55, cy + 32, cx + 46 + sweep * 2, cy + 17, signal);
    g.fillCircle(cx + 59, cy + 23, 2, signal);
    g.fillCircle(cx + 49, cy + 41, 2, Paper);
    g.fillCircle(cx + 59, cy + 41, 2, signal);
    g.drawLine(cx + 66, cy + 11, cx + 59, cy - 9, blush);
    g.fillRoundRect(cx + 49, cy - 17, 10, 19, 4, Ink);
    g.fillRect(cx + 52, cy - 13, 4, 11, blush);
    g.fillEllipse(cx + 40, cy + 31, 9, 8, body);
  }
}
} // namespace dash
