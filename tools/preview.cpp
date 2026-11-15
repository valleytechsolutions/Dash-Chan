// PC preview of the buddy art and home screen. Run through tools/preview.py.
#include "home_view.h"
#include "host_canvas.h"
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
int main(int argc, char **argv) {
  out = argc > 1 ? argv[1] : ".";
  HostCanvas buddies(7 * 150, 5 * 130);
  buddies.fillScreen(0x3a4f);
  for (int a = 0; a < 5; ++a)
    for (int mood = 0; mood < 7; ++mood)
      drawBuddy(buddies, a, 75 + mood * 150, 72 + a * 130, mood, 0, false, 0, 0, Blush, 0, false, 0,
                Ink, 0, 0, 0, 0, 2);
  buddies.savePpm((out + "/buddies.ppm").c_str());

  HostCanvas home = sheetOf(3, 3);
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
  v.clues[0] = {"Find My / tracker", true, true, -59, "MED"};
  v.clues[1] = {"Camera / Ring vendor", false, true, -49, "HIGH"};
  v.clues[2] = {"Flipper Zero", true, false, -80, "LOW"};
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
  home.savePpm((out + "/home.ppm").c_str());

  HostCanvas face = sheetOf(3, 2);
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
  face.savePpm((out + "/face.ppm").c_str());

  // Documentation screenshots, one screen each (site/assets/ui-*.png).
  auto shot = [&](const char *file, const HomeView &view) {
    HostCanvas screen(320, 240);
    screen.fillScreen(view.colors.bg);
    drawHome(screen, view);
    screen.savePpm((out + "/ui-" + file + ".ppm").c_str());
  };
  auto clues = [](HomeView &view) {
    view.clueCount = 3;
    view.clues[0] = {"Find My / tracker", true, true, -59, "MED"};
    view.clues[1] = {"Camera / Ring vendor", false, true, -49, "HIGH"};
    view.clues[2] = {"Flipper Zero", true, false, -80, "LOW"};
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
