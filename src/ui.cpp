#include "app.h"
#include "avatars.h"
#include "boot_logos.h"
#include "home_view.h"
#include "travel.h"
#include "uploads.h"
#include <M5StackChan.h>
#include <algorithm>
namespace dash {
static M5Canvas canvas(&M5.Display);
static bool buffered = false;
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
  Diagnostics,
  GpsSetup,
  GpsModels,
  GpsWiring,
  DisplaySetup,
  Reactions,
  NfcCards,
  Uploads
};
static Page page = Home;
static uint8_t historyPage = 0;
static bool detailOpen = false;
static bool fromHome = false; // The page was opened by a Home shortcut, so BACK returns Home.
static bool confirmUpload = false;
static Alert detail;
static Palette colors;
static const char *names[] = {"Mochi", "Sprout", "Orbit", "Wisp", "Ember"};
static unsigned buddyPage = 0, gpsModelPage = 0;
static GpsConfig gpsDraft;
static bool gpsWasSearching = false;
static const char *tones[] = {"Personality", "Siren", "Arcade", "Chime", "Silent"};
static const char *petName() {
  return app.settings.avatar == Avatar::Custom ? app.settings.customName
                                               : names[avatarBase(app.settings.avatar, 0)];
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
static void mark(int x, int y, int scale = 1) {
  homeui::mark(canvas, colors, x, y, scale);
}
static BuddyLook buddyLook() {
  BuddyLook look;
  look.base = int(avatarBase(app.settings.avatar, app.settings.customBase));
  look.custom = app.settings.avatar == Avatar::Custom;
  look.body = app.settings.customBody;
  look.accent = app.settings.customAccent;
  look.eye = app.settings.customEye;
  look.eyes = app.settings.customEyes;
  look.headwear = app.settings.customHeadwear;
  look.eyewear = app.settings.customEyewear;
  look.neckwear = app.settings.customNeckwear;
  return look;
}
// Snapshot the app state for the shared home-screen renderer (include/home_view.h).
static void home(uint32_t now) {
  HomeView v;
  v.now = now;
  v.colors = colors;
  v.theme = app.settings.theme;
  v.backdrop = app.settings.backdrop;
  v.face = app.settings.view == 1;
  v.look = buddyLook();
  v.name = petName();
  v.mood = int(app.mood);
  v.gaze = int(round(app.gaze + app.tiltX));
  v.tiltY = int(app.tiltY);
  v.kit = fieldScene(app.settings.fieldKit, app.session.armed, app.moving);
  v.feeling = app.feeling;
  v.food = app.energy;
  v.love = app.bond;
  v.xp = levelProgress(app.settings.xp);
  v.level = petLevel(app.settings.xp);
  v.message = app.message;
  v.speaking = app.alertOpen || uint32_t(now - app.messageAt) < 6500;
  v.discoveryAge = app.discoveryAt ? uint32_t(now - app.discoveryAt) : UINT32_MAX;
  v.levelAge = app.levelUpAt ? uint32_t(now - app.levelUpAt) : UINT32_MAX;
  v.alert = app.alertOpen;
  v.demo = app.demo;
  v.alertFamily = familyName(app.active.match.family);
  v.alertConfidence = confidenceName(app.active.match.confidence);
  v.fix = usableFix(app.fix);
  v.sats = app.fix.sats;
  v.recording = app.session.armed;
  v.moving = app.moving;
  v.state = uploadBusy()        ? "UPLOAD"
            : !app.radioReady   ? "RADIO!"
            : !app.sdReady      ? "NO SD"
            : app.session.armed ? (v.fix ? "REC" : "NO GPS")
                                : "WATCHING";
  v.stateUrgent = !app.radioReady || !app.sdReady || (app.session.armed && !v.fix);
  v.wifiSaved = app.food.discoveries();
  v.bleSaved = app.bleCollection.discoveries();
  v.clueCount = std::min(3u, unsigned(app.historyCount));
  for (unsigned i = 0; i < v.clueCount; ++i) {
    auto &a = app.history[i];
    v.clues[i].family = familyShort(a.match.family);
    v.clues[i].ble = a.ble;
    v.clues[i].rssi = a.rssi;
    v.clues[i].fresh = uint32_t(now - a.at) < 300000;
    v.clues[i].confidence = a.match.confidence == Confidence::High     ? "HIGH"
                            : a.match.confidence == Confidence::Medium ? "MED"
                                                                       : "LOW";
  }
  drawHome(canvas, v);
}
static void menu() {
  heading("DASH-CHAN");
  const char *labels[] = {"BUDDY + LOOK", "SOUND + VOICE", "PERSONALITY",   "DETECTION",
                          "RECENT CLUES", "GPS + SD",      "DISPLAY + FUN", "UPLOAD DRIVES"};
  for (int i = 0; i < 8; ++i) {
    int x = 8 + i % 2 * 156, y = 42 + i / 2 * 47;
    button(x, y, 148, 39, labels[i]);
    canvas.fillRoundRect(x + 5, y + 9, 3, 21, 1,
                         i % 3 == 0   ? colors.accent
                         : i % 3 == 1 ? colors.heart
                                      : colors.danger);
  }
}
static void displaySetup() {
  heading("DISPLAY + FUN");
  button(8, 43, 304, 34, app.settings.view ? "Home: FULL FACE" : "Home: RECON + BUDDY");
  static const char *scenes[] = {"Signal City", "Sunset Run", "Star Patrol", "Pixel Garden"};
  button(8, 84, 304, 34, (std::string("Background: ") + scenes[app.settings.backdrop]).c_str());
  button(8, 125, 304, 34, "LIGHTS + SENSOR REACTIONS");
  button(8, 166, 304, 34, "NFC OUTFIT CARDS");
  text("All looks unlocked. 25 discoveries = 1 level.", 12, 216, colors.muted);
}
static void reactions() {
  heading("SENSOR REACTIONS");
  const bool states[] = {app.settings.rgb, app.settings.tilt, app.settings.gestures,
                         app.settings.proximity, app.settings.autoDim};
  const char *names[] = {"Mood lights", "Tilt + gaze", "Head swipe", "Wave hello",
                         "Auto brightness"};
  for (int i = 0; i < 5; ++i) {
    char label[40];
    snprintf(label, sizeof(label), "%s: %s", names[i], states[i] ? "ON" : "OFF");
    button(8, 42 + i * 38, 304, 32, label);
  }
}
static void nfcCards() {
  heading("NFC OUTFIT CARDS");
  button(8, 44, 304, 36, app.settings.nfc ? "NFC cards: ON" : "NFC cards: OFF");
  button(8, 91, 304, 36, "LINK NEXT CARD TO THIS OUTFIT", true);
  button(8, 138, 304, 32, "CANCEL LINKING");
  wrap(app.nfcMessage, 12, 184, 47, colors.accent, 3);
  text("Own tags only. Cards do not earn food/XP.", 12, 226, colors.muted);
}
static void uploadScreen() {
  heading("UPLOAD DRIVES");
  char label[80];
  snprintf(label, sizeof(label), "%s%s%s", uploads.wigle ? "WiGLE" : "",
           uploads.wigle && uploads.wdg ? " + " : "", uploads.wdg ? "WDGWars" : "");
  text(uploads.configured ? label : "Set up at the Dash-Chan website", 12, 43, colors.accent);
  text(uploads.automatic ? "Auto-send after Stop + Save is ON" : "Auto-send is OFF", 12, 58,
       colors.muted);
  button(8, 77, 50, 32, "<");
  button(262, 77, 50, 32, ">");
  if (uploads.count)
    text(uploads.files[uploads.selected], 66, 88, colors.text);
  else
    text("No drive selected", 66, 88, colors.muted);
  wrap(uploads.message, 12, 120, 48, colors.text, 3);
  text(uploadBusy() ? "WATCHING PAUSED during Wi-Fi upload" : "Shares GPS, SSIDs and MAC addresses",
       12, 163, colors.danger);
  button(8, 182, 210, 36,
         uploadBusy()    ? "CANCEL UPLOAD"
         : confirmUpload ? "CONFIRM SEND TO SERVICES"
                         : "UPLOAD SELECTED DRIVE",
         true);
  button(224, 182, 88, 36, "REFRESH");
  text("Original files stay on SD. HTTPS verified.", 12, 228, colors.muted);
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
    int base = int(buddyPage * 3) + a;
    if (base >= 5)
      break;
    int id = base < 3 ? base : base + 1;
    int x = 54 + 106 * a;
    bool selected = unsigned(app.settings.avatar) == unsigned(id);
    canvas.fillRoundRect(x - 49, 47, 98, 121, 10, selected ? colors.panel : colors.bg);
    BuddyLook look;
    look.base = base;
    art::Zoom<M5Canvas> zoom{canvas, x, 100, 0.62f};
    drawBuddyLook(zoom, look, 0, 0, selected ? 1 : 0, 0, now % 4000 < 100, 0, 0, int(now / 150));
    text(names[base], x - 18, 149, selected ? colors.accent : colors.text);
  }
  button(8, 184, 206, 40, (std::string("Custom: ") + app.settings.customName).c_str(),
         app.settings.avatar == Avatar::Custom);
  button(222, 184, 90, 40, buddyPage ? "< ORIGINALS" : "WISP/EMBER >");
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
// Settings for how the buddy behaves, plus its report card: tapping the vitals row lands here.
static void behavior() {
  heading("PERSONALITY");
  char s[64];
  snprintf(s, sizeof(s), "%.16s the %s", petName(), rankName(petLevel(app.settings.xp)));
  text(s, 12, 40, colors.text);
  snprintf(s, sizeof(s), "Feeling %s", feelingName(app.feeling));
  text(s, 12, 53, feelingColor(colors, app.feeling));
  const char *labels[] = {"PETS", "SNACKS", "DRIVES", "BEST DRIVE"};
  const uint32_t values[] = {app.life.pets, app.life.snacks, app.life.drives, app.life.bestDrive};
  for (int i = 0; i < 4; ++i) {
    int x = 8 + i * 77;
    canvas.fillRoundRect(x, 68, 73, 34, 7, colors.panel);
    text(labels[i], x + 6, 73, colors.muted);
    snprintf(s, sizeof(s), "%lu", (unsigned long)values[i]);
    text(s, x + 6, 87, colors.accent);
  }
  button(8, 110, 304, 34,
         !app.settings.motion            ? "Head motion: OFF"
         : app.settings.motionStyle == 1 ? "Head motion: Gentle"
                                         : "Head motion: Lively");
  button(8, 150, 304, 34, app.settings.spicy ? "Banter: Spicy" : "Banter: Gentle");
  button(8, 190, 304, 34, "PET YOUR BUDDY", true);
  text("Pets build love. Only wardriving feeds me.", 12, 229, colors.muted);
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
  snprintf(s, sizeof(s), "GPS SETUP: %s",
           app.gpsSearching ? "FINDING BAUD" : GpsPresets[app.settings.gpsProfile].name);
  button(8, 151, 304, 34, s);
  button(8, 199, 148, 33, "RETRY SD");
  button(164, 199, 148, 33, "HOME", true);
}
static void gpsSetup() {
  heading("GPS MODULE & WIRING");
  char s[80];
  snprintf(s, sizeof(s), "Module: %s", GpsPresets[gpsDraft.profile].name);
  button(8, 42, 304, 32, s);
  button(8, 82, 148, 32,
         gpsDraft.port == 0   ? "Blue Port C"
         : gpsDraft.port == 1 ? "Port A"
         : gpsDraft.port == 3 ? "Port B"
                              : "Custom Grove RX");
  snprintf(s, sizeof(s), "Host RX: G%d", gpsRxPin(gpsDraft));
  button(164, 82, 148, 32, s);
  snprintf(s, sizeof(s), "Baud: %lu", (unsigned long)gpsDraft.baud);
  button(8, 122, 148, 32, s);
  button(164, 122, 148, 32, app.gpsSearching ? "CANCEL SEARCH" : "APPLY + FIND BAUD");
  if (app.gpsSearching) {
    snprintf(s, sizeof(s), "Trying %lu baud; radios still watch", (unsigned long)app.gpsActiveBaud);
    text(s, 10, 163, colors.accent);
  } else
    text(app.session.armed    ? "Stop recording to change GPS settings"
         : usableFix(app.fix) ? "GPS READY; receiving valid fix data"
                              : app.gpsMessage,
         10, 163, colors.muted);
  text("UART receive-only; module RX unused", 10, 179, colors.muted);
  button(8, 201, 148, 32, "APPLY SETTINGS", true);
  button(164, 201, 148, 32, "WIRING GUIDE");
}
static void gpsModels() {
  heading("CHOOSE GPS MODULE");
  for (unsigned i = 0; i < 4 && gpsModelPage * 4 + i < GpsPresetCount; ++i) {
    unsigned n = gpsModelPage * 4 + i;
    button(8, 43 + i * 37, 304, 32, GpsPresets[n].name, n == gpsDraft.profile);
  }
  button(8, 201, 90, 32, "PREV");
  button(222, 201, 90, 32, "NEXT");
  char s[12];
  snprintf(s, sizeof(s), "%u / %u", gpsModelPage + 1, (GpsPresetCount + 3) / 4);
  text(s, 140, 213, colors.muted);
}
static void gpsWiring() {
  heading("GPS WIRING HELP");
  char s[64];
  snprintf(s, sizeof(s), "GPS TX -> StackChan G%d (RX)", gpsRxPin(gpsDraft));
  text(s, 10, 46, colors.accent);
  text("GPS GND -> Grove black / GND", 10, 65, colors.text);
  text("GPS RX and PPS: leave disconnected", 10, 84, colors.text);
  text("White / yellow: C G18/G17", 10, 106, colors.muted);
  text("A G1/G2 | B G8/G9", 10, 122, colors.muted);
  wrap("Grove red is 5V. Bare ATGM336H needs regulated power per its datasheet. UART logic: 3.3V "
       "only.",
       10, 145, 48, colors.danger, 3);
  wrap(GpsPresets[gpsDraft.profile].adapter
           ? "Use a UART breakout/adapter. A preset does not change the connector pinout."
           : "Unit GPS: use its Grove cable on Port C. Power off before rewiring.",
       10, 196, 48, colors.muted, 3);
}
static bool row(int y, int top, int height = 36) {
  return y >= top && y < top + height;
}
static void touch(int x, int y) {
  buddyPoke(); // Any tap counts as attention: it keeps the buddy awake.
  if (page == Home) {
    HomeTapTarget tap = homeTap(x, y, app.settings.view == 1, app.alertOpen, app.historyCount);
    switch (tap.action) {
    case HomeTap::Menu:
      page = Menu;
      break;
    case HomeTap::SwitchView:
      uiSwitchView();
      break;
    case HomeTap::Drive:
      if (app.session.armed)
        stopDrive();
      else
        startDrive();
      break;
    case HomeTap::Clue:
    case HomeTap::ClueList:
      if (app.alertOpen) // Do not leave the alert running behind the History page.
        buddyDismiss();
      detailOpen = tap.action == HomeTap::Clue;
      if (detailOpen)
        detail = app.history[tap.clue];
      historyPage = 0;
      page = History;
      fromHome = true;
      break;
    case HomeTap::Stats: // The vitals row opens the buddy's stats.
      page = Behavior;
      fromHome = true;
      break;
    case HomeTap::Dismiss:
      buddyDismiss();
      break;
    case HomeTap::Pet:
      buddyPet();
      break;
    case HomeTap::None:
      break;
    }
    return;
  }
  if (y < 34 && x >= 250) {
    page = page == Menu || (fromHome && (page == Behavior || page == History)) ? Home
           : (page == Buddies || page == Studio)                               ? Appearance
           : (page == GpsModels || page == GpsWiring)                          ? GpsSetup
           : (page == Reactions || page == NfcCards)                           ? DisplaySetup
           : page == GpsSetup                                                  ? Diagnostics
                                                                               : Menu;
    detailOpen = false;
    fromHome = false;
    return;
  }
  bool right = x >= 160;
  switch (page) {
  case Menu:
    if (y >= 42 && y < 230) {
      static const Page pages[] = {Appearance, Audio,       Behavior,     Detection,
                                   History,    Diagnostics, DisplaySetup, Uploads};
      unsigned n = (y - 42) / 47 * 2 + unsigned(right);
      if (n < 8 && (y - 42) % 47 < 39) {
        page = pages[n];
        fromHome = false;
        detailOpen = false;
        historyPage = 0;
        confirmUpload = false;
        if (page == Uploads)
          uploadRefresh();
      }
    }
    break;
  case Uploads:
    if (uploadBusy()) {
      if (row(y, 182, 36) && x < 218)
        uploadCancel();
    } else if (row(y, 77, 32) && uploads.count) {
      if (x < 58)
        uploads.selected = (uploads.selected + uploads.count - 1) % uploads.count;
      else if (x >= 262)
        uploads.selected = (uploads.selected + 1) % uploads.count;
      confirmUpload = false;
    } else if (row(y, 182, 36)) {
      if (x >= 224) {
        uploadRefresh();
        confirmUpload = false;
      } else if (uploads.configured && uploads.count) {
        if (confirmUpload) {
          uploadStart();
          confirmUpload = false;
        } else
          confirmUpload = true;
      }
    }
    break;
  case DisplaySetup:
    if (row(y, 43, 34))
      app.settings.view ^= 1;
    else if (row(y, 84, 34))
      app.settings.backdrop = (app.settings.backdrop + 1) % 4;
    else if (row(y, 125, 34))
      page = Reactions;
    else if (row(y, 166, 34))
      page = NfcCards;
    break;
  case Reactions:
    if (y >= 42 && y < 232 && (y - 42) % 38 < 32) {
      bool *flags[] = {&app.settings.rgb, &app.settings.tilt, &app.settings.gestures,
                       &app.settings.proximity, &app.settings.autoDim};
      *flags[(y - 42) / 38] = !*flags[(y - 42) / 38];
    }
    break;
  case NfcCards:
    if (row(y, 44, 36)) {
      app.settings.nfc = !app.settings.nfc;
      if (!app.settings.nfc)
        nfcCancelEnrollment();
    } else if (row(y, 91, 36))
      nfcRememberOutfit();
    else if (row(y, 138, 32))
      nfcCancelEnrollment();
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
        app.settings.customBase = (app.settings.customBase + 1) % 5;
        app.settings.avatar = Avatar::Custom;
      }
    } else if (row(y, 192, 34) && !right) {
      app.settings.avatar = Avatar::Custom;
      page = Home;
    }
    break;
  case Buddies:
    if (y >= 47 && y < 168) {
      int base = int(buddyPage * 3) + std::min(2, x / 106);
      if (base < 5)
        app.settings.avatar = Avatar(base < 3 ? base : base + 1);
    } else if (row(y, 184, 40)) {
      if (x >= 222)
        buddyPage ^= 1;
      else
        app.settings.avatar = Avatar::Custom;
    }
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
    if (row(y, 110, 34)) {
      if (!app.settings.motion) {
        app.settings.motionStyle = 1;
        buddyMotion(true);
      } else if (app.settings.motionStyle == 1)
        app.settings.motionStyle = 2;
      else
        buddyMotion(false);
    } else if (row(y, 150, 34))
      app.settings.spicy = !app.settings.spicy;
    else if (row(y, 190, 34)) {
      if (app.alertOpen) // Same as tapping the buddy on Home during an alert.
        buddyDismiss();
      else
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
    if (row(y, 151, 34)) {
      gpsDraft = gpsConfig();
      page = GpsSetup;
    } else if (row(y, 199, 33)) {
      if (right)
        page = Home;
      else if (!app.session.armed) {
        storageBegin();
        storageLoadAvatar();
      }
    }
    break;
  case GpsSetup:
    if (row(y, 201, 32) && right) {
      page = GpsWiring;
      break;
    }
    if (app.session.armed)
      break;
    if (app.gpsSearching) {
      if (row(y, 122, 32) && right)
        gpsCancelSearch();
      break;
    }
    if (row(y, 42, 32)) {
      gpsModelPage = gpsDraft.profile / 4;
      page = GpsModels;
    } else if (row(y, 82, 32)) {
      if (!right)
        gpsDraft.port = (gpsDraft.port + 1) % 4;
      else {
        int pin = gpsRxPin(gpsDraft);
        unsigned n = 0;
        while (n < GpsInputPinCount && GpsInputPins[n] != pin)
          ++n;
        gpsDraft.port = 2;
        gpsDraft.rx = GpsInputPins[(n + 1) % GpsInputPinCount];
      }
    } else if (row(y, 122, 32)) {
      if (right) {
        if (gpsApply(gpsDraft))
          gpsFindBaud();
      } else {
        unsigned n = 0;
        while (n < GpsRateCount && GpsRates[n] != gpsDraft.baud)
          ++n;
        gpsDraft.baud = GpsRates[(n + 1) % GpsRateCount];
      }
    } else if (row(y, 201, 32) && !right)
      gpsApply(gpsDraft);
    break;
  case GpsModels:
    if (app.session.armed)
      break;
    if (row(y, 201, 32))
      gpsModelPage =
          (gpsModelPage + (right ? 1 : (GpsPresetCount + 3) / 4 - 1)) % ((GpsPresetCount + 3) / 4);
    else if (y >= 43 && y < 191) {
      unsigned n = gpsModelPage * 4 + (y - 43) / 37;
      if (n < GpsPresetCount) {
        gpsDraft.profile = n;
        if (n)
          gpsDraft.baud = GpsPresets[n].baud;
        page = GpsSetup;
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
  if (!buffered) {
    M5.Display.println("Display buffer allocation failed");
    Serial.println("UI allocation failed");
  }
}
void uiBoot() {
  if (!buffered)
    return;
  colors = palette(app.settings.theme);
  canvas.fillSprite(0xffff);
  canvas.drawPng(bootBwm, sizeof(bootBwm), 21, 34);
  canvas.drawPng(bootVts, sizeof(bootVts), 178, 34);
  mark(67, 183, 2);
  text("DASH-CHAN", 112, 187, Ink, 2);
  text("Your tiny road buddy is waking up", 64, 216, Ink);
  canvas.pushSprite(0, 0);
  delay(1500);
}
void uiTick() {
  if (gpsWasSearching && !app.gpsSearching)
    gpsDraft = gpsConfig();
  gpsWasSearching = app.gpsSearching;
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
  case DisplaySetup:
    displaySetup();
    break;
  case Reactions:
    reactions();
    break;
  case NfcCards:
    nfcCards();
    break;
  case Uploads:
    uploadScreen();
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
  case GpsSetup:
    gpsSetup();
    break;
  case GpsModels:
    gpsModels();
    break;
  case GpsWiring:
    gpsWiring();
    break;
  }
  canvas.pushSprite(0, 0);
}
void uiSwitchView() {
  app.settings.view ^= 1;
  saveSettings();
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
