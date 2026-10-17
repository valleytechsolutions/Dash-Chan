// Original vector companions. Shared by firmware and the desktop SVG renderer.
#pragma once
#include <stdint.h>
namespace dash {
constexpr uint16_t Ink = 0x1085, Paper = 0xff7a, Mint = 0x87b6, Peach = 0xfdaf, Lilac = 0xc5ff,
                   Blush = 0xfbb2;
template <class G>
void drawBuddy(G &g, int avatar, int cx, int cy, int mood, int gaze, bool blink, int bounce = 0,
               uint16_t customBody = 0, uint16_t customAccent = Blush, int accessory = 0,
               bool custom = false) {
  cy += bounce;
  uint16_t body = avatar == 0 ? Paper : avatar == 1 ? Mint : Lilac;
  if (custom)
    body = customBody;
  uint16_t blush = custom ? customAccent : Blush;
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
  } else {
    g.fillRoundRect(cx - 57, cy - 13, 12, 31, 5, 0x8bdb);
    g.fillRoundRect(cx + 45, cy - 13, 12, 31, 5, 0x8bdb);
    g.fillRect(cx - 2, cy - 52, 4, 16, body);
    g.fillCircle(cx, cy - 55, 7, Peach);
    g.fillRoundRect(cx - 48, cy - 36, 96, 77, 22, body);
    g.fillRoundRect(cx - 39, cy - 25, 78, 49, 16, Ink);
    g.fillCircle(cx - 9, cy + 33, 3, Mint);
    g.fillCircle(cx, cy + 33, 3, Peach);
    g.fillCircle(cx + 9, cy + 33, 3, blush);
  }
  uint16_t eye = avatar == 2 ? Mint : Ink;
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
    int h = mood == 4 ? 16 : 12;
    g.fillEllipse(ex - 21, ey, 8, h, eye);
    g.fillEllipse(ex + 21, ey, 8, h, eye);
    g.fillCircle(ex - 23, ey - 5, 3, avatar == 2 ? Paper : 0xffff);
    g.fillCircle(ex + 19, ey - 5, 3, avatar == 2 ? Paper : 0xffff);
  }
  if (avatar != 2) {
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
  if (accessory == 1) { // A tiny bow, never obscuring the eyes.
    g.fillTriangle(cx - 6, cy - 34, cx - 23, cy - 44, cx - 23, cy - 24, blush);
    g.fillTriangle(cx + 6, cy - 34, cx + 23, cy - 44, cx + 23, cy - 24, blush);
    g.fillCircle(cx, cy - 34, 6, blush);
  } else if (accessory == 2) {
    g.fillRect(cx - 37, cy + 31, 74, 8, blush);
    g.fillRoundRect(cx + 22, cy + 31, 12, 22, 4, blush);
  } else if (accessory == 3) {
    g.fillRect(cx - 22, cy - 42, 44, 10, Peach);
    g.fillTriangle(cx - 22, cy - 42, cx - 22, cy - 59, cx - 8, cy - 42, Peach);
    g.fillTriangle(cx - 10, cy - 42, cx, cy - 61, cx + 10, cy - 42, Peach);
    g.fillTriangle(cx + 8, cy - 42, cx + 22, cy - 59, cx + 22, cy - 42, Peach);
  }
}
} // namespace dash
