// Original vector companions. Shared by firmware and the desktop SVG renderer.
#pragma once
#include <initializer_list>
#include <math.h>
#include <stdint.h>
namespace dash {
constexpr uint16_t Ink = 0x1085, Paper = 0xff7a, Mint = 0x87b6, Peach = 0xfdaf, Lilac = 0xc5ff,
                   Blush = 0xfbb2;
template <class G>
void drawBuddy(G &g, int avatar, int cx, int cy, int mood, int gaze, bool blink, int bounce = 0,
               uint16_t customBody = 0, uint16_t customAccent = Blush, int accessory = 0,
               bool custom = false, int eyeStyle = 0, uint16_t eyeColor = Ink, int headwear = -1,
               int eyewear = 0, int neckwear = -1, int field = 0, int phase = 0) {
  cy += bounce;
  uint16_t body = avatar == 0   ? Paper
                  : avatar == 1 ? Mint
                  : avatar == 2 ? Lilac
                  : avatar == 3 ? 0x0000
                                : 0xfa64;
  if (custom)
    body = customBody;
  uint16_t blush = custom ? customAccent : avatar == 3 ? 0xc5ff : avatar == 4 ? 0xff28 : Blush;
  g.fillEllipse(cx, cy + 46, 49, 6, 0x18e7);
  if (avatar == 0) {
    g.fillTriangle(cx - 44, cy - 16, cx - 44, cy - 52, cx - 12, cy - 28, body);
    g.fillTriangle(cx + 44, cy - 16, cx + 44, cy - 52, cx + 12, cy - 28, body);
    g.fillTriangle(cx - 38, cy - 27, cx - 38, cy - 42, cx - 25, cy - 29, blush);
    g.fillTriangle(cx + 38, cy - 27, cx + 38, cy - 42, cx + 25, cy - 29, blush);
    g.fillEllipse(cx, cy, 52, 42, body);
    g.fillEllipse(cx - 26, cy + 36, 17, 10, body);
    g.fillEllipse(cx + 26, cy + 36, 17, 10, body);
  } else if (avatar == 1) {
    g.fillTriangle(cx - 40, cy + 15, cx - 72, cy - 16, cx - 53, cy + 33, 0x35d0);
    g.fillTriangle(cx + 40, cy + 15, cx + 72, cy - 16, cx + 53, cy + 33, 0x35d0);
    g.fillTriangle(cx - 32, cy - 24, cx - 30, cy - 55, cx - 12, cy - 31, Peach);
    g.fillTriangle(cx + 32, cy - 24, cx + 30, cy - 55, cx + 12, cy - 31, Peach);
    g.fillEllipse(cx, cy + 1, 51, 42, body);
    g.fillEllipse(cx, cy + 27, 28, 17, 0xdff6);
    g.fillEllipse(cx - 31, cy + 37, 16, 9, body);
    g.fillEllipse(cx + 31, cy + 37, 16, 9, body);
  } else if (avatar == 2) {
    g.fillRoundRect(cx - 57, cy - 13, 12, 31, 5, 0x8bdb);
    g.fillRoundRect(cx + 45, cy - 13, 12, 31, 5, 0x8bdb);
    g.fillRect(cx - 2, cy - 52, 4, 16, body);
    g.fillCircle(cx, cy - 55, 7, Peach);
    g.fillRoundRect(cx - 48, cy - 36, 96, 77, 22, body);
    g.fillRoundRect(cx - 39, cy - 25, 78, 49, 16, Ink);
    g.fillCircle(cx - 9, cy + 33, 3, Mint);
    g.fillCircle(cx, cy + 33, 3, Peach);
    g.fillCircle(cx + 9, cy + 33, 3, blush);
  } else if (avatar == 3) {
    // Black ghost with a fine charcoal rim: readable on all four themes.
    g.fillEllipse(cx, cy, 49, 43, 0x420a);
    g.fillEllipse(cx, cy, 47, 41, body);
    g.fillTriangle(cx - 44, cy + 13, cx - 51, cy + 42, cx - 12, cy + 33, body);
    g.fillTriangle(cx + 44, cy + 13, cx + 51, cy + 42, cx + 12, cy + 33, body);
    for (int x : {-28, 0, 28})
      g.fillEllipse(cx + x, cy + 35 + (x == 0 ? 4 : 0), 15, 10, body);
    g.fillTriangle(cx - 13, cy - 33, cx + 5, cy - 57, cx + 12, cy - 32, body);
    g.drawLine(cx - 8, cy - 39, cx + 3, cy - 52, 0x420a);
    for (int side : {-1, 1}) {
      int y = cy - 13 - int((phase + side + 8) % 8) * 2;
      g.fillCircle(cx + side * 60, y, 4, 0x420a);
      g.fillCircle(cx + side * 60, y, 2, body);
    }
  } else {
    int flicker = (phase % 6 < 3) ? -2 : 2;
    g.fillTriangle(cx - 38, cy + 12, cx - 33, cy - 43 - flicker, cx - 4, cy - 10, body);
    g.fillTriangle(cx - 24, cy - 12, cx + 3 + flicker, cy - 61, cx + 28, cy + 3, body);
    g.fillTriangle(cx + 11, cy - 3, cx + 43, cy - 40 + flicker, cx + 45, cy + 18, body);
    g.fillEllipse(cx, cy + 7, 47, 36, body);
    g.fillEllipse(cx, cy + 25, 27, 17, blush);
    g.fillTriangle(cx - 18, cy + 25, cx + 3, cy + 2, cx + 20, cy + 27, blush);
    g.fillEllipse(cx - 28, cy + 38, 14, 8, body);
    g.fillEllipse(cx + 28, cy + 38, 14, 8, body);
    for (int side : {-1, 1})
      g.fillCircle(cx + side * 56, cy - 10 - int((phase + side + 8) % 8) * 3, 2, blush);
  }
  uint16_t eye = custom ? eyeColor : avatar == 2 ? Mint : avatar == 3 ? 0xefff : Ink;
  int ey = cy - 4, ex = cx + gaze;
  if (mood == 3) {
    for (int s = -1; s <= 1; s += 2) {
      int x = ex + s * 21;
      g.fillCircle(x - 4, ey - 3, 6, blush);
      g.fillCircle(x + 4, ey - 3, 6, blush);
      g.fillTriangle(x - 10, ey, x + 10, ey, x, ey + 12, blush);
    }
  } else if (blink || mood == 2) {
    g.fillRoundRect(ex - 30, ey, 18, 4, 2, eye);
    g.fillRoundRect(ex + 12, ey, 18, 4, 2, eye);
  } else {
    for (int side : {-1, 1}) {
      int x = ex + side * 21;
      if (mood == 4 || eyeStyle == 0 || (eyeStyle == 5 && side < 0)) {
        g.fillEllipse(x, ey, 8, mood == 4 ? 16 : 12, eye);
        g.fillCircle(x - 2, ey - 5, 3, 0xffff);
      } else if (eyeStyle == 1) { // Four-point sparkle.
        g.fillTriangle(x, ey - 14, x - 10, ey, x + 10, ey, eye);
        g.fillTriangle(x - 10, ey, x + 10, ey, x, ey + 14, eye);
        g.fillCircle(x, ey, 3, 0xffff);
      } else if (eyeStyle == 2) { // Relaxed lids.
        g.fillRoundRect(x - 10, ey - 1, 20, 5, 2, eye);
        g.fillEllipse(x, ey + 5, 5, 4, eye);
      } else if (eyeStyle == 3) { // Cat irises with a vertical pupil.
        g.fillEllipse(x, ey, 10, 12, eye);
        g.fillEllipse(x, ey, 3, 10, avatar == 2 ? Paper : Ink);
        g.fillCircle(x - 4, ey - 5, 2, 0xffff);
      } else if (eyeStyle == 4) {
        g.fillRoundRect(x - 9, ey - 10, 18, 20, 3, eye);
        g.fillRect(x - 5, ey - 6, 5, 5, 0xffff);
      } else { // Wink.
        g.drawLine(x - 9, ey + 2, x, ey - 3, eye);
        g.drawLine(x, ey - 3, x + 9, ey + 2, eye);
      }
    }
  }
  if (avatar == 0 || avatar == 1 || avatar == 4) {
    g.fillEllipse(cx - 36, cy + 12, 8, 4, blush);
    g.fillEllipse(cx + 36, cy + 12, 8, 4, blush);
  }
  if (mood == 4) {
    g.fillEllipse(cx, cy + 22, 7, 10, eye);
    g.fillTriangle(cx + 58, cy - 26, cx + 64, cy - 11, cx + 52, cy - 11, 0x55ff);
  } else {
    g.drawLine(cx - 8, cy + 17, cx - 4, cy + 21, eye);
    g.drawLine(cx - 4, cy + 21, cx, cy + 18, eye);
    g.drawLine(cx, cy + 18, cx + 4, cy + 21, eye);
    g.drawLine(cx + 4, cy + 21, cx + 8, cy + 17, eye);
  }
  if (headwear < 0)
    headwear = accessory == 1 ? 1 : accessory == 3 ? 2 : 0;
  if (neckwear < 0)
    neckwear = accessory == 2 ? 1 : 0;
  // Cosmetic glasses lift onto the forehead for panic/love so emotions stay readable.
  int glassesY = cy + ((mood == 3 || mood == 4) ? -25 : -4);
  if (eyewear == 1) {
    for (int side : {-1, 1}) {
      int x = cx + side * 21;
      for (int a = 0; a < 16; ++a) {
        double angle = a * 6.2831853 / 16;
        g.fillCircle(x + int(cos(angle) * 13), glassesY + int(sin(angle) * 14), 1, blush);
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
