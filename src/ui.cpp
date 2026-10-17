#include "app.h"
#include "avatars.h"
#include "boot_logos.h"
#include "travel.h"
#include <M5StackChan.h>
#include <algorithm>
namespace dash {
static M5Canvas canvas(&M5.Display), thumb;
static bool buffered = false, thumbReady = false;
static uint32_t painted = 0;
enum Page {
  Home,
  Menu,
  Appearance,
  Buddies,
  Studio,
  Audio,
  Behavior,
  Detection,
  History,
  Diagnostics
};
static Page page = Home;
static uint8_t historyPage = 0;
static bool detailOpen = false;
static Alert detail;
static Palette colors;
static const char *names[] = {"Mochi", "Sprout", "Orbit"};
static const char *tones[] = {"Personality", "Siren", "Arcade", "Chime", "Silent"};
static const char *petName() {
  return app.settings.avatar == Avatar::Custom ? app.settings.customName
                                               : names[unsigned(app.settings.avatar)];
}
static void text(const char *s, int x, int y, uint16_t color, int size = 1) {
  canvas.setTextSize(size);
  canvas.setTextColor(color);
  canvas.setCursor(x, y);
  canvas.print(s);
}
static void button(int x, int y, int w, int h, const char *label, bool active = false) {
  canvas.fillRoundRect(x, y, w, h, 9, active ? colors.accent : colors.panel);
  canvas.setTextDatum(middle_center);
  canvas.setTextSize(1);
  canvas.setTextColor(active ? (app.settings.theme == Theme::Light ? 0xffff : Ink) : colors.text);
  canvas.drawString(label, x + w / 2, y + h / 2);
  canvas.setTextDatum(top_left);
}
static void wrap(const char *s, int x, int y, int width, uint16_t color, int lines = 3) {
  std::string rest = s;
  for (int line = 0; line < lines && !rest.empty(); ++line) {
    size_t n = std::min(rest.size(), size_t(width));
    if (n < rest.size()) {
      size_t space = rest.rfind(' ', n);
      if (space != std::string::npos && space > 0)
        n = space;
    }
    text(rest.substr(0, n).c_str(), x, y + line * 12, color);
    rest.erase(0, n);
    while (!rest.empty() && rest.front() == ' ')
      rest.erase(0, 1);
  }
}
static void heading(const char *name) {
  text(name, 12, 12, colors.accent);
  button(254, 5, 58, 27, "BACK");
}
static void pet(M5Canvas &g, int avatar, int x, int y, int mood, int gaze, bool blink, int bob = 0,
                int field = 0, int phase = 0) {
  bool custom = avatar == 3;
  drawBuddy(g, custom ? app.settings.customBase : avatar, x, y, mood, gaze, blink, bob,
            app.settings.customBody, app.settings.customAccent, 0, custom,
            custom ? app.settings.customEyes : 0, app.settings.customEye,
            custom ? app.settings.customHeadwear : 0, custom ? app.settings.customEyewear : 0,
            custom ? app.settings.customNeckwear : 0, field, phase);
}
static void heart(int x, int y) {
  canvas.fillCircle(x - 3, y - 2, 4, colors.heart);
  canvas.fillCircle(x + 3, y - 2, 4, colors.heart);
  canvas.fillTriangle(x - 7, y, x + 7, y, x, y + 8, colors.heart);
}
static void home(uint32_t now) {
  char s[80];
  snprintf(s, sizeof(s), "%s  Lv.%lu", petName(), (unsigned long)(1 + app.settings.xp / 25));
  text(s, 12, 10, colors.accent);
  text(!app.radioReady      ? "RADIO ERROR"
       : !app.sdReady       ? "NO SD"
       : usableFix(app.fix) ? "GPS READY"
                            : "GPS WAIT",
       247, 10, colors.muted);
  if (app.alertOpen) {
    snprintf(s, sizeof(s), "%s %s | %s",
             app.demo ? "DEMO:" : "SIGNAL:", familyName(app.active.match.family),
             confidenceName(app.active.match.confidence));
    text(s, 12, 31, colors.danger);
  } else {
    snprintf(s, sizeof(s), "%s  %lu rows | %u recent",
             app.session.armed
                 ? (usableFix(app.fix) ? (app.moving ? "CRUISING + LOG" : "RECORDING") : "WAIT GPS")
                 : (app.moving ? "MOVING + WATCH" : "WATCHING"),
             (unsigned long)app.rows, app.historyCount);
    text(s, 12, 31, colors.muted);
  }
  if (app.settings.theme == Theme::Matrix)
    for (int x = 75; x < 250; x += 20)
      for (int y = 56; y < 151; y += 19)
        text(((x / 20 + y / 19 + now / 1200) % 2) ? "1" : "0", x, y, colors.grid);
  int scene = fieldScene(app.settings.fieldKit, app.session.armed, app.moving);
  canvas.setClipRect(70, 42, 182, 115);
  if (scene == 2 && !app.alertOpen) {
    for (int i = 0; i < 4; ++i) {
      int x = 78 + (int(now / 25) + i * 47) % 164;
      canvas.drawLine(x, 149, std::min(247, x + 9), 149, colors.grid);
    }
  }
  int shake = app.alertOpen ? int(sin(now / 65.0) * 4) : 0;
  int bob = app.alertOpen ? int(cos(now / 90.0) * 4)
                          : int(sin(now / (app.energy < 30 ? 1600.0 : 650.0)) * 2);
  pet(canvas, unsigned(app.settings.avatar), 160 + shake, 104, int(app.mood),
      app.alertOpen ? int(sin(now / 100.0) * 5) : int(round(app.gaze)),
      !app.alertOpen && (now % 4100) < 130, bob, scene, now / 150);
  if (app.mood == Mood::Love) {
    heart(91, 88 - int(now / 100 % 18));
    heart(229, 90 - int((now / 100 + 9) % 18));
  }
  if (app.alertOpen) {
    text("!", 83, 59, colors.danger, 2);
    text("!", 230, 70, colors.danger, 2);
  }
  canvas.clearClipRect();
  text("FUEL", 12, 71, colors.muted);
  canvas.fillRoundRect(12, 85, 44, 5, 2, colors.panel);
  canvas.fillRoundRect(12, 85, std::max(2, int(app.energy) * 44 / 100), 5, 2, colors.accent);
  snprintf(s, sizeof(s), "%u%%", app.energy);
  text(s, 12, 99, colors.text);
  snprintf(s, sizeof(s), "%u/5", app.food.progress());
  text(s, 12, 115, colors.accent);
  text("snack", 12, 127, colors.muted);
  text("BOND", 270, 71, colors.muted);
  canvas.fillRoundRect(266, 85, 42, 5, 2, colors.panel);
  canvas.fillRoundRect(266, 85, std::max(2, int(app.bond) * 42 / 100), 5, 2, colors.heart);
  text("pet me", 266, 102, colors.muted);
  canvas.fillRoundRect(8, 159, 304, 34, 9, colors.panel);
  wrap(app.message, 17, 166, 47, app.alertOpen ? colors.danger : colors.text, 2);
  button(8, 201, 222, 32, app.session.armed ? "STOP & SAVE" : "START WARDRIVE", true);
  button(238, 201, 74, 32, "MENU");
}
static void menu() {
  heading("YOUR LITTLE COPILOT");
  button(8, 45, 148, 42, "LOOK & THEMES");
  button(164, 45, 148, 42, "SOUND & VOICE");
  button(8, 97, 148, 42, "PERSONALITY");
  button(164, 97, 148, 42, "DETECTION");
  button(8, 149, 148, 42, "RECENT SIGNALS");
  button(164, 149, 148, 42, "GPS & SD");
  text("Watching continues on every screen.", 12, 212, colors.muted);
}
static void appearance() {
  heading("LOOK & THEMES");
  button(8, 44, 304, 38, (std::string("Choose buddy: ") + petName()).c_str());
  for (int i = 0; i < 4; ++i)
    button(8 + (i % 2) * 156, 96 + (i / 2) * 45, 148, 36, themeName(Theme(i)),
           unsigned(app.settings.theme) == unsigned(i));
  button(8, 188, 304, 38, "AVATAR STUDIO: EYES & GEAR");
}
static void studio() {
  heading("AVATAR STUDIO");
  const char *eyes[] = {"Classic", "Sparkle", "Sleepy", "Cat", "Pixel", "Wink"};
  const char *hats[] = {"None", "Bow", "Crown", "Beanie", "Flower", "Scout cap"};
  const char *glasses[] = {"None", "Round", "Shades", "Visor"};
  const char *necks[] = {"None", "Scarf", "Bandana", "Bow tie"};
  const char *kits[] = {"Auto", "Always", "Off"};
  char s[48];
  snprintf(s, sizeof(s), "Eyes: %s", eyes[app.settings.customEyes]);
  button(8, 46, 148, 38, s);
  snprintf(s, sizeof(s), "Head: %s", hats[app.settings.customHeadwear]);
  button(164, 46, 148, 38, s);
  snprintf(s, sizeof(s), "Glasses: %s", glasses[app.settings.customEyewear]);
  button(8, 94, 148, 38, s);
  snprintf(s, sizeof(s), "Neck: %s", necks[app.settings.customNeckwear]);
  button(164, 94, 148, 38, s);
  snprintf(s, sizeof(s), "Shape: %s", names[app.settings.customBase]);
  button(8, 142, 148, 38, s);
  snprintf(s, sizeof(s), "Field kit: %s", kits[app.settings.fieldKit]);
  button(164, 142, 148, 38, s);
  button(8, 192, 148, 34, "SHOW MY BUDDY", true);
  text("Colors + name in", 172, 195, colors.muted);
  text("the web studio", 172, 210, colors.muted);
}
static void buddies(uint32_t now) {
  heading("CHOOSE YOUR BUDDY");
  for (int a = 0; a < 3; ++a) {
    int x = 54 + 106 * a;
    bool selected = unsigned(app.settings.avatar) == unsigned(a);
    canvas.fillRoundRect(x - 49, 47, 98, 121, 10, selected ? colors.panel : colors.bg);
    if (thumbReady) {
      thumb.fillSprite(selected ? colors.panel : colors.bg);
      pet(thumb, a, 80, 61, 0, 0, now % 4000 < 100);
      thumb.pushRotateZoom(&canvas, x, 107, 0, 0.58, 0.65);
    }
    text(names[a], x - 18, 149, selected ? colors.accent : colors.text);
  }
  button(8, 184, 304, 40, (std::string("Custom: ") + app.settings.customName).c_str(),
         app.settings.avatar == Avatar::Custom);
}
static void audio() {
  heading("SOUND & VOICE");
  char s[64];
  snprintf(s, sizeof(s), "Alert tone: %s  >", tones[unsigned(app.settings.tone)]);
  button(8, 44, 304, 36, s);
  snprintf(s, sizeof(s), "Volume: %u / 255", app.settings.volume);
  text(s, 12, 92, colors.muted);
  constexpr uint8_t levels[] = {0, 100, 170, 220, 255};
  for (int i = 0; i < 5; ++i) {
    snprintf(s, sizeof(s), "%u", levels[i]);
    button(8 + i * 62, 109, 56, 30, s, app.settings.volume == levels[i]);
  }
  button(8, 151, 148, 34, app.settings.sound ? "Master sound: ON" : "Master sound: OFF");
  button(164, 151, 148, 34, app.settings.voice ? "Voice: ON" : "Voice: OFF");
  button(8, 198, 148, 34, "PREVIEW TONE");
  button(164, 198, 148, 34, "AXON DEMO", true);
}
static void behavior() {
  heading("PERSONALITY");
  button(8, 44, 304, 38,
         !app.settings.motion            ? "Head motion: OFF"
         : app.settings.motionStyle == 1 ? "Head motion: Gentle"
                                         : "Head motion: Lively");
  button(8, 94, 304, 38, app.settings.spicy ? "Banter: Spicy panic" : "Banter: Gentle lookout");
  button(8, 144, 304, 38, "PET YOUR BUDDY", true);
  wrap("Stroke the top sensor or tap the face for love. Only GPS-tagged wardriving feeds me!", 12,
       199, 47, colors.muted);
}
static void detection() {
  heading("DETECTION");
  char s[70];
  snprintf(s, sizeof(s), "Notify: %s confidence or higher", confidenceName(app.settings.threshold));
  button(8, 44, 304, 36, s);
  snprintf(s, sizeof(s), "Same-family quiet: %lu seconds",
           (unsigned long)(app.settings.familyQuietMs / 1000));
  button(8, 90, 304, 36, s);
  button(8, 136, 304, 36,
         app.settings.repeatAlerts ? "Reminders: every 5 min" : "Reminders: OFF (new encounters)");
  wrap("Same device stays quiet until absent 5 min. Stronger evidence may alert again. Signals are "
       "clues, not proof of identity.",
       12, 190, 47, colors.muted);
}
static void history() {
  heading("RECENT SIGNALS");
  char s[90];
  if (detailOpen) {
    for (unsigned i = 0; i < app.historyCount; ++i) {
      const auto &a = app.history[i];
      if (a.ble == detail.ble && a.addressType == detail.addressType &&
          a.match.family == detail.match.family && !strcmp(a.mac, detail.mac)) {
        detail = a;
        break;
      }
    }
    text(familyName(detail.match.family), 12, 46, colors.text);
    snprintf(s, sizeof(s), "%s | %s | %d dBm", confidenceName(detail.match.confidence),
             detail.ble ? "BLE" : "WiFi", detail.rssi);
    text(s, 12, 65, colors.accent);
    wrap(*detail.name ? detail.name : "(no advertised name)", 12, 86, 47, colors.text, 1);
    text(detail.mac, 12, 105, colors.muted);
    wrap(detail.match.evidence, 12, 126, 47, colors.muted);
    snprintf(s, sizeof(s), "Seen %lu times | last %lus ago", (unsigned long)detail.sightings,
             (unsigned long)((millis() - detail.at) / 1000));
    text(s, 12, 170, colors.accent);
    button(8, 198, 304, 34, "BACK TO RECENT");
    return;
  }
  if (!app.historyCount)
    wrap("Nothing matched your alert filter yet. Your buddy is still watching Wi-Fi and BLE.", 12,
         70, 45, colors.muted);
  unsigned pages = std::max(1u, (app.historyCount + 3u) / 4);
  historyPage = std::min(unsigned(historyPage), pages - 1);
  for (unsigned i = 0; i < 4 && historyPage * 4 + i < app.historyCount; ++i) {
    auto &a = app.history[historyPage * 4 + i];
    snprintf(s, sizeof(s), "%s | %d dBm", familyName(a.match.family), a.rssi);
    button(8, 43 + i * 36, 304, 31, s);
  }
  button(8, 199, 84, 33, "PREV");
  button(228, 199, 84, 33, "NEXT");
  snprintf(s, sizeof(s), "%u / %u", historyPage + 1, pages);
  text(s, 144, 212, colors.muted);
}
static void diagnostics() {
  heading("GPS & SD");
  char s[96];
  snprintf(s, sizeof(s), "GPS %s | %u sats | HDOP %.1f", usableFix(app.fix) ? "READY" : "WAIT",
           app.fix.sats, app.fix.hdop);
  text(s, 10, 44, colors.text);
  snprintf(s, sizeof(s), "NMEA %lu chars | fix age %lu ms", (unsigned long)app.gpsChars,
           (unsigned long)app.fix.age);
  text(s, 10, 62, colors.muted);
  snprintf(s, sizeof(s), "SD %s | %lu rows | %lu drops", app.sdReady ? "READY" : "MISSING",
           (unsigned long)app.rows, (unsigned long)app.drops);
  text(s, 10, 82, colors.text);
  wrap(*app.storageError   ? app.storageError
       : app.session.armed ? app.logPath
                           : "Ready: start a wardrive from the face.",
       10, 104, 49, colors.muted);
  snprintf(s, sizeof(s), "GPS: %s", app.settings.gpsPort ? "PORT A" : "PORT C");
  button(8, 151, 148, 34, s);
  snprintf(s, sizeof(s), "Baud: %lu", (unsigned long)app.settings.gpsBaud);
  button(164, 151, 148, 34, s);
  button(8, 199, 148, 33, "RETRY SD");
  button(164, 199, 148, 33, "HOME", true);
}
static bool row(int y, int top, int height = 36) {
  return y >= top && y < top + height;
}
static void touch(int x, int y) {
  if (page == Home) {
    if (row(y, 201, 32)) {
      if (x < 230) {
        if (app.session.armed)
          stopDrive();
        else
          startDrive();
      } else if (x >= 238)
        page = Menu;
    } else if (row(y, 159, 34) && app.alertOpen)
      buddyDismiss();
    else if (y >= 48 && y < 155 && x >= 66 && x < 258) {
      if (app.alertOpen)
        buddyDismiss();
      buddyPet();
    }
    return;
  }
  if (y < 34 && x >= 250) {
    page = page == Menu ? Home : (page == Buddies || page == Studio) ? Appearance : Menu;
    detailOpen = false;
    return;
  }
  bool right = x >= 160;
  switch (page) {
  case Menu:
    if (row(y, 45, 42))
      page = right ? Audio : Appearance;
    else if (row(y, 97, 42))
      page = right ? Detection : Behavior;
    else if (row(y, 149, 42)) {
      page = right ? Diagnostics : History;
      detailOpen = false;
      historyPage = 0;
    }
    break;
  case Appearance:
    if (row(y, 44, 38))
      page = Buddies;
    else if (row(y, 96))
      app.settings.theme = Theme(right ? 1 : 0);
    else if (row(y, 141))
      app.settings.theme = Theme(right ? 3 : 2);
    else if (row(y, 188, 38))
      page = Studio;
    break;
  case Studio:
    if (row(y, 46, 38)) {
      if (right)
        app.settings.customHeadwear = (app.settings.customHeadwear + 1) % 6;
      else
        app.settings.customEyes = (app.settings.customEyes + 1) % 6;
      app.settings.avatar = Avatar::Custom;
    } else if (row(y, 94, 38)) {
      if (right)
        app.settings.customNeckwear = (app.settings.customNeckwear + 1) % 4;
      else
        app.settings.customEyewear = (app.settings.customEyewear + 1) % 4;
      app.settings.avatar = Avatar::Custom;
    } else if (row(y, 142, 38)) {
      if (right)
        app.settings.fieldKit = (app.settings.fieldKit + 1) % 3;
      else {
        app.settings.customBase = (app.settings.customBase + 1) % 3;
        app.settings.avatar = Avatar::Custom;
      }
    } else if (row(y, 192, 34) && !right) {
      app.settings.avatar = Avatar::Custom;
      page = Home;
    }
    break;
  case Buddies:
    if (y >= 47 && y < 168)
      app.settings.avatar = Avatar(std::min(2, x / 106));
    else if (row(y, 184, 40))
      app.settings.avatar = Avatar::Custom;
    break;
  case Audio:
    if (row(y, 44))
      app.settings.tone = AlertTone((unsigned(app.settings.tone) + 1) % 5);
    else if (row(y, 109, 30)) {
      constexpr uint8_t levels[] = {0, 100, 170, 220, 255};
      app.settings.volume = levels[std::min(4, std::max(0, (x - 8) / 62))];
    } else if (row(y, 151, 34)) {
      if (right) {
        app.settings.voice = !app.settings.voice;
        if (!app.settings.voice)
          M5.Speaker.stop();
      } else {
        app.settings.sound = !app.settings.sound;
        if (!app.settings.sound)
          M5.Speaker.stop();
      }
    } else if (row(y, 198, 34)) {
      if (right) {
        buddyDemo();
        page = Home;
      } else
        buddySoundPreview();
    }
    break;
  case Behavior:
    if (row(y, 44, 38)) {
      if (!app.settings.motion) {
        app.settings.motionStyle = 1;
        buddyMotion(true);
      } else if (app.settings.motionStyle == 1)
        app.settings.motionStyle = 2;
      else
        buddyMotion(false);
    } else if (row(y, 94, 38))
      app.settings.spicy = !app.settings.spicy;
    else if (row(y, 144, 38)) {
      buddyPet();
      page = Home;
    }
    break;
  case Detection:
    if (row(y, 44))
      app.settings.threshold = Confidence(unsigned(app.settings.threshold) % 3 + 1);
    else if (row(y, 90))
      app.settings.familyQuietMs = app.settings.familyQuietMs == 60000    ? 180000
                                   : app.settings.familyQuietMs == 180000 ? 300000
                                                                          : 60000;
    else if (row(y, 136))
      app.settings.repeatAlerts = !app.settings.repeatAlerts;
    break;
  case History:
    if (detailOpen) {
      if (row(y, 198, 34))
        detailOpen = false;
    } else if (row(y, 199, 33)) {
      unsigned pages = std::max(1u, (app.historyCount + 3u) / 4);
      if (x < 100)
        historyPage = (historyPage + pages - 1) % pages;
      else if (x > 220)
        historyPage = (historyPage + 1) % pages;
    } else if (y >= 43 && y < 187) {
      unsigned i = historyPage * 4 + (y - 43) / 36;
      if (i < app.historyCount) {
        detail = app.history[i];
        detailOpen = true;
      }
    }
    break;
  case Diagnostics:
    if (row(y, 151, 34) && !app.session.armed) {
      if (!right)
        app.settings.gpsPort ^= 1;
      else
        app.settings.gpsBaud = app.settings.gpsBaud == 9600     ? 115200
                               : app.settings.gpsBaud == 115200 ? 38400
                                                                : 9600;
      gpsBegin();
    } else if (row(y, 199, 33)) {
      if (right)
        page = Home;
      else if (!app.session.armed) {
        storageBegin();
        storageLoadAvatar();
      }
    }
    break;
  default:
    break;
  }
  saveSettings();
}
void uiBegin() {
  canvas.setColorDepth(16);
  buffered = canvas.createSprite(320, 240) != nullptr;
  thumb.setColorDepth(16);
  thumbReady = thumb.createSprite(160, 120) != nullptr;
  if (!buffered) {
    M5.Display.println("Display buffer allocation failed");
    Serial.println("UI allocation failed");
  }
}
void uiBoot() {
  if (!buffered)
    return;
  canvas.fillSprite(0xffff);
  canvas.drawPng(bootBwm, sizeof(bootBwm), 21, 34);
  canvas.drawPng(bootVts, sizeof(bootVts), 178, 34);
  text("DASH-CHAN", 106, 187, Ink, 2);
  text("Your tiny road buddy is waking up", 64, 216, Ink);
  canvas.pushSprite(0, 0);
  delay(1500);
}
void uiTick() {
  if (!buffered)
    return;
  auto t = M5.Touch.getDetail();
  if (t.wasPressed())
    touch(t.x, t.y);
  uint32_t now = millis();
  if (uint32_t(now - painted) < 50)
    return;
  painted = now;
  colors = palette(app.settings.theme);
  canvas.fillSprite(colors.bg);
  switch (page) {
  case Home:
    home(now);
    break;
  case Menu:
    menu();
    break;
  case Appearance:
    appearance();
    break;
  case Studio:
    studio();
    break;
  case Buddies:
    buddies(now);
    break;
  case Audio:
    audio();
    break;
  case Behavior:
    behavior();
    break;
  case Detection:
    detection();
    break;
  case History:
    history();
    break;
  case Diagnostics:
    diagnostics();
    break;
  }
  canvas.pushSprite(0, 0);
}
void uiTap(int x, int y) {
  if (x >= 0 && x < 320 && y >= 0 && y < 240)
    touch(x, y);
}
void uiScreenshot() {
  if (!buffered)
    return;
  Serial.println("FRAME 320 240 RGB888");
  uint8_t row[320 * 3];
  for (int y = 0; y < 240; ++y) {
    for (int x = 0; x < 320; ++x) {
      uint16_t c = canvas.readPixel(x, y);
      row[x * 3] = ((c >> 11) * 255 + 15) / 31;
      row[x * 3 + 1] = (((c >> 5) & 63) * 255 + 31) / 63;
      row[x * 3 + 2] = ((c & 31) * 255 + 15) / 31;
    }
    size_t sent = 0;
    uint32_t started = millis();
    while (sent < sizeof(row)) {
      sent += Serial.write(row + sent, sizeof(row) - sent);
      if (uint32_t(millis() - started) > 2000)
        return;
      delay(1);
    }
  }
  Serial.println("\nFRAME_END");
}
} // namespace dash
