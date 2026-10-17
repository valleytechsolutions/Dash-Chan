#include "profile.h"
#include "app.h"
namespace dash {
bool applyAvatarConfig(const char *line) {
  AvatarProfile p;
  if (!parseAvatar(line, p))
    return false;
  app.settings.customBase = p.base;
  app.settings.customAccessory = p.accessory;
  app.settings.customBody = rgb565(p.body);
  app.settings.customAccent = rgb565(p.accent);
  app.settings.customEyes = p.eyes;
  app.settings.customEye = rgb565(p.eye);
  app.settings.customHeadwear = p.headwear;
  app.settings.customEyewear = p.eyewear;
  app.settings.customNeckwear = p.neckwear;
  strcpy(app.settings.customName, p.name);
  app.settings.avatar = Avatar::Custom;
  saveSettings();
  say("A new look! Same little signal goblin.");
  return true;
}
void printAvatarConfig() {
  Serial.printf(
      "AVATAR {\"schema\":2,\"base\":%u,\"body565\":%u,\"accent565\":%u,\"accessory\":%u,"
      "\"eyes\":%u,\"eye565\":%u,\"headwear\":%u,\"eyewear\":%u,\"neckwear\":%u,\"name\":\"%s\"}\n",
      app.settings.customBase, app.settings.customBody, app.settings.customAccent,
      app.settings.customAccessory, app.settings.customEyes, app.settings.customEye,
      app.settings.customHeadwear, app.settings.customEyewear, app.settings.customNeckwear,
      app.settings.customName);
}
} // namespace dash
