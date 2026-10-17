#include "app.h"
#include "voice.h"
#include <M5StackChan.h>
#include <algorithm>
namespace dash {
static uint32_t lastMotion = 0, lastNeed = 0, lastChatter = 0, lastSave = 0, loveAt = 0,
                lastSiren = 0;
static Cooldown petTimer, feedTimer;
static const char *idle[3][4] = {
    {"Tiny paws. Big curiosity.", "Your copilot requests one snack.",
     "I can hear the invisible internet.", "My whiskers are on patrol."},
    {"Small dragon. Excellent lookout.", "Collecting signals for my hoard.",
     "I brought courage. And snacks.", "My wings are in airplane-ish mode."},
    {"Beep. Friendship signal: strong.", "Processing... you are my favorite.",
     "No cloud. Just a little personality.", "My emotional support human is here."}};
void buddyMotion(bool on) {
  app.settings.motion = on;
  if (on) {
    M5StackChan.setServoPowerEnabled(true);
    M5StackChan.Motion.move(0, 200, 180);
  } else {
    M5StackChan.Motion.stop();
    M5StackChan.Motion.setTorqueEnabled(false);
    M5StackChan.setServoPowerEnabled(false);
  }
  saveSettings();
}
void buddyBegin() {
  M5StackChan.Motion.setAutoTorqueReleaseEnabled(true);
  buddyMotion(app.settings.motion);
  lastNeed = lastChatter = lastSave = millis();
}
void buddyPet() {
  if (!petTimer.ready(millis(), 800))
    return;
  petTimer.mark(millis());
  app.bond = std::min(100, int(app.bond) + 4);
  app.mood = Mood::Love;
  loveAt = millis();
  say("Hehe. That's the good scritch.");
  if (app.settings.sound)
    M5.Speaker.tone(1046, 70);
}
void buddyFeed() {
  if (!feedTimer.ready(millis(), 10000)) {
    say("Still chewing. Tiny mouth, remember?");
    return;
  }
  feedTimer.mark(millis());
  app.energy = std::min(100, int(app.energy) + 18);
  ++app.settings.xp;
  app.mood = Mood::Love;
  loveAt = millis();
  say(app.settings.avatar == Avatar::Sprout ? "Crunch! A snack for my signal hoard."
                                            : "Nom nom. Adventure fuel acquired!");
  if (app.settings.sound)
    M5.Speaker.tone(1320, 100);
}
void buddyAlert(const Alert &a, bool demo) {
  app.active = a;
  app.alertAt = millis();
  app.alertOpen = true;
  app.demo = demo;
  app.mood = Mood::Alarm;
  lastMotion = 0;
  if (!demo) {
    ++app.alerts;
    for (size_t i = HistoryCapacity - 1; i > 0; --i)
      app.history[i] = app.history[i - 1];
    app.history[0] = a;
    if (app.historyCount < HistoryCapacity)
      ++app.historyCount;
    logAlert(a);
  }
  bool police = a.match.family == Family::Axon;
  unsigned line = esp_random() % 3;
  say(police ? (app.settings.spicy ? (line == 0   ? "PIG DETECTED! ACT NATURAL!"
                                      : line == 1 ? "Oh man, the fuzz!"
                                                  : "Wee-woo! Tiny copilot panic!")
                                   : "Possible body-camera signal nearby.")
      : a.match.family == Family::Tracker   ? "A little tracker signal just popped up!"
      : a.match.family == Family::Meta      ? "Glasses signal! Is this my good side?"
      : a.match.family == Family::Biscuit   ? "Biscuit nearby! Wait... not a snack?"
      : a.match.family == Family::Flipper   ? "A wild dolphin gadget appeared!"
      : a.match.family == Family::Pineapple ? "Spicy fruit on the airwaves!"
                                            : "Psst. I spotted an interesting signal!");
  if (app.settings.sound) {
    if (police && app.settings.spicy && line < 2) {
      const uint8_t *wav = line == 0 ? voicePig : voiceFuzz;
      size_t len = line == 0 ? sizeof(voicePig) : sizeof(voiceFuzz);
      M5.Speaker.playWav(wav, len);
    } else
      M5.Speaker.tone(police ? 880 : 1174, 160);
  }
}
void buddyDismiss() {
  app.alertOpen = false;
  app.demo = false;
  app.mood = Mood::Curious;
  M5.Speaker.stop();
  say("Still here. Still keeping watch.");
}
void buddyTick() {
  uint32_t now = millis();
  if (M5StackChan.TouchSensor.wasPressed() && !app.alertOpen)
    buddyPet();
  if (app.alertOpen && uint32_t(now - app.alertAt) > 8500)
    buddyDismiss();
  if (app.mood == Mood::Love && uint32_t(now - loveAt) > 3000)
    app.mood = Mood::Happy;
  if (uint32_t(now - lastNeed) > 60000) {
    lastNeed = now;
    if (app.energy > 10)
      --app.energy;
    if (app.bond > 10)
      --app.bond;
  }
  if (!app.alertOpen && uint32_t(now - lastChatter) > 22000) {
    lastChatter = now;
    app.mood = app.energy < 30 ? Mood::Sleepy : Mood::Curious;
    say(app.energy < 30 ? "A tiny snack would be wonderful."
                        : idle[unsigned(app.settings.avatar)][esp_random() % 4]);
  }
  if (app.alertOpen && app.settings.sound && app.active.match.family == Family::Axon &&
      !M5.Speaker.isPlaying() && uint32_t(now - lastSiren) > 450 &&
      uint32_t(now - app.alertAt) < 2400) {
    lastSiren = now;
    M5.Speaker.tone((now / 450) % 2 ? 660 : 880, 140);
  }
  if (app.settings.motion && uint32_t(now - lastMotion) > (app.alertOpen ? 600u : 4200u)) {
    lastMotion = now;
    int yaw = app.alertOpen ? ((now / 600) % 2 ? 220 : -220) : int(esp_random() % 401) - 200;
    int pitch = app.alertOpen ? ((now / 600) % 2 ? 300 : 130) : 180 + esp_random() % 120;
    // BSP units are tenths of a degree. Stay well inside K151 travel limits.
    M5StackChan.Motion.move(yaw, pitch, app.alertOpen ? 320 : 160);
  }
  if (uint32_t(now - lastSave) > 300000) {
    lastSave = now;
    saveSettings();
  }
}
} // namespace dash
