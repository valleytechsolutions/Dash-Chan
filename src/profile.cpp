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
  strcpy(app.settings.customName, p.name);
  app.settings.avatar = Avatar::Custom;
  saveSettings();
  say("A new look! Same little signal goblin.");
  return true;
}
void printAvatarConfig() {
  Serial.printf(
      "AVATAR {\"base\":%u,\"body565\":%u,\"accent565\":%u,\"accessory\":%u,\"name\":\"%s\"}\n",
      app.settings.customBase, app.settings.customBody, app.settings.customAccent,
      app.settings.customAccessory, app.settings.customName);
}
} // namespace dash
