#include "app.h"
#include <M5StackChan.h>
#include <Preferences.h>
#include <algorithm>
namespace dash {
State app;
Contact contacts[ContactCapacity];
Watchlist watchlist;
std::atomic<uint32_t> captureGeneration{0};
void loadSettings() {
  Preferences p;
  p.begin("dashchan", true);
  app.settings.avatar = Avatar(p.getUChar("avatar", 0) % 4);
  app.settings.sound = p.getBool("sound", true);
  app.settings.motion = p.getBool("motion", false);
  app.settings.spicy = p.getBool("spicy", true);
  app.settings.threshold = Confidence(constrain(p.getUChar("filter", 2), 1, 3));
  app.settings.gpsBaud = p.getUInt("baud", GpsBaud);
  app.settings.gpsPort = p.getUChar("gpsport", 0) % 2;
  app.settings.xp = p.getUInt("xp", 0);
  app.energy = std::min(100, int(p.getUChar("fuel", 85)));
  app.settings.theme = Theme(p.getUChar("theme", 0) % 4);
  app.settings.tone = AlertTone(p.getUChar("tone", 0) % 5);
  app.settings.volume = p.getUChar("volume", 245);
  app.settings.motionStyle = std::min(2, int(p.getUChar("moves", 2)));
  app.settings.voice = p.getBool("voice", false);
  app.settings.repeatAlerts = p.getBool("repeat", false);
  app.settings.familyQuietMs = constrain(p.getUInt("quietms", 60000), 15000u, 600000u);
  app.settings.customBase = p.getUChar("custombase", 0) % 3;
  app.settings.customAccessory = p.getUChar("accessory", 0) % 4;
  app.settings.customBody = p.getUShort("body", 0xff7a);
  app.settings.customAccent = p.getUShort("accent", 0xfbb2);
  p.getString("petname", app.settings.customName, sizeof(app.settings.customName));
  if (!*app.settings.customName)
    strcpy(app.settings.customName, "My buddy");
  p.end();
}
void saveSettings() {
  Preferences p;
  p.begin("dashchan", false);
  p.putUChar("avatar", uint8_t(app.settings.avatar));
  p.putBool("sound", app.settings.sound);
  p.putBool("motion", app.settings.motion);
  p.putBool("spicy", app.settings.spicy);
  p.putUChar("filter", uint8_t(app.settings.threshold));
  p.putUInt("baud", app.settings.gpsBaud);
  p.putUChar("gpsport", app.settings.gpsPort);
  p.putUInt("xp", app.settings.xp);
  p.putUChar("fuel", app.energy);
  p.putUChar("theme", uint8_t(app.settings.theme));
  p.putUChar("tone", uint8_t(app.settings.tone));
  p.putUChar("volume", app.settings.volume);
  p.putUChar("moves", app.settings.motionStyle);
  p.putBool("voice", app.settings.voice);
  p.putBool("repeat", app.settings.repeatAlerts);
  p.putUInt("quietms", app.settings.familyQuietMs);
  p.putUChar("custombase", app.settings.customBase);
  p.putUChar("accessory", app.settings.customAccessory);
  p.putUShort("body", app.settings.customBody);
  p.putUShort("accent", app.settings.customAccent);
  p.putString("petname", app.settings.customName);
  p.end();
}
void say(const char *text) {
  snprintf(app.message, sizeof(app.message), "%s", text);
  app.messageAt = millis();
}
} // namespace dash
void setup() {
  Serial.begin(115200);
  dash::loadSettings();
  M5StackChan.begin();
  M5.Display.setRotation(1);
  M5.Display.setBrightness(160);
  M5.Speaker.setVolume(128);
  dash::uiBegin();
  dash::uiBoot();
  dash::buddyBegin();
  dash::gpsBegin();
  dash::storageBegin();
  dash::storageLoadAvatar();
  dash::radioBegin();
  Serial.printf("DASHCHAN %s ready\n", dash::Version);
  dash::printStatus();
}
void loop() {
  M5StackChan.update();
  dash::gpsTick();
  dash::radioTick();
  dash::storageTick();
  dash::buddyTick();
  dash::uiTick();
  dash::serialTick();
  delay(2);
}
