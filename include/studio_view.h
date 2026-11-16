// SPDX-License-Identifier: GPL-3.0-or-later
// The on-device Avatar Studio: where things sit on the 320x240 screen, what each button is
// called and what a tap lands on. Drawing (src/ui.cpp) and touch both read the same boxes, so
// a button can never be drawn in one place and listen in another. No hardware calls: tested
// on a PC.
//
//    8..133   live preview of the buddy        140..311  four option buttons (one page of three)
//  196..231   FACE / OUTFIT / EXTRAS page tabs and DONE
#pragma once
#include "avatar_limits.h"
#include "avatars.h"
#include "themes.h"
#include <stdio.h>
#include <string.h>
namespace dash {
namespace studioview {
struct Box {
  int x, y, w, h;
  constexpr bool has(int px, int py) const {
    return px >= x && px < x + w && py >= y && py < y + h;
  }
};
constexpr int Pages = 3, Rows = 4;
constexpr Box Preview{8, 40, 126, 150};
constexpr float PreviewScale = 0.68f; // The whole buddy, wings and hat included, fits the box.
constexpr Box option(int row) {
  return {140, 40 + row * 38, 172, 34};
}
constexpr Box tab(int page) {
  return {8 + page * 78, 196, 74, 36};
}
constexpr Box Done{242, 196, 70, 36};
constexpr const char *PageNames[Pages] = {"FACE", "OUTFIT", "EXTRAS"};

// Everything the studio can change. Colours and the name stay with the web studio.
enum class Field : uint8_t {
  Shape,
  Eyes,
  Mark,
  Pack,
  Head,
  Glasses,
  Neck,
  Back,
  Held,
  Aura,
  Persona,
  Kit
};
constexpr Field PageFields[Pages][Rows] = {
    {Field::Shape, Field::Eyes, Field::Mark, Field::Pack},
    {Field::Head, Field::Glasses, Field::Neck, Field::Back},
    {Field::Held, Field::Aura, Field::Persona, Field::Kit},
};
constexpr unsigned KitModes = 3; // Auto / Always / Off.
constexpr unsigned choices(Field f) {
  return f == Field::Shape     ? limits::Bases
         : f == Field::Eyes    ? limits::Eyes
         : f == Field::Mark    ? limits::Marks
         : f == Field::Pack    ? limits::Packs
         : f == Field::Head    ? limits::Headwear
         : f == Field::Glasses ? limits::Eyewear
         : f == Field::Neck    ? limits::Neckwear
         : f == Field::Back    ? limits::Backs
         : f == Field::Held    ? limits::Held
         : f == Field::Aura    ? limits::Auras
         : f == Field::Persona ? limits::Personas
                               : KitModes;
}
// Names are short on purpose: a button is 172 px wide, so title and name share 28 characters.
constexpr const char *Shapes[limits::Bases] = {"Mochi", "Sprout", "Orbit", "Wisp", "Ember"};
constexpr const char *EyeNames[limits::Eyes] = {"Classic",  "Sparkle", "Sleepy", "Cat",
                                                "Pixel",    "Wink",    "Heart",  "Star",
                                                "Glitch X", "Half-lid"};
constexpr const char *MarkNames[limits::Marks] = {"None", "Blush hearts", "Star cheek",  "Whiskers",
                                                  "Tear", "Circuit",      "Stitch scar", "Barcode"};
// Pack 0 is not a pack: on this screen it means "the look I walked in with".
constexpr const char *PackNames[limits::Packs] = {"My own look", "Kawaii pastel", "Cyberpunk neon",
                                                  "Goth",        "Vaporwave",     "Retro terminal"};
constexpr const char *HeadNames[limits::Headwear] = {
    "None",           "Bow",       "Crown", "Beanie", "Flower",       "Scout cap",
    "Cat headphones", "Witch hat", "Halo",  "Horns",  "Antenna band", "Hood"};
constexpr const char *GlassesNames[limits::Eyewear] = {
    "None", "Round", "Shades", "Visor", "Heart shades", "Monocle", "Eyepatch", "Scan HUD"};
constexpr const char *NeckNames[limits::Neckwear] = {
    "None", "Scarf", "Bandana", "Bow tie", "Choker", "Spiked collar", "Bell collar", "Headphones"};
constexpr const char *BackNames[limits::Backs] = {
    "None", "Angel wings", "Bat wings", "Cape", "Antenna pack", "Jet thrusters", "Fairy wings"};
constexpr const char *HeldNames[limits::Held] = {"None",     "Boba tea", "Lollipop",
                                                 "Game pad", "Lantern",  "Plush bear"};
constexpr const char *AuraNames[limits::Auras] = {"None",   "Sparkles", "Hearts", "Glitch pixels",
                                                  "Embers", "Bats",     "Petals"};
constexpr const char *PersonaNames[limits::Personas] = {"Its shape", "Mochi", "Sprout",
                                                        "Orbit",     "Wisp",  "Ember"};
constexpr const char *KitNames[KitModes] = {"Auto", "Always", "Off"};
constexpr const char *title(Field f) {
  return f == Field::Shape     ? "Shape"
         : f == Field::Eyes    ? "Eyes"
         : f == Field::Mark    ? "Mark"
         : f == Field::Pack    ? "Style"
         : f == Field::Head    ? "Head"
         : f == Field::Glasses ? "Glasses"
         : f == Field::Neck    ? "Neck"
         : f == Field::Back    ? "Back"
         : f == Field::Held    ? "Holding"
         : f == Field::Aura    ? "Aura"
         : f == Field::Persona ? "Talks like"
                               : "Field kit";
}
constexpr const char *name(Field f, unsigned value) {
  return value >= choices(f)   ? "?"
         : f == Field::Shape   ? Shapes[value]
         : f == Field::Eyes    ? EyeNames[value]
         : f == Field::Mark    ? MarkNames[value]
         : f == Field::Pack    ? PackNames[value]
         : f == Field::Head    ? HeadNames[value]
         : f == Field::Glasses ? GlassesNames[value]
         : f == Field::Neck    ? NeckNames[value]
         : f == Field::Back    ? BackNames[value]
         : f == Field::Held    ? HeldNames[value]
         : f == Field::Aura    ? AuraNames[value]
         : f == Field::Persona ? PersonaNames[value]
                               : KitNames[value];
}
constexpr unsigned ButtonChars = 28;
inline void label(char *out, size_t size, Field f, unsigned value) {
  snprintf(out, size, "%s: %s", title(f), name(f, value));
}
// Tapping an option steps to the next choice and wraps back to the first.
constexpr unsigned next(Field f, unsigned value) {
  return value + 1 < choices(f) ? value + 1 : 0;
}

enum class Hit : uint8_t { None, Option, Tab, Done };
struct Target {
  Hit hit = Hit::None;
  int index = 0; // Row for Option, page for Tab.
};
constexpr Target tap(int x, int y) {
  for (int row = 0; row < Rows; ++row)
    if (option(row).has(x, y))
      return {Hit::Option, row};
  for (int page = 0; page < Pages; ++page)
    if (tab(page).has(x, y))
      return {Hit::Tab, page};
  if (Done.has(x, y))
    return {Hit::Done, 0};
  return {};
}
} // namespace studioview

// A snapshot of what the studio page shows. Like the home screen, it is drawn by a template so
// the firmware and the PC preview (tools/preview.cpp) render the very same page.
struct StudioView {
  uint32_t now = 0;
  Palette colors{};
  Theme theme = Theme::Dark;
  Look look; // The custom buddy, even while a preset is the active avatar.
  const char *name = "My buddy";
  unsigned page = 0;
  unsigned values[studioview::Rows] = {}; // The current choice for each row of this page.
  int kit = 0;                            // 1 shows the field kit instead of the held item.
};
template <class G> void drawStudio(G &g, const StudioView &v) {
  namespace sv = studioview;
  auto words = [&](const char *s, int x, int y, uint16_t color) {
    g.setTextSize(1);
    g.setTextColor(color);
    g.setCursor(x, y);
    g.print(s);
  };
  auto button = [&](const sv::Box &b, const char *label, bool active) {
    g.fillRoundRect(b.x, b.y, b.w, b.h, 9, active ? v.colors.accent : v.colors.panel);
    words(label, b.x + b.w / 2 - int(strlen(label)) * 3, b.y + b.h / 2 - 3,
          active ? (v.theme == Theme::Light ? 0xffff : Ink) : v.colors.text);
  };
  words("AVATAR STUDIO", 12, 12, v.colors.accent);
  button({254, 5, 58, 27}, "BACK", false);
  const sv::Box &pv = sv::Preview;
  g.fillRoundRect(pv.x, pv.y, pv.w, pv.h, 10, v.colors.panel);
  g.setClipRect(pv.x, pv.y, pv.w, pv.h);
  art::Zoom<G> zoom{g, pv.x + pv.w / 2, pv.y + 72, sv::PreviewScale};
  Pose pose;
  pose.blink = v.now % 4000 < 100;
  pose.kit = v.kit;
  pose.phase = int(v.now / 150);
  drawBuddy(zoom, v.look, 0, 0, pose);
  g.clearClipRect();
  char s[48];
  snprintf(s, sizeof(s), "%.16s", v.name);
  words(s, pv.x + pv.w / 2 - int(strlen(s)) * 3, pv.y + pv.h - 15, v.colors.text);
  for (int row = 0; row < sv::Rows; ++row) {
    sv::label(s, sizeof(s), sv::PageFields[v.page % sv::Pages][row], v.values[row]);
    button(sv::option(row), s, false);
  }
  for (int i = 0; i < sv::Pages; ++i)
    button(sv::tab(i), sv::PageNames[i], unsigned(i) == v.page);
  button(sv::Done, "DONE", false);
  g.fillRoundRect(sv::Done.x + 5, sv::Done.y + 9, 3, sv::Done.h - 18, 1, v.colors.heart);
}
} // namespace dash
