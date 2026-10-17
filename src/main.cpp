#include "app.h"
#include <M5StackChan.h>
#include <Preferences.h>
#include <algorithm>
namespace dash {
State app;
Contact contacts[ContactCapacity];
std::atomic<uint32_t> captureGeneration{0};
void loadSettings() {
  Preferences p;
  p.begin("dashchan", true);
  app.settings.avatar = Avatar(p.getUChar("avatar", 0) % 3);
  app.settings.sound = p.getBool("sound", true);
  app.settings.motion = p.getBool("motion", false);
  app.settings.spicy = p.getBool("spicy", true);
  app.settings.threshold = Confidence(constrain(p.getUChar("filter", 2), 1, 3));
  app.settings.gpsBaud = p.getUInt("baud", GpsBaud);
  app.settings.gpsPort = p.getUChar("gpsport", 0) % 2;
  app.settings.xp = p.getUInt("xp", 0);
  app.energy = std::min(100, int(p.getUChar("fuel", 85)));
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
  dash::buddyBegin();
  dash::gpsBegin();
  dash::storageBegin();
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
