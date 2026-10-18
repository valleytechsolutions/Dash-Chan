// SPDX-License-Identifier: GPL-3.0-or-later
#include "app.h"
#include "uploads.h"
#include <M5StackChan.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedNFC.h>
#include <Preferences.h>
#include <algorithm>
namespace dash {
static m5::unit::UnitUnified units;
static m5::unit::UnitNFC nfcUnit{};
static m5::nfc::NFCLayerA nfc{nfcUnit};
static bool triedNfc = false, enrolling = false, near = false;
static uint32_t enrollAt = 0, lastCardAt = 0, tickAt = 0, nfcAt = 0;
static String lastUid;
struct Outfit {
  uint8_t version = 1, avatar, base, eyes, head, glasses, neck, theme, scene;
  uint16_t body, accent, eye;
  char name[17];
};
static Outfit outfit() {
  auto &s = app.settings;
  Outfit o{};
  o.avatar = unsigned(s.avatar);
  o.base = s.customBase;
  o.eyes = s.customEyes;
  o.head = s.customHeadwear;
  o.glasses = s.customEyewear;
  o.neck = s.customNeckwear;
  o.theme = unsigned(s.theme);
  o.scene = s.backdrop;
  o.body = s.customBody;
  o.accent = s.customAccent;
  o.eye = s.customEye;
  memcpy(o.name, s.customName, sizeof(o.name));
  return o;
}
static void dress(const Outfit &o) {
  if (o.version != 1 || o.avatar > 5 || o.base > 4 || o.eyes > 5 || o.head > 5 || o.glasses > 3 ||
      o.neck > 3 || o.theme > 3 || o.scene > 3 || o.name[16])
    return;
  auto &s = app.settings;
  s.avatar = Avatar(o.avatar);
  s.customBase = o.base;
  s.customEyes = o.eyes;
  s.customHeadwear = o.head;
  s.customEyewear = o.glasses;
  s.customNeckwear = o.neck;
  s.theme = Theme(o.theme);
  s.backdrop = o.scene;
  s.customBody = o.body;
  s.customAccent = o.accent;
  s.customEye = o.eye;
  memcpy(s.customName, o.name, sizeof(s.customName));
  saveSettings();
  say("New outfit. Same tiny menace.");
}
void nfcRememberOutfit() {
  if (app.session.armed || uploadBusy()) {
    snprintf(app.nfcMessage, sizeof(app.nfcMessage), "Finish your drive or upload first.");
    return;
  }
  app.settings.nfc = true;
  saveSettings();
  enrolling = true;
  enrollAt = millis();
  lastUid = "";
  snprintf(app.nfcMessage, sizeof(app.nfcMessage), "Hold your NFC tag near the base. 30 seconds.");
}
void nfcCancelEnrollment() {
  enrolling = false;
  snprintf(app.nfcMessage, sizeof(app.nfcMessage), "Linking canceled. Saved outfits remain.");
}
void hardwareBegin() {
  uint8_t id[2]{};
  app.proximityReady =
      M5.In_I2C.readRegister(0x23, 0x86, id, 2, 100000) && (id[0] & 0xf0) == 0x90 && id[1] == 0x05;
  M5StackChan.showRgbColor(0, 0, 0);
}
static void nfcTick(uint32_t now) {
  if (enrolling && uint32_t(now - enrollAt) > 30000)
    nfcCancelEnrollment();
  if (!app.settings.nfc || app.session.armed || uploadBusy() || uint32_t(now - nfcAt) < 750)
    return;
  nfcAt = now;
  if (!triedNfc) {
    triedNfc = true;
    app.nfcReady = units.add(nfcUnit, M5.In_I2C) && units.begin();
    if (!app.nfcReady)
      snprintf(app.nfcMessage, sizeof(app.nfcMessage), "NFC unavailable. Restart to try again.");
  }
  if (!app.nfcReady)
    return;
  units.update();
  m5::nfc::a::PICC card;
  bool found = nfc.detect(card, 15);
  nfc.deactivate();
  if (!found) {
    if (uint32_t(now - lastCardAt) > 1800)
      lastUid = "";
    return;
  }
  String uid(card.uidAsString().c_str());
  lastCardAt = now;
  if (uid == lastUid)
    return;
  lastUid = uid;
  Preferences p;
  if (!p.begin("dash-outfits", false))
    return;
  int slot = -1;
  for (int i = 0; i < 8; ++i) {
    char key[8];
    snprintf(key, sizeof(key), "uid%d", i);
    if (p.getString(key) == uid) {
      slot = i;
      break;
    }
  }
  if (enrolling) {
    if (slot < 0) {
      slot = p.getUChar("next", 0) % 8;
      p.putUChar("next", (slot + 1) % 8);
    }
    char key[8];
    snprintf(key, sizeof(key), "uid%d", slot);
    p.putString(key, uid);
    snprintf(key, sizeof(key), "look%d", slot);
    auto o = outfit();
    p.putBytes(key, &o, sizeof(o));
    enrolling = false;
    snprintf(app.nfcMessage, sizeof(app.nfcMessage),
             "Outfit linked! Up to 8 cards, oldest replaced.");
    say("My outfit has a calling card now.");
  } else if (slot >= 0) {
    char key[8];
    snprintf(key, sizeof(key), "look%d", slot);
    Outfit o{};
    if (p.getBytesLength(key) == sizeof(o) && p.getBytes(key, &o, sizeof(o)) == sizeof(o))
      dress(o);
  } else
    snprintf(app.nfcMessage, sizeof(app.nfcMessage), "New tag. Tap LINK to save this outfit.");
  p.end();
}
void hardwareTick() {
  auto now = millis();
  if (app.settings.gestures &&
      (M5StackChan.TouchSensor.wasSwipedForward() || M5StackChan.TouchSensor.wasSwipedBackward()))
    uiSwitchView();
  if (uint32_t(now - tickAt) < 100)
    return;
  tickAt = now;
  float ax = 0, ay = 0, az = 0;
  bool tilt = app.settings.tilt && M5.Imu.isEnabled() && M5.Imu.getAccel(&ax, &ay, &az);
  app.tiltX += ((tilt ? constrain(ay * 5.f, -4.f, 4.f) : 0) - app.tiltX) * .15f;
  app.tiltY += ((tilt ? constrain(ax * 3.f, -2.f, 2.f) : 0) - app.tiltY) * .15f;
  if (app.settings.rgb) {
    float pulse = .55f + .35f * sinf(now / 900.f);
    unsigned r = 45, g = 200, b = 185;
    if (app.alertOpen) {
      r = 255;
      g = 75;
      b = 18;
    } else if (app.mood == Mood::Love) {
      r = 255;
      g = 80;
      b = 160;
    } else if (app.levelUpAt && now - app.levelUpAt < 5000) {
      r = 255;
      g = 200;
      b = 40;
    } else if (!app.session.armed) {
      r = 130;
      g = 100;
      b = 255;
    }
    unsigned brightness = std::min(80u, unsigned(app.settings.ledBrightness));
    M5StackChan.showRgbColor(r * pulse * brightness / 255, g * pulse * brightness / 255,
                             b * pulse * brightness / 255);
  } else
    M5StackChan.showRgbColor(0, 0, 0);
  static int lastModes = -1, brightness = 160;
  int modes = app.settings.proximity | (app.settings.autoDim << 1);
  if (app.proximityReady && lastModes != modes) {
    M5.In_I2C.writeRegister8(0x23, 0x80, app.settings.autoDim ? 1 : 0, 100000);
    M5.In_I2C.writeRegister8(0x23, 0x81, app.settings.proximity ? 3 : 0, 100000);
    lastModes = modes;
  }
  if (app.proximityReady && app.settings.proximity) {
    uint8_t data[2]{};
    if (M5.In_I2C.readRegister(0x23, 0x8d, data, 2, 100000)) {
      unsigned value = data[0] | ((data[1] & 7) << 8);
      if (!near && value > 250 && !app.alertOpen && !app.session.armed &&
          (!app.greetAt || uint32_t(now - app.greetAt) > 60000)) {
        app.greetAt = now;
        buddyPet(false);
        say("Oh! My favorite human-shaped signal.");
      }
      if (value > 250)
        near = true;
      if (value < 150)
        near = false;
    }
  }
  int target = 160;
  if (app.proximityReady && app.settings.autoDim) {
    uint8_t data[2]{};
    if (M5.In_I2C.readRegister(0x23, 0x8a, data, 2, 100000)) {
      unsigned value = data[0] | (data[1] << 8);
      target = value < 15 ? 70 : value < 150 ? 125 : 190;
    }
  }
  if (brightness != target) {
    brightness += target > brightness ? 1 : -1;
    M5.Display.setBrightness(brightness);
  }
  nfcTick(now);
}
} // namespace dash
