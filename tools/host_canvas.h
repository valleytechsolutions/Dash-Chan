// A tiny software canvas with the subset of the M5Canvas API that the shared drawing headers
// (include/avatars.h, include/home_view.h) use. It exists only so tools/preview.cpp can render
// the real screens on a PC; shapes are close to, not pixel-identical with, the LCD driver.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

extern const unsigned char previewFont[]; // Classic 5x7 font, extracted at build time.

struct HostCanvas {
  int w, h;
  std::vector<uint16_t> px;
  int cx0 = 0, cy0 = 0, cx1, cy1;
  int textSize = 1, tx = 0, ty = 0;
  uint16_t textColor = 0xffff;
  HostCanvas(int width, int height)
      : w(width), h(height), px(width * height), cx1(width), cy1(height) {}
  void setClipRect(int x, int y, int cw, int ch) {
    cx0 = std::max(0, x);
    cy0 = std::max(0, y);
    cx1 = std::min(w, x + cw);
    cy1 = std::min(h, y + ch);
  }
  void clearClipRect() {
    cx0 = cy0 = 0;
    cx1 = w;
    cy1 = h;
  }
  void drawPixel(int x, int y, uint16_t c) {
    if (x >= cx0 && x < cx1 && y >= cy0 && y < cy1)
      px[y * w + x] = c;
  }
  void span(int x0, int x1, int y, uint16_t c) { // Inclusive.
    if (y < cy0 || y >= cy1)
      return;
    x0 = std::max(x0, cx0);
    x1 = std::min(x1, cx1 - 1);
    for (int x = x0; x <= x1; ++x)
      px[y * w + x] = c;
  }
  void fillScreen(uint16_t c) {
    std::fill(px.begin(), px.end(), c);
  }
  void fillRect(int x, int y, int rw, int rh, uint16_t c) {
    for (int j = 0; j < rh; ++j)
      span(x, x + rw - 1, y + j, c);
  }
  void fillRoundRect(int x, int y, int rw, int rh, int r, uint16_t c) {
    r = std::max(0, std::min(r, std::min(rw, rh) / 2));
    for (int j = 0; j < rh; ++j) {
      int inset = 0, d = j < r ? r - j : j >= rh - r ? j - (rh - r - 1) : 0;
      if (d > 0)
        inset = r - int(std::lround(std::sqrt(double(r * r - d * d + r))));
      span(x + std::max(0, inset), x + rw - 1 - std::max(0, inset), y + j, c);
    }
  }
  void fillEllipse(int x, int y, int rx, int ry, uint16_t c) {
    if (rx < 0 || ry < 0)
      return;
    for (int dy = -ry; dy <= ry; ++dy) {
      double t = ry ? double(dy) / (ry + 0.5) : 0;
      int dx = int(std::floor((rx + 0.5) * std::sqrt(std::max(0.0, 1 - t * t))));
      span(x - dx, x + dx, y + dy, c);
    }
  }
  void fillCircle(int x, int y, int r, uint16_t c) {
    fillEllipse(x, y, r, r, c);
  }
  void drawEllipse(int x, int y, int rx, int ry, uint16_t c) {
    int steps = 8 * (rx + ry) + 8;
    for (int i = 0; i < steps; ++i) {
      double a = i * 6.283185307 / steps;
      drawPixel(x + int(std::lround(std::cos(a) * rx)), y + int(std::lround(std::sin(a) * ry)), c);
    }
  }
  void drawCircle(int x, int y, int r, uint16_t c) {
    drawEllipse(x, y, r, r, c);
  }
  void drawLine(int x0, int y0, int x1, int y1, uint16_t c) {
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -std::abs(y1 - y0),
        sy = y0 < y1 ? 1 : -1;
    for (int e = dx + dy;;) {
      drawPixel(x0, y0, c);
      if (x0 == x1 && y0 == y1)
        break;
      int e2 = 2 * e;
      if (e2 >= dy) {
        e += dy;
        x0 += sx;
      }
      if (e2 <= dx) {
        e += dx;
        y0 += sy;
      }
    }
  }
  void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) {
    if (y0 > y1) {
      std::swap(x0, x1);
      std::swap(y0, y1);
    }
    if (y1 > y2) {
      std::swap(x1, x2);
      std::swap(y1, y2);
    }
    if (y0 > y1) {
      std::swap(x0, x1);
      std::swap(y0, y1);
    }
    auto edge = [](int xa, int ya, int xb, int yb, int y) {
      return yb == ya ? double(xa) : xa + double(xb - xa) * (y - ya) / (yb - ya);
    };
    for (int y = y0; y <= y2; ++y) {
      double a = edge(x0, y0, x2, y2, y);
      double b = y < y1 || y1 == y2 ? (y1 == y0 ? double(x1) : edge(x0, y0, x1, y1, y))
                                    : edge(x1, y1, x2, y2, y);
      if (y == y1 && y1 == y0)
        b = x1;
      span(int(std::lround(std::min(a, b))), int(std::lround(std::max(a, b))), y, c);
    }
  }
  void setTextSize(int s) {
    textSize = s;
  }
  void setTextColor(uint16_t c) {
    textColor = c;
  }
  void setCursor(int x, int y) {
    tx = x;
    ty = y;
  }
  void print(const char *s) {
    for (; *s; ++s, tx += 6 * textSize)
      for (int col = 0; col < 5; ++col) {
        unsigned bits = previewFont[(unsigned char)*s * 5 + col];
        for (int row = 0; row < 8; ++row)
          if (bits & (1u << row))
            fillRect(tx + col * textSize, ty + row * textSize, textSize, textSize, textColor);
      }
  }
  bool savePpm(const char *path) const {
    FILE *f = fopen(path, "wb");
    if (!f)
      return false;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (uint16_t c : px) {
      unsigned char rgb[3] = {(unsigned char)(((c >> 11) & 31) * 255 / 31),
                              (unsigned char)(((c >> 5) & 63) * 255 / 63),
                              (unsigned char)((c & 31) * 255 / 31)};
      fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    return true;
  }
  void blit(const HostCanvas &src, int x, int y) {
    for (int j = 0; j < src.h; ++j)
      for (int i = 0; i < src.w; ++i)
        drawPixel(x + i, y + j, src.px[j * src.w + i]);
  }
};
