#include "app.h"
#include "idle_motion.h"
#include "voice.h"
#include <M5StackChan.h>
#include <algorithm>
namespace dash {
static uint32_t lastMotion = 0, lastNeed = 0, lastChatter = 0, lastSave = 0, loveAt = 0,
                lastSiren = 0;
static Cooldown petTimer;
static IdleMotion idleMotion;
static float gazeTarget = 0;
static uint32_t lastGaze = 0;
static unsigned panicStep = 0;
static uint32_t panicWait = 0;
static const char *idle[3][4] = {
    {"Tiny paws. Big curiosity.", "Let's find networks. I'm peckish!",
     "I can hear the invisible internet.", "My whiskers are on patrol."},
    {"Small dragon. Excellent lookout.", "Collecting signals for my hoard.",
     "New networks taste like adventure.", "My wings are in airplane-ish mode."},
    {"Beep. Friendship signal: strong.", "Processing... you are my favorite.",
     "No cloud. Just a little personality.", "My emotional support human is here."}};
void buddyMotion(bool on) {
  app.settings.motion = on;
  idleMotion.reset(millis());
  gazeTarget = 0;
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
  idleMotion.reset(loveAt, 3500);
  gazeTarget = 0;
  if (app.settings.motion)
    M5StackChan.Motion.move(0, 230, 110);
  say("Hehe. That's the good scritch.");
  if (app.settings.sound)
    M5.Speaker.tone(1046, 70);
}
void buddyDiscovery(const uint8_t mac[6]) {
  if (!app.session.armed || !app.food.discover(mac))
    return;
  ++app.settings.xp;
  if (app.food.progress() != 0)
    return;
  app.energy = std::min(100, int(app.energy) + 12);
  // Never replace an active signal warning with snack chatter.
  if (app.alertOpen)
    return;
  app.mood = Mood::Love;
  loveAt = millis();
  say(app.settings.avatar == Avatar::Sprout ? "Five new networks! Crunch. Signal hoard snack!"
                                            : "Nom nom! Five new networks earned a snack!");
  if (app.settings.sound)
    M5.Speaker.tone(1320, 100);
}
void buddyAlert(const Alert &a, bool demo) {
  app.active = a;
  app.alertAt = millis();
  app.alertOpen = true;
  app.demo = demo;
  app.mood = Mood::Alarm;
  lastMotion = app.alertAt;
  lastSiren = app.alertAt - 220;
  panicStep = 0;
  panicWait = 0;
  idleMotion.reset(millis(), 4000);
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
  unsigned line = demo ? 0 : esp_random() % 3;
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
    M5.Speaker.stop();
    M5.Speaker.setVolume(police && app.settings.spicy ? 220 : 170);
    if (police && app.settings.spicy && line < 2) {
      const uint8_t *wav = line == 0 ? voicePig : voiceFuzz;
      size_t len = line == 0 ? sizeof(voicePig) : sizeof(voiceFuzz);
      M5.Speaker.playWav(wav, len, 1, 0, true);
    } else
      M5.Speaker.tone(police ? 880 : 1174, 160);
  }
}
void buddyDismiss() {
  app.alertOpen = false;
  app.demo = false;
  app.mood = Mood::Curious;
  M5.Speaker.stop();
  M5.Speaker.setVolume(128);
  idleMotion.reset(millis(), 4500);
  gazeTarget = 0;
  if (app.settings.motion)
    M5StackChan.Motion.move(0, 200, 100);
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
    say(app.energy < 30 ? "Let's wardrive! New networks are my snacks."
                        : idle[unsigned(app.settings.avatar)][esp_random() % 4]);
  }
  if (app.alertOpen && app.settings.sound && app.settings.spicy &&
      app.active.match.family == Family::Axon && !M5.Speaker.isPlaying(0) &&
      uint32_t(now - lastSiren) > 220 && uint32_t(now - app.alertAt) < 6500) {
    lastSiren = now;
    M5.Speaker.tone((now / 220) % 2 ? 650 : 1150, 180, 1, true);
  }
  if (app.alertOpen && app.settings.motion && uint32_t(now - lastMotion) >= panicWait) {
    lastMotion = now;
    auto cue = panicMotion(panicStep++);
    panicWait = cue.holdMs;
    if (!app.settings.spicy || app.active.match.family != Family::Axon) {
      cue.yaw /= 2;
      cue.pitch = 200 + (cue.pitch - 200) / 2;
      cue.speed = 260;
      panicWait += 300;
    }
    // BSP units are tenths of a degree. Stay well inside K151 travel limits.
    M5StackChan.Motion.move(cue.yaw, cue.pitch, cue.speed);
  } else if (!app.alertOpen) {
    MotionCue cue;
    if (idleMotion.tick(now, esp_random(), app.energy < 30, unsigned(app.settings.avatar),
                        app.session.armed, cue)) {
      gazeTarget = cue.yaw / 35.0f;
      if (app.settings.motion)
        M5StackChan.Motion.move(cue.yaw, cue.pitch, cue.speed);
    }
  }
  if (uint32_t(now - lastGaze) >= 40) {
    lastGaze = now;
    app.gaze += (gazeTarget - app.gaze) * 0.22f;
  }
  if (uint32_t(now - lastSave) > 300000) {
    lastSave = now;
    saveSettings();
  }
}
} // namespace dash
