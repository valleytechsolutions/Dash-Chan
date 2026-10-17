#pragma once
#include <stdint.h>
namespace dash {
enum class Theme : uint8_t { Dark, Light, Matrix, Bwm };
struct Palette {
  uint16_t bg, panel, text, muted, accent, danger, heart, grid;
};
constexpr uint16_t rgb565(uint32_t c) {
  return ((c >> 8) & 0xf800) | ((c >> 5) & 0x07e0) | ((c >> 3) & 0x001f);
}
inline Palette palette(Theme theme) {
  switch (theme) {
  case Theme::Light:
    return {rgb565(0xf7f6f1), rgb565(0xe4e8ec), rgb565(0x172638), rgb565(0x536172),
            rgb565(0x087d63), rgb565(0xb32941), rgb565(0xcb4774), rgb565(0xdbe4de)};
  case Theme::Matrix:
    return {rgb565(0x020603), rgb565(0x0b1d10), rgb565(0xcaffd1), rgb565(0x68ad78),
            rgb565(0x31f875), rgb565(0xffce59), rgb565(0x92ffb5), rgb565(0x0a2a12)};
  case Theme::Bwm:
    return {rgb565(0x0b090a), rgb565(0x251518), rgb565(0xfff1eb), rgb565(0xb5a1a5),
            rgb565(0xff3434), rgb565(0xffbe52), rgb565(0xff7795), rgb565(0x290e12)};
  default:
    return {rgb565(0x101425), rgb565(0x242c47), rgb565(0xf3f4ff), rgb565(0xa5afcb),
            rgb565(0x9af6ce), rgb565(0xffb572), rgb565(0xff88b6), rgb565(0x1b2537)};
  }
}
inline const char *themeName(Theme theme) {
  static const char *names[] = {"Dark", "Light", "Matrix", "BWM"};
  return names[unsigned(theme) % 4];
}
} // namespace dash
