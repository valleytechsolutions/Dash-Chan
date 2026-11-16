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
  app.settings.customMark = p.mark;
  app.settings.customBack = p.back;
  app.settings.customHeld = p.held;
  app.settings.customAura = p.aura;
  app.settings.customPack = p.pack;
  app.settings.customPersona = p.persona;
  strcpy(app.settings.customName, p.name);
  app.settings.avatar = Avatar::Custom;
  saveSettings();
  say("A new look! Same little signal goblin.");
  return true;
}
void printAvatarConfig() {
  Serial.printf(
      "AVATAR {\"schema\":%u,\"base\":%u,\"body565\":%u,\"accent565\":%u,\"accessory\":%u,"
      "\"eyes\":%u,\"eye565\":%u,\"headwear\":%u,\"eyewear\":%u,\"neckwear\":%u,"
      "\"mark\":%u,\"back\":%u,\"held\":%u,\"aura\":%u,\"pack\":%u,\"persona\":%u,"
      "\"name\":\"%s\"}\n",
      limits::Schema, app.settings.customBase, app.settings.customBody, app.settings.customAccent,
      app.settings.customAccessory, app.settings.customEyes, app.settings.customEye,
      app.settings.customHeadwear, app.settings.customEyewear, app.settings.customNeckwear,
      app.settings.customMark, app.settings.customBack, app.settings.customHeld,
      app.settings.customAura, app.settings.customPack, app.settings.customPersona,
      app.settings.customName);
}
} // namespace dash
