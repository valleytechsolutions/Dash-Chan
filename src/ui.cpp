#include "app.h"
#include "avatars.h"
#include <M5StackChan.h>
#include <algorithm>
namespace dash {
static M5Canvas canvas(&M5.Display), thumb;
static bool buffered = false, thumbReady = false;
static uint32_t painted = 0;
static uint8_t page = 0;
static int selectedAlert = -1;
constexpr uint16_t Bg = 0x0864, Panel = 0x1928, Muted = 0x94b5, Accent = 0x87b6, White = 0xf7be,
                   Orange = 0xfd2d;
static const char *names[] = {"Mochi", "Sprout", "Orbit"};
static void text(const char *s, int x, int y, uint16_t color = White, int size = 1) {
  canvas.setTextColor(color);
  canvas.setTextSize(size);
  canvas.setCursor(x, y);
  canvas.print(s);
}
static void button(int x, int y, int w, int h, const char *label, uint16_t color = Panel) {
  canvas.fillRoundRect(x, y, w, h, 9, color);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(color == Accent ? Ink : White);
  canvas.setTextSize(1);
  canvas.drawString(label, x + w / 2, y + h / 2);
  canvas.setTextDatum(top_left);
}
static void wrap(const char *s, int x, int y, int width, uint16_t color = White) {
  std::string rest = s;
  for (int line = 0; line < 3 && !rest.empty(); ++line) {
    size_t n = std::min(rest.size(), size_t(width));
    if (n < rest.size()) {
      size_t space = rest.rfind(' ', n);
      if (space != std::string::npos && space > 0)
        n = space;
    }
    std::string part = rest.substr(0, n);
    text(part.c_str(), x, y + line * 12, color);
    rest.erase(0, n);
    while (!rest.empty() && rest.front() == ' ')
      rest.erase(0, 1);
  }
}
static void home(uint32_t now) {
  text("DASH-CHAN", 10, 8, Accent);
  char status[80];
  snprintf(status, sizeof(status), "%s  %s",
           app.radioReady ? (app.blePhase ? "BLE" : "WiFi") : "RADIO ERROR",
           usableFix(app.fix) ? "GPS FIX" : "NO FIX");
  text(status, 176, 8, Muted);
  snprintf(status, sizeof(status), "%s  Lv.%lu   %lu WiFi / %lu BLE",
           names[unsigned(app.settings.avatar)], (unsigned long)(1 + app.settings.xp / 25),
           (unsigned long)app.wifiSeen, (unsigned long)app.bleSeen);
  text(status, 10, 26, Muted);
  int bob = int(sin(now / 500.0) * 2);
  drawBuddy(canvas, unsigned(app.settings.avatar), 160, 95, int(app.mood),
            int(sin(now / 1800.0) * 3), (now % 4100) < 130, bob);
  text("BOND", 12, 62, Muted);
  canvas.fillRoundRect(12, 76, 48, 5, 2, Panel);
  canvas.fillRoundRect(12, 76, std::max(2, int(app.bond) * 48 / 100), 5, 2, Blush);
  text("FUEL", 263, 62, Muted);
  canvas.fillRoundRect(263, 76, 45, 5, 2, Panel);
  canvas.fillRoundRect(263, 76, std::max(2, int(app.energy) * 45 / 100), 5, 2, Accent);
  snprintf(status, sizeof(status), "%u/5 food", app.food.progress());
  text(status, 258, 89, Muted);
  if (app.session.armed) {
    snprintf(status, sizeof(status), "%s\n%lu rows", usableFix(app.fix) ? "REC" : "WAIT GPS",
             (unsigned long)app.rows);
    text(status, 12, 101, usableFix(app.fix) ? Accent : Orange);
  } else
    text("WATCH\nalways on", 12, 101, Accent);
  text("pet me", 266, 111, Muted);
  canvas.fillRoundRect(8, 151, 304, 38, 10, Panel);
  wrap(app.message, 17, 158, 47);
  button(8, 198, 177, 34, app.session.armed ? "STOP & SAVE" : "START WARDRIVE",
         app.session.armed ? 0xba49 : Accent);
  button(192, 198, 57, 34, "BUDDY");
  button(256, 198, 56, 34, "MENU");
  if (app.alertOpen) {
    canvas.fillRoundRect(5, 40, 310, 108, 12, app.demo ? 0x62b0 : 0x6247);
    text(app.demo ? "DEMO ALERT  /  tap to dismiss" : "SIGNAL ALERT  /  tap to dismiss", 14, 48,
         Orange);
    if (thumbReady) {
      thumb.fillSprite(app.demo ? 0x62b0 : 0x6247);
      drawBuddy(thumb, unsigned(app.settings.avatar), 80, 61, 4, int(sin(now / 100.0) * 5), false);
      thumb.pushRotateZoom(&canvas, 56, 101, 0, 0.58, 0.58);
    }
    text(familyName(app.active.match.family), 103, 65, White);
    char detail[80];
    snprintf(detail, sizeof(detail), "%s | %d dBm", confidenceName(app.active.match.confidence),
             app.active.rssi);
    text(detail, 103, 82, Accent);
    wrap(app.active.match.evidence, 103, 97, 32, White);
    text("Candidate signal; identity not verified", 14, 135, Muted);
  }
}
static void buddies(uint32_t now) {
  text("CHOOSE YOUR COPILOT", 12, 12, Accent);
  for (int a = 0; a < 3; ++a) {
    int x = 53 + 107 * a;
    if (unsigned(app.settings.avatar) == unsigned(a))
      canvas.fillRoundRect(x - 49, 38, 98, 137, 12, Panel);
    // Scale the same full-size art into three thumbnails.
    if (thumbReady) {
      thumb.fillSprite(unsigned(app.settings.avatar) == unsigned(a) ? Panel : Bg);
      drawBuddy(thumb, a, 80, 61, 0, 0, now % 4000 < 100);
      thumb.pushRotateZoom(&canvas, x, 100, 0, 0.58, 0.72);
    }
    text(names[a], x - 18, 154, White);
  }
  button(8, 194, 96, 38, app.session.armed ? "KEEP DRIVING" : "FIND FOOD", Accent);
  button(112, 194, 96, 38, "PET");
  button(216, 194, 96, 38, "HOME");
}
static void menu() {
  text("COPILOT SETTINGS", 12, 9, Accent);
  button(252, 4, 60, 24, "HOME");
  char s[64];
  snprintf(s, sizeof(s), "Sound: %s", app.settings.sound ? "ON" : "QUIET");
  button(8, 39, 148, 32, s);
  snprintf(s, sizeof(s), "Motion: %s", app.settings.motion ? "ON" : "OFF");
  button(164, 39, 148, 32, s);
  snprintf(s, sizeof(s), "Banter: %s", app.settings.spicy ? "SPICY" : "GENTLE");
  button(8, 78, 148, 32, s);
  snprintf(s, sizeof(s), "Alerts: %s+", confidenceName(app.settings.threshold));
  button(164, 78, 148, 32, s);
  button(8, 117, 148, 32, "DETECTION HISTORY");
  button(164, 117, 148, 32, "GPS / SD STATUS");
  button(8, 156, 148, 32, "TRY AXON REACTION");
  button(164, 156, 148, 32, "CHOOSE BUDDY");
  wrap("Motion uses gentle, limited head turns. Low confidence includes broad vendor clues.", 12,
       204, 48, Muted);
}
static void diagnostics() {
  text("GPS / RECORDING", 12, 9, Accent);
  button(252, 4, 60, 24, "BACK");
  char s[96];
  snprintf(s, sizeof(s), "GPS %s / %u satellites / HDOP %.1f", usableFix(app.fix) ? "FIX" : "WAIT",
           app.fix.sats, app.fix.hdop);
  text(s, 10, 39);
  snprintf(s, sizeof(s), "NMEA chars %lu / fix age %lu ms", (unsigned long)app.gpsChars,
           (unsigned long)app.fix.age);
  text(s, 10, 55, Muted);
  snprintf(s, sizeof(s), "SD %s | %lu rows | %lu no-fix skips", app.sdReady ? "READY" : "MISSING",
           (unsigned long)app.rows, (unsigned long)app.skippedFix);
  text(s, 10, 73);
  snprintf(s, sizeof(s), "Radio: %s | drops %lu | malformed %lu", app.radioReady ? "OK" : "ERROR",
           (unsigned long)app.drops, (unsigned long)app.malformed);
  text(s, 10, 91, Muted);
  wrap(*app.storageError ? app.storageError : app.logPath, 10, 109, 49, Orange);
  snprintf(s, sizeof(s), "GPS: %s", app.settings.gpsPort ? "PORT A" : "PORT C");
  button(8, 150, 148, 32, s);
  snprintf(s, sizeof(s), "Baud: %lu", (unsigned long)app.settings.gpsBaud);
  button(164, 150, 148, 32, s);
  button(8, 194, 148, 36, "RETRY SD");
  button(164, 194, 148, 36, "HOME");
}
static void history() {
  text("RECENT DETECTIONS", 12, 9, Accent);
  button(252, 4, 60, 24, "BACK");
  if (selectedAlert >= 0 && selectedAlert < app.historyCount) {
    auto &a = app.history[selectedAlert];
    text(familyName(a.match.family), 12, 45, White, 2);
    wrap(a.name, 12, 73, 47);
    text(a.mac, 12, 100, Muted);
    wrap(a.match.evidence, 12, 121, 47, Accent);
    button(12, 193, 296, 37, "BACK TO HISTORY");
    return;
  }
  if (!app.historyCount)
    wrap("No alert events yet. Background Wi-Fi and BLE listening continues on every screen.", 12,
         64, 45, Muted);
  for (int i = 0; i < std::min(5, int(app.historyCount)); ++i) {
    char s[80];
    auto &a = app.history[i];
    snprintf(s, sizeof(s), "%s  %d dBm", familyName(a.match.family), a.rssi);
    button(8, 40 + i * 38, 304, 32, s);
  }
}
static void touch(int x, int y) {
  if (page == 0) {
    if (y >= 198) {
      if (x < 186) {
        if (app.session.armed)
          stopDrive();
        else
          startDrive();
      } else
        page = x < 250 ? 1 : 2;
    } else if (app.alertOpen && y >= 40 && y < 150)
      buddyDismiss();
    else if (y < 32) {
      page = 4;
      selectedAlert = -1;
    } else if (y >= 40 && y < 150)
      buddyPet();
  } else if (page == 1) {
    if (y < 180 && y > 35) {
      app.settings.avatar = Avatar(std::min(2, x / 107));
      saveSettings();
    } else if (y >= 194) {
      if (x < 105) {
        if (!app.session.armed)
          startDrive();
        page = 0;
      } else if (x < 210)
        buddyPet();
      else
        page = 0;
    }
  } else if (page == 2) {
    if (y < 32 && x > 250) {
      page = 0;
      return;
    }
    bool right = x >= 160;
    if (y >= 39 && y < 71) {
      if (right)
        buddyMotion(!app.settings.motion);
      else {
        app.settings.sound = !app.settings.sound;
        if (!app.settings.sound)
          M5.Speaker.stop();
      }
    } else if (y >= 78 && y < 110) {
      if (right)
        app.settings.threshold = Confidence(unsigned(app.settings.threshold) % 3 + 1);
      else
        app.settings.spicy = !app.settings.spicy;
    } else if (y >= 117 && y < 149) {
      page = right ? 3 : 4;
      selectedAlert = -1;
    } else if (y >= 156 && y < 188) {
      if (right)
        page = 1;
      else {
        Alert a;
        a.match = {Family::Axon, Confidence::Medium, "Synthetic example; no device detected"};
        a.rssi = -45;
        buddyAlert(a, true);
        page = 0;
      }
    }
    saveSettings();
  } else if (page == 3) {
    if (y < 32 && x > 250)
      page = 2;
    else if (y >= 150 && y < 182 && !app.session.armed) {
      if (x < 160)
        app.settings.gpsPort ^= 1;
      else
        app.settings.gpsBaud = app.settings.gpsBaud == 9600     ? 115200
                               : app.settings.gpsBaud == 115200 ? 38400
                                                                : 9600;
      saveSettings();
      gpsBegin();
    } else if (y >= 194) {
      if (x < 160)
        storageBegin();
      else
        page = 0;
    }
  } else if (page == 4) {
    if (y < 32 && x > 250)
      page = 2;
    else if (selectedAlert >= 0 && y >= 193)
      selectedAlert = -1;
    else if (y >= 40 && y < 230 && selectedAlert < 0) {
      int i = (y - 40) / 38;
      if (i < app.historyCount)
        selectedAlert = i;
    }
  }
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
  canvas.fillSprite(Bg);
  if (page == 0)
    home(now);
  else if (page == 1)
    buddies(now);
  else if (page == 2)
    menu();
  else if (page == 3)
    diagnostics();
  else
    history();
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
