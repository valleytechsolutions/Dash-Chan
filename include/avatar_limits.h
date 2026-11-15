// SPDX-License-Identifier: GPL-3.0-or-later
// How many choices each Avatar Studio slot has. This is the one place those counts live: the
// profile parser, saved settings, the on-device studio, NFC outfit cards and the website's
// tests all read them from here, so a new option cannot be half-supported.
#pragma once
#include <stdint.h>
namespace dash {
namespace limits {
constexpr unsigned Bases = 5;     // Mochi, Sprout, Orbit, Wisp, Ember.
constexpr unsigned Eyes = 10;     // Classic .. Wink, Heart, Star, Glitch X, Half-lid.
constexpr unsigned Headwear = 12; // None .. Scout cap, Cat-ear headphones .. Hood.
constexpr unsigned Eyewear = 8;   // None .. Cyber visor, Heart shades .. Scan-line HUD.
constexpr unsigned Neckwear = 8;  // None .. Bow tie, Choker .. Neck headphones.
constexpr unsigned Marks = 8;     // None, Blush hearts .. Barcode.
constexpr unsigned Backs = 7;     // None, Angel wings .. Fairy wings.
constexpr unsigned Held = 6;      // None, Boba .. Plush.
constexpr unsigned Auras = 7;     // None, Sparkles .. Petals.
constexpr unsigned Packs = 6;     // None, Kawaii pastel .. Retro terminal.
constexpr unsigned Personas = 6;  // Match shape, then speak as Mochi .. Ember.
constexpr unsigned NameChars = 16;
constexpr unsigned AuraParticles = 8; // Hard cap, so an aura costs a fixed amount to draw.
// What the two older profile formats may carry. They never grow: firmware that only knows them
// must keep accepting every line the website still writes in them.
constexpr unsigned V2Eyes = 6, V2Headwear = 6, V2Eyewear = 4, V2Neckwear = 4;
constexpr unsigned V1Bases = 3, V1Accessories = 4;
constexpr unsigned Schema = 3;        // Newest profile line this firmware reads (DCAV3).
constexpr unsigned ProfileChars = 80; // Longest profile line any format can produce is 75.
} // namespace limits
// Brings a saved or received number back into range. Unknown values fall back to "none"
// rather than wrapping onto an unrelated option.
constexpr uint8_t inRange(unsigned value, unsigned count) {
  return uint8_t(value < count ? value : 0);
}
// Whose lines a buddy speaks: its own shape's, unless a custom buddy picked a persona.
constexpr unsigned speaker(bool custom, unsigned persona, unsigned base) {
  return custom && persona >= 1 && persona < limits::Personas ? persona - 1 : base;
}
} // namespace dash
