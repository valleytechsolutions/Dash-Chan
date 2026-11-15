// PC preview of the buddy art and home screen. Run through tools/preview.py.
#include "audio_view.h"
#include "avatar_limits.h"
#include "core.h"
#include "home_view.h"
#include "host_canvas.h"
#include "profile.h"
#include "studio_view.h"
#include <string>
using namespace dash;
static std::string out;
static HostCanvas sheetOf(int columns, int rows) {
  return HostCanvas(columns * 324 + 4, rows * 244 + 4);
}
static void place(HostCanvas &sheet, int column, int row, const HomeView &v) {
  HostCanvas screen(320, 240);
  screen.fillScreen(v.colors.bg);
  drawHome(screen, v);
  sheet.blit(screen, 4 + column * 324, 4 + row * 244);
}
static HomeView base(Theme theme = Theme::Dark) {
  HomeView v;
  v.now = 61000;
  v.theme = theme;
  v.colors = palette(theme);
  v.level = 4;
  v.xp = 19;
  return v;
}
// A custom buddy in neutral colours, so a gear sheet shows the gear and not the palette.
static Look plain(int shape) {
  static const uint32_t bodies[] = {0xfff3d2, 0xa5edc0, 0xc9c2ff, 0x17141f, 0xff6a3d};
  static const uint32_t eyes[] = {0x101029, 0x245549, 0x9affdd, 0xedffff, 0x201029};
  Look look;
  look.custom = true;
  look.base = shape;
  look.body = rgb565(bodies[shape]);
  look.accent = rgb565(shape == 3 ? 0xb18cff : 0xff5c8a);
  look.eye = rgb565(eyes[shape]);
  return look;
}
// One sheet per studio slot: a row per shape, a column per option, on a mid-tone backdrop.
// Wide slots are split so every cell stays at the LCD's own scale.
static void slotSheet(const char *name, int count, int Look::*slot) {
  const int w = 184, h = 140, perSheet = 6;
  for (int first = 0; first < count; first += perSheet) {
    int columns = count - first < perSheet ? count - first : perSheet;
    HostCanvas sheet(columns * w, 5 * h);
    sheet.fillScreen(0x3a4f);
    for (int shape = 0; shape < 5; ++shape)
      for (int i = 0; i < columns; ++i) {
        Look look = plain(shape);
        look.*slot = first + i;
        Pose pose;
        pose.phase = 3;
        drawBuddy(sheet, look, w / 2 + i * w, 72 + shape * h, pose);
      }
    sheet.savePpm((out + "/slot-" + name + (first ? "-2" : "") + ".ppm").c_str());
  }
}
// A style pack's signature look on a given shape, as the robot would show it.
static Look packLook(unsigned pack, int shape) {
  const PackStyle &style = PackStyles[pack];
  Look look;
  look.custom = true;
  look.base = shape;
  look.body = rgb565(style.body);
  look.accent = rgb565(style.accent);
  look.eye = rgb565(style.eye);
  look.eyes = style.eyes;
  look.headwear = style.headwear;
  look.eyewear = style.eyewear;
  look.neckwear = style.neckwear;
  look.mark = style.mark;
  look.back = style.back;
  look.held = style.held;
  look.aura = style.aura;
  return look;
}
// Every pack on every shape, on the real home screen: Recon view, then Full Face.
static void packSheets() {
  for (int face = 0; face < 2; ++face)
    for (int half = 0; half < 2; ++half) {
      int columns = half ? 2 : 3;
      HostCanvas sheet = sheetOf(columns, 5);
      for (unsigned pack = 1; pack < limits::Packs; ++pack)
        for (int i = 0; i < columns; ++i) {
          HomeView v = base(pack == 4 && face ? Theme::Light : Theme::Dark);
          v.face = face;
          v.look = packLook(pack, half * 3 + i);
          v.name = PackStyles[pack].name;
          v.backdrop = PackStyles[pack].backdrop;
          v.mood = face && i == 1 ? 3 : 0;
          if (!face && i == 2) {
            v.speaking = true;
            v.message = "New fit. Same little signal goblin.";
          }
          place(sheet, i, int(pack) - 1, v);
        }
      sheet.savePpm(
          (out + "/packs-" + (face ? "face" : "recon") + (half ? "-2" : "") + ".ppm").c_str());
    }
}
// The Full Face close-up (2.1x) of the options that sit on the face, where detail matters most.
static void closeUps() {
  struct Pick {
    int Look::*slot;
    int first, last;
  };
  const Pick picks[] = {
      {&Look::eyes, 6, 9}, {&Look::eyewear, 4, 7}, {&Look::mark, 1, 7}, {&Look::eyewear, 1, 1}};
  const int w = 320, h = 152;
  HostCanvas sheet(4 * w, 4 * h);
  sheet.fillScreen(0x3a4f);
  int cell = 0;
  for (const Pick &pick : picks)
    for (int option = pick.first; option <= pick.last; ++option, ++cell) {
      int x = cell % 4 * w, y = cell / 4 * h;
      Look look = plain(cell % 5);
      look.*pick.slot = option;
      sheet.setClipRect(x + 1, y + 1, w - 2, h - 2);
      art::Zoom<HostCanvas> zoom{sheet, x + w / 2, y + 72, 2.1f};
      Pose pose;
      pose.phase = 3;
      drawBuddy(zoom, look, 0, 0, pose);
      sheet.clearClipRect();
    }
  sheet.savePpm((out + "/closeups.ppm").c_str());
}
// The on-device Avatar Studio: its three pages, in two themes.
static void studioScreens() {
  HostCanvas sheet = sheetOf(3, 2);
  for (int row = 0; row < 2; ++row)
    for (unsigned page = 0; page < 3; ++page) {
      StudioView v;
      v.now = 61000;
      v.theme = row ? Theme::Light : Theme::Dark;
      v.colors = palette(v.theme);
      unsigned pack = row ? 1 : 3;
      v.look = packLook(pack, row ? 0 : 3);
      v.name = row ? "Peaches" : "Nyx the 2nd";
      v.page = page;
      const PackStyle &style = PackStyles[pack];
      const unsigned values[3][4] = {{unsigned(v.look.base), style.eyes, style.mark, pack},
                                     {style.headwear, style.eyewear, style.neckwear, style.back},
                                     {style.held, style.aura, row ? 0u : 4u, 0u}};
      for (int i = 0; i < 4; ++i)
        v.values[i] = values[page][i];
      HostCanvas screen(320, 240);
      screen.fillScreen(v.colors.bg);
      drawStudio(screen, v);
      sheet.blit(screen, 4 + int(page) * 324, 4 + row * 244);
      if (!row && page == 1)
        screen.savePpm((out + "/ui-studio.ppm").c_str());
    }
  sheet.savePpm((out + "/studio.ppm").c_str());
}
int main(int argc, char **argv) {
  out = argc > 1 ? argv[1] : ".";
  studioScreens();
  packSheets();
  closeUps();
  slotSheet("eyes", limits::Eyes, &Look::eyes);
  slotSheet("headwear", limits::Headwear, &Look::headwear);
  slotSheet("eyewear", limits::Eyewear, &Look::eyewear);
  slotSheet("neckwear", limits::Neckwear, &Look::neckwear);
  slotSheet("mark", limits::Marks, &Look::mark);
  slotSheet("back", limits::Backs, &Look::back);
  slotSheet("held", limits::Held, &Look::held);
  slotSheet("aura", limits::Auras, &Look::aura);
  HostCanvas buddies(7 * 150, 5 * 130);
  buddies.fillScreen(0x3a4f);
  for (int a = 0; a < 5; ++a)
    for (int mood = 0; mood < 7; ++mood) {
      Look look;
      look.base = a;
      Pose pose;
      pose.mood = mood;
      pose.phase = 2;
      drawBuddy(buddies, look, 75 + mood * 150, 72 + a * 130, pose);
    }
  buddies.savePpm((out + "/buddies.ppm").c_str());

  HostCanvas home = sheetOf(3, 4);
  HomeView v = base(); // 1. Calm, nothing nearby.
  place(home, 0, 0, v);
  v = base(); // 2. Talking.
  v.speaking = true;
  v.message = "I'd knock a router off a shelf. For science.";
  v.mood = 1;
  place(home, 1, 0, v);
  v = base(); // 3. Hungry and pouting, with clues.
  v.look.base = 1;
  v.name = "Sprout";
  v.mood = 6;
  v.feeling = Feeling::Hungry;
  v.food = 22;
  v.love = 31;
  v.speaking = true;
  v.message = "My tummy is making dial-up noises.";
  v.clueCount = 3;
  v.clues[0] = {familyShort(Family::Tracker), true, true, -59, "MED"};
  v.clues[1] = {familyShort(Family::Raven), false, true, -49, "HIGH"};
  v.clues[2] = {familyShort(Family::Flipper), true, false, -80, "LOW"};
  place(home, 2, 0, v);
  v = base(); // 4. Recording a drive.
  v.look.base = 4;
  v.name = "Ember";
  v.recording = v.moving = v.fix = true;
  v.sats = 9;
  v.state = "REC";
  v.kit = 2;
  v.wifiSaved = 148;
  v.bleSaved = 37;
  v.feeling = Feeling::Hyped;
  v.backdrop = 1;
  v.level = 12;
  v.discoveryAge = 400;
  place(home, 0, 1, v);
  v.discoveryAge = UINT32_MAX; // 5. Same drive, quoting a network.
  v.speaking = true;
  v.message = "Spotted 'NETGEAR-Guest-5G..'. That's a new one.";
  place(home, 1, 1, v);
  v = base(); // 6. Alert.
  v.look.base = 2;
  v.name = "Orbit";
  v.mood = 4;
  v.alert = v.speaking = true;
  v.message = "A wild dolphin gadget appeared!";
  v.alertFamily = "Flipper Zero";
  v.alertConfidence = "high";
  v.backdrop = 2;
  place(home, 2, 1, v);
  v = base(); // 7. Napping.
  v.look.base = 3;
  v.name = "Wisp";
  v.mood = 2;
  v.feeling = Feeling::Napping;
  v.backdrop = 3;
  v.food = 48;
  place(home, 0, 2, v);
  v = base(Theme::Light); // 8. Light theme, level up.
  v.levelAge = 900;
  v.level = 5;
  v.xp = 0;
  v.speaking = true;
  v.message = "Ding! I feel smarter. And rounder.";
  v.mood = 3;
  place(home, 1, 2, v);
  v = base(Theme::Matrix); // 9. Matrix theme, no SD.
  v.state = "NO SD";
  v.stateUrgent = true;
  v.look.base = 1;
  v.name = "Sprout";
  place(home, 2, 2, v);
  v = base(); // 10. A long message: three tight rows, and the chin stays clear.
  v.speaking = true;
  v.message = "SD full/write failed. Drive stopped. Check the card, then tap Retry SD.";
  place(home, 0, 3, v);
  v = base(Theme::Light); // 11. Light theme alert while recording: white text on the deep red.
  v.mood = 4;
  v.alert = v.speaking = true;
  v.message = "Tracker nearby. Yours, I hope?";
  v.alertFamily = familyName(Family::SamsungTag);
  v.alertConfidence = "high";
  v.recording = v.fix = true;
  v.sats = 6;
  v.state = "REC";
  v.clueCount = 1;
  v.clues[0] = {familyShort(Family::SamsungTag), true, true, -52, "HIGH"};
  place(home, 1, 3, v);
  v = base(); // 12. A veteran: the XP caption must stay inside its panel.
  v.level = 1000;
  v.xp = 24;
  v.name = "Sixteen chars ok";
  place(home, 2, 3, v);
  home.savePpm((out + "/home.ppm").c_str());

  HostCanvas face = sheetOf(3, 4);
  for (int a = 0; a < 5; ++a) {
    static const char *names[] = {"Mochi", "Sprout", "Orbit", "Wisp", "Ember"};
    v = base(a == 4 ? Theme::Bwm : Theme::Dark);
    v.face = true;
    v.look.base = a;
    v.name = names[a];
    v.backdrop = a % 4;
    v.mood = a == 1 ? 3 : a == 3 ? 6 : a == 2 ? 1 : 0;
    if (a == 3) {
      v.feeling = Feeling::Lonely;
      v.love = 20;
      v.speaking = true;
      v.message = "Being invisible is my job, not my hobby.";
    }
    if (a == 4) {
      v.recording = v.fix = true;
      v.sats = 7;
      v.state = "REC";
      v.wifiSaved = 12;
    }
    place(face, a % 3, a / 3, v);
  }
  v = base();
  v.face = true;
  v.mood = 2;
  v.feeling = Feeling::Napping;
  place(face, 2, 1, v);
  // Custom buddies in Full Face: every hat must be visible, and speech must not hide the mouth.
  for (int hat = 1; hat <= 5; ++hat) {
    v = base();
    v.face = true;
    v.look.custom = true;
    v.look.base = hat % 5;
    v.look.body = hat == 2 ? 0x0000 : rgb565(0xbfe8ff);
    v.look.accent = rgb565(0xff8a5c);
    v.look.headwear = hat;
    v.look.eyewear = hat == 3 ? 1 : 0;
    v.look.neckwear = hat == 5 ? 2 : 0;
    v.name = "Custom";
    v.mood = hat == 4 ? 3 : hat == 5 ? 4 : 0;
    if (hat == 1 || hat == 4) {
      v.speaking = true;
      v.message = hat == 1 ? "Hehe. That's the good spot." : "You were gone 14 hours. I counted.";
    }
    if (hat == 5) {
      v.alert = v.speaking = true;
      v.message = "Possible body-camera signal nearby.";
      v.alertFamily = familyName(Family::Axon);
      v.alertConfidence = "medium";
    }
    place(face, (hat - 1) % 3, 2 + (hat - 1) / 3, v);
  }
  v = base(Theme::Light); // No hat: the 2.1x close-up, round glasses, Ink eyes on Orbit's screen.
  v.face = true;
  v.look.custom = true;
  v.look.base = 2;
  v.look.body = rgb565(0xd8c8ff);
  v.look.eyewear = 1;
  v.name = "Custom";
  v.alert = v.speaking = true;
  v.mood = 0;
  v.message = "Something odd nearby. Stay curious. This one wraps onto a second row.";
  v.alertFamily = familyName(Family::None);
  v.alertConfidence = "low";
  place(face, 2, 3, v);
  face.savePpm((out + "/face.ppm").c_str());

  // Sound & Voice page: each voice mode, the TEST VOICE read-back, and the light theme.
  {
    HostCanvas audio = sheetOf(2, 2);
    auto page = [&](int column, int row, Theme theme, const AudioView &state) {
      AudioView view = state;
      view.theme = theme;
      view.colors = palette(theme);
      HostCanvas screen(320, 240);
      screen.fillScreen(view.colors.bg);
      homeui::text(screen, "SOUND & VOICE", 12, 12, view.colors.accent); // As ui.cpp heading().
      audioui::button(screen, view, 254, 5, 58, 27, "BACK");
      drawAudio(screen, view);
      audio.blit(screen, 4 + column * 324, 4 + row * 244);
    };
    AudioView a;
    page(0, 0, Theme::Dark, a);
    a.voice = VoiceMode::Alerts;
    a.tested = true;
    a.said = clipCaption(Clip::SpicyPig);
    a.volume = 170;
    page(1, 0, Theme::Dark, a);
    a.voice = VoiceMode::All;
    a.tested = false;
    a.tone = "Chime";
    page(0, 1, Theme::Light, a);
    a.sound = false;
    a.tested = true;
    a.said = nullptr;
    page(1, 1, Theme::Bwm, a);
    audio.savePpm((out + "/audio.ppm").c_str());
  }

  // Documentation screenshots, one screen each (site/assets/ui-*.png).
  auto shot = [&](const char *file, const HomeView &view) {
    HostCanvas screen(320, 240);
    screen.fillScreen(view.colors.bg);
    drawHome(screen, view);
    screen.savePpm((out + "/ui-" + file + ".ppm").c_str());
  };
  auto clues = [](HomeView &view) {
    view.clueCount = 3;
    view.clues[0] = {familyShort(Family::Tracker), true, true, -59, "MED"};
    view.clues[1] = {familyShort(Family::Camera), false, true, -49, "HIGH"};
    view.clues[2] = {familyShort(Family::Flipper), true, false, -80, "LOW"};
  };
  v = base();
  clues(v);
  shot("recon", v);
  const Theme themes[] = {Theme::Dark, Theme::Light, Theme::Matrix, Theme::Bwm};
  const char *themeFiles[] = {"dark", "light", "matrix", "bwm"};
  for (int i = 0; i < 4; ++i) {
    v = base(themes[i]);
    clues(v);
    v.speaking = true;
    v.message = "I'm not staring. I'm monitoring.";
    v.mood = 1;
    shot(themeFiles[i], v);
  }
  for (int i = 0; i < 4; ++i) {
    v = base();
    v.backdrop = i;
    v.look.base = i == 3 ? 1 : i;
    v.name = i == 0 ? "Mochi" : i == 1 ? "Sprout" : i == 2 ? "Orbit" : "Sprout";
    shot((std::string("world-") + char('0' + i)).c_str(), v);
  }
  v = base();
  v.face = true;
  v.speaking = true;
  v.message = "Tiny copilot, reporting for vibes.";
  shot("face", v);
  v = base();
  v.look.base = 3;
  v.name = "Wisp";
  v.backdrop = 2;
  v.speaking = true;
  v.message = "Boo. Did I scare the Wi-Fi?";
  shot("wisp", v);
  v = base();
  v.look.base = 4;
  v.name = "Ember";
  v.backdrop = 1;
  v.speaking = true;
  v.message = "I'm lit. Literally.";
  shot("ember", v);
  v = base();
  v.mood = 3;
  v.love = 82;
  v.speaking = true;
  v.message = "Hehe. That's the good spot.";
  shot("love", v);
  v = base();
  v.mood = 4;
  v.alert = v.speaking = true;
  v.message = "A little tracker signal just popped up!";
  v.alertFamily = "Find My / tracker";
  v.alertConfidence = "medium";
  clues(v);
  shot("alert", v);
  v = base();
  v.mood = 4;
  v.alert = v.demo = v.speaking = true;
  v.message = "PIG DETECTED! ACT NATURAL!";
  v.alertFamily = "Axon / body camera";
  v.alertConfidence = "medium";
  shot("panic", v);
  HomeView kit = base();
  kit.look.custom = true;
  kit.look.base = 0;
  kit.look.body = rgb565(0xbfe8ff);
  kit.look.accent = rgb565(0xff8a5c);
  kit.look.eye = Ink;
  kit.look.eyes = 1;
  kit.look.headwear = 5;
  kit.look.neckwear = 1;
  kit.name = "Signal Scout";
  kit.kit = 1;
  shot("fieldkit", kit);
  v = kit;
  v.mood = 4;
  v.alert = v.speaking = true;
  v.message = "Smart glasses nearby. Act cool.";
  v.alertFamily = "Camera glasses";
  v.alertConfidence = "high";
  shot("fieldkit-alert", v);
  v = base();
  v.recording = v.fix = true;
  v.sats = 8;
  v.state = "REC";
  v.kit = 1;
  v.wifiSaved = 64;
  v.bleSaved = 21;
  v.speaking = true;
  v.message = "Logging! Show me the good streets.";
  shot("recording", v);
  v.speaking = false;
  v.moving = true;
  v.kit = 2;
  v.feeling = Feeling::Hyped;
  v.wifiSaved = 212;
  v.bleSaved = 58;
  v.level = 12;
  v.backdrop = 1;
  shot("drive", v);
}
