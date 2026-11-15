#include "app.h"
#include "idle_motion.h"
#include "petting.h"
#include "voice.h"
#include <M5StackChan.h>
#include <algorithm>
namespace dash {
static uint32_t lastMotion = 0, lastNeed = 0, lastChatter = 0, lastSave = 0, lastSiren = 0;
static Cooldown petTimer;
static Cooldown loveVoiceTimer;
static PetTouch headTouch;
static uint32_t soundAt = 0, soundStepAt = 0;
static unsigned soundStep = 0;
static bool soundActive = false;
static AlertTone playingTone = AlertTone::Personality;
static IdleMotion idleMotion;
static float gazeTarget = 0;
static uint32_t lastGaze = 0;
static unsigned panicStep = 0;
static uint32_t panicWait = 0;
static Banter banter;
static PetCombo petCombo;
// lastTouch is the "someone is paying attention to me" clock that drives boredom and naps.
static uint32_t lastTouch = 0, chatterGap = 20000, moodAt = 0, moodHold = 0, burstAt = 0,
                lastFindAt = 0, lastJackpot = 0, lastRemark = 0, stoppedAt = 0, noFixAt = 0,
                needMinute = 0, lastSnackFuss = 0;
static unsigned burstFinds = 0;
static bool greeted = false;
struct Note {
  uint16_t hz, ms;
};
static const Note chirpPurr[] = {{880, 60}, {1046, 80}}, chirpCoo[] = {{988, 50}, {1319, 90}},
                  chirpTrill[] = {{1175, 45}, {1397, 45}, {1175, 70}},
                  chirpGiggle[] = {{1319, 45}, {1568, 45}, {1319, 45}, {1760, 80}},
                  chirpEnough[] = {{700, 90}, {520, 140}}, chirpYawn[] = {{784, 110}, {587, 180}},
                  chirpSnack[] = {{1320, 70}, {1760, 90}};
static const Note *jingle = nullptr;
static unsigned jingleLeft = 0;
static uint32_t jingleAt = 0, jingleWait = 0;
template <size_t N> static void chirp(const Note (&notes)[N]) {
  if (!app.settings.sound || !app.settings.volume || soundActive || app.alertOpen ||
      M5.Speaker.isPlaying())
    return;
  jingle = notes;
  jingleLeft = N;
  jingleAt = millis();
  jingleWait = 0;
}
static unsigned who() {
  return avatarBase(app.settings.avatar, app.settings.customBase);
}
static const char *line(Topic topic) {
  return banter.pick(topic, who(), app.settings.spicy, esp_random());
}
void buddySay(Topic topic) {
  say(line(topic));
}
static bool onTheRoad() {
  return app.session.armed || app.moving;
}
static void feel(Mood mood, uint32_t holdMs) {
  app.mood = mood;
  moodAt = millis();
  moodHold = holdMs;
}
static Mood restingMood(Feeling f) {
  return f == Feeling::Napping || f == Feeling::Drowsy  ? Mood::Sleepy
         : f == Feeling::Hungry || f == Feeling::Lonely ? Mood::Pout
                                                        : Mood::Happy;
}
// Any touch or tap counts as attention. A sleeping pet wakes up, a bit groggy.
void buddyPoke() {
  lastTouch = millis();
  if (app.feeling == Feeling::Napping && !app.alertOpen)
    buddySay(Topic::Wake); // buddyTick re-evaluates the feeling on the next pass.
}
// Someone leaned in (proximity sensor). A hello, not a pet: it wakes the buddy and warms it a
// little without counting toward pets or combos.
void buddyGreet() {
  bool asleep = app.feeling == Feeling::Napping;
  lastTouch = millis();
  app.bond = std::min(100, int(app.bond) + 1);
  feel(Mood::Love, 2500);
  buddySay(asleep ? Topic::Wake : Topic::Greet);
}
// Wall-clock time, when anything has told us what it is. Seconds since 2000 UTC, or 0.
static uint32_t clockBase = 0, clockAt = 0;
static bool awayChecked = false, rtcSynced = false;
static uint32_t clockNow(uint32_t now) {
  return clockBase ? clockBase + uint32_t(now - clockAt) / 1000 : 0;
}
static void clockLearn(uint32_t seconds, uint32_t now) {
  if (!seconds)
    return;
  clockBase = seconds;
  clockAt = now;
}
// The first time we learn the time after boot, catch up on the hours spent switched off.
static bool catchUpOnTimeApart(uint32_t now) {
  uint32_t seconds = clockNow(now);
  if (awayChecked || !seconds)
    return false;
  awayChecked = true;
  uint32_t hours = awayHours(app.life.lastSeen, seconds);
  app.life.lastSeen = seconds;
  if (!hours)
    return false;
  applyAway(app.energy, app.bond, hours);
  char text[64];
  if (app.alertOpen || !awayLine(text, sizeof(text), hours, esp_random()))
    return false;
  say(text);
  return true;
}
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
  M5StackChan.TouchSensor.setResponseCycles(5);
  M5StackChan.Motion.setAutoTorqueReleaseEnabled(true);
  buddyMotion(app.settings.motion);
  lastNeed = lastChatter = lastSave = lastTouch = lastFindAt = lastJackpot = lastRemark =
      stoppedAt = noFixAt = millis();
  lastSnackFuss = millis() - 20000;
  ++app.life.boots;
  // The board clock is only trusted after GPS has set it at least once (year check).
  if (M5.Rtc.isEnabled()) {
    auto t = M5.Rtc.getDateTime();
    char stamp[24];
    snprintf(stamp, sizeof(stamp), "%04d-%02d-%02d %02d:%02d:%02d", t.date.year, t.date.month,
             t.date.date, t.time.hours, t.time.minutes, t.time.seconds);
    clockLearn(utcSeconds(stamp), millis());
  }
}
void buddyPet(bool allowVoice) {
  uint32_t now = millis();
  if (!petTimer.ready(now, 800))
    return;
  petTimer.mark(now);
  Feeling before = app.feeling;
  lastTouch = now;
  unsigned streak = petCombo.pet(now);
  ++app.pets;
  ++app.life.pets;
  app.bond = std::min(100, int(app.bond) + int(petBondGain(streak)));
  Topic topic = petTopic(streak, before, app.moving, app.bond, esp_random());
  bool enough = topic == Topic::PetEnough;
  feel(enough ? Mood::Curious : Mood::Love, 3000);
  idleMotion.reset(now, 3500);
  gazeTarget = 0;
  if (app.settings.motion)
    M5StackChan.Motion.move(enough ? (esp_random() & 1 ? 140 : -140) : 0, 230, 110);
  const char *text = line(topic);
  char counted[64];
  if (topic == Topic::Pet && milestone(app.life.pets)) {
    petMilestoneLine(counted, sizeof(counted), app.life.pets, esp_random());
    text = counted;
  }
  say(text);
  // Sensor affection is visual; only deliberate screen pets make noise.
  if (!allowVoice || !app.settings.sound || !app.settings.volume)
    return;
  M5.Speaker.setVolume(app.settings.volume);
  // The recorded clip only plays when the pet actually says that line.
  if (text == LoveYourFace && app.settings.voice && loveVoiceTimer.ready(now, 60000)) {
    loveVoiceTimer.mark(now);
    jingleLeft = 0;
    M5.Speaker.playWav(voiceLove, sizeof(voiceLove), 1, 0, true);
  } else if (enough)
    chirp(chirpEnough);
  else if (topic == Topic::Wake)
    chirp(chirpYawn);
  else if (topic == Topic::PetGiggle)
    chirp(chirpGiggle);
  else
    switch (esp_random() % 3) {
    case 0:
      chirp(chirpPurr);
      break;
    case 1:
      chirp(chirpCoo);
      break;
    default:
      chirp(chirpTrill);
    }
}
static void beginTone(AlertTone tone) {
  playingTone = tone;
  jingleLeft = 0;
  soundStep = 0;
  soundAt = millis();
  soundStepAt = soundAt - 180;
  soundActive = app.settings.sound && app.settings.volume && tone != AlertTone::Silent;
  M5.Speaker.setVolume(app.settings.volume);
}
void buddySoundPreview() {
  M5.Speaker.stop();
  beginTone(app.settings.tone);
}
void buddyDemo() {
  Alert a;
  a.match = {Family::Axon, Confidence::Medium, "Synthetic demo; no device detected"};
  a.rssi = -45;
  strcpy(a.name, "Demo body-camera signal");
  buddyAlert(a, true);
}
void buddyDriveStart(bool fix) {
  uint32_t now = millis();
  lastTouch = lastFindAt = stoppedAt = noFixAt = now;
  burstFinds = 0;
  ++app.life.drives;
  feel(Mood::Curious, 4000);
  // Without a fix nothing gets logged yet, so say that plainly instead of cheering.
  say(fix ? line(Topic::DriveStart) : "Ready! Waiting for a good GPS fix.");
}
void buddyDriveEnd(uint32_t finds) {
  lastTouch = millis();
  bool record = app.life.bestDrive && finds > app.life.bestDrive;
  app.life.bestDrive = std::max(app.life.bestDrive, finds);
  char text[64];
  if (driveSummaryLine(text, sizeof(text), finds, record))
    say(text);
  else
    buddySay(Topic::DriveEmpty);
}
void buddyDiscovery(const uint8_t mac[6], const char *ssid, bool open) {
  if (!app.session.armed || !app.food.discover(mac))
    return;
  uint32_t now = millis();
  lastFindAt = lastTouch = now;
  // In a dense area new networks arrive constantly; cheer at most every few seconds so the
  // buddy is not stuck mid-bounce for a whole city block.
  if (!app.discoveryAt || uint32_t(now - app.discoveryAt) > 3000)
    app.discoveryAt = now;
  if (uint32_t(now - burstAt) > 30000) {
    burstAt = now;
    burstFinds = 0;
  }
  ++burstFinds;
  bool spoke = false;
  auto oldLevel = petLevel(app.settings.xp);
  earnDiscoveryXp(app.settings.xp);
  if (petLevel(app.settings.xp) != oldLevel) {
    app.levelUpAt = now;
    if (!app.alertOpen)
      buddySay(Topic::LevelUp);
    spoke = true;
    saveSettings();
  }
  bool snack = app.food.progress() == 0;
  bool stuffed = app.energy >= StuffedAtFuel;
  if (snack) {
    app.energy = std::min(100, int(app.energy) + 12);
    app.bond = std::min(100, int(app.bond) + 1); // Adventures together count as quality time.
    ++app.life.snacks;
  }
  // Never replace an active signal warning with snack chatter.
  if (app.alertOpen)
    return;
  uint32_t finds = app.food.discoveries();
  char text[64];
  // Snacks can come every few seconds downtown. The food meter always moves, but the buddy
  // only makes a fuss about one when it has been quiet for a while.
  bool fuss = snack && uint32_t(now - lastSnackFuss) > 20000;
  if (fuss) {
    lastSnackFuss = now;
    feel(Mood::Love, 3000);
    chirp(chirpSnack);
  }
  if (finds >= 25 && milestone(finds)) {
    findMilestoneLine(text, sizeof(text), finds, esp_random());
    say(text);
  } else if (spoke)
    return;
  else if (fuss)
    buddySay(stuffed ? Topic::Stuffed : Topic::Snack);
  else if (snack)
    return;
  else if (burstFinds == 12 && uint32_t(now - lastJackpot) > 180000) {
    lastJackpot = now;
    buddySay(Topic::Jackpot);
  } else if (uint32_t(now - app.messageAt) > 9000 && uint32_t(now - lastRemark) > 40000) {
    // Now and then, react to the actual network that was just logged.
    lastRemark = now;
    if (open && esp_random() % 2)
      buddySay(Topic::OpenNet);
    else if (!ssid || !*ssid) {
      if (esp_random() % 3 == 0)
        buddySay(Topic::HiddenNet);
    } else if (ssidLine(text, sizeof(text), ssid, esp_random()))
      say(text);
  }
}
void buddyAlert(const Alert &a, bool demo) {
  app.active = a;
  app.alertAt = millis();
  app.alertOpen = true;
  app.demo = demo;
  feel(Mood::Alarm, 0);
  jingleLeft = 0;
  lastTouch = lastMotion = app.alertAt;
  lastSiren = app.alertAt - 220;
  panicStep = 0;
  panicWait = 0;
  idleMotion.reset(millis(), 4000);
  if (!demo) {
    ++app.alerts;
    logAlert(a);
  }
  bool police = a.match.family == Family::Axon;
  unsigned line = demo ? 0 : esp_random() % 3;
  Family family = a.match.family;
  if (police && app.settings.spicy) // These three are tied to the recorded voice clips below.
    say(line == 0   ? "PIG DETECTED! ACT NATURAL!"
        : line == 1 ? "Oh man, the fuzz!"
                    : "Wee-woo! Tiny copilot panic!");
  else
    buddySay(police ? Topic::AlertBodyCam
             : family == Family::Flock || family == Family::Alpr || family == Family::Raven ||
                     family == Family::Camera
                 ? Topic::AlertCamera
             : family == Family::Tracker   ? Topic::AlertTracker
             : family == Family::Meta      ? Topic::AlertGlasses
             : family == Family::Flipper   ? Topic::AlertFlipper
             : family == Family::Drone     ? Topic::AlertDrone
             : family == Family::Watchlist ? Topic::AlertWatchlist
             : family == Family::Biscuit || family == Family::Pineapple ||
                     family == Family::Pwnagotchi || family == Family::Nyanbox ||
                     family == Family::Halehound
                 ? Topic::AlertGadget
                 : Topic::AlertOdd);
  soundActive = false;
  if (app.settings.sound && app.settings.volume && app.settings.tone != AlertTone::Silent) {
    M5.Speaker.stop();
    M5.Speaker.setVolume(app.settings.volume);
    if (app.settings.voice && app.settings.tone == AlertTone::Personality && police &&
        app.settings.spicy && line < 2) {
      const uint8_t *wav = line == 0 ? voicePig : voiceFuzz;
      size_t len = line == 0 ? sizeof(voicePig) : sizeof(voiceFuzz);
      M5.Speaker.playWav(wav, len, 1, 0, true);
    } else
      beginTone(app.settings.tone);
  }
}
void buddyDismiss() {
  app.alertOpen = false;
  app.demo = false;
  feel(Mood::Curious, 4000);
  M5.Speaker.stop();
  soundActive = false;
  idleMotion.reset(millis(), 4500);
  gazeTarget = 0;
  if (app.settings.motion)
    M5StackChan.Motion.move(0, 200, 100);
  buddySay(Topic::AllClear);
}
void buddyTick() {
  uint32_t now = millis();
  bool headPressed = false;
  for (auto intensity : M5StackChan.TouchSensor.getIntensities())
    headPressed |= intensity >= 2;
  if (headTouch.update(now, headPressed) && !app.alertOpen)
    buddyPet(false); // Sensor affection is visual; only deliberate screen pets can speak.
  if (app.alertOpen && uint32_t(now - app.alertAt) > 8500)
    buddyDismiss();
  bool fix = usableFix(app.fix);
  if (app.moving || !app.session.armed)
    stoppedAt = now;
  if (fix || !app.session.armed)
    noFixAt = now;
  Vitals vitals;
  vitals.energy = app.energy;
  vitals.bond = app.bond;
  vitals.onTheRoad = onTheRoad();
  vitals.quietMs = now - lastTouch;
  vitals.recentFinds = uint32_t(now - burstAt) < 45000 ? burstFinds : 0;
  app.feeling = feelingFor(vitals);
  if (!app.alertOpen && uint32_t(now - moodAt) >= moodHold)
    app.mood = restingMood(app.feeling);
  if (uint32_t(now - lastNeed) > 60000) {
    lastNeed = now;
    decayNeeds(app.energy, app.bond, app.feeling, onTheRoad(), ++needMinute);
    if (clockNow(now))
      app.life.lastSeen = clockNow(now);
  }
  if (fix) { // GPS time is the source of truth; also teach it to the board clock once.
    clockLearn(utcSeconds(app.fix.utc), now);
    if (!rtcSynced && M5.Rtc.isEnabled()) {
      rtcSynced = true;
      int y, mo, d, h, mi, sec;
      if (sscanf(app.fix.utc, "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &sec) == 6)
        M5.Rtc.setDateTime(
            {{int16_t(y), int8_t(mo), int8_t(d)}, {int8_t(h), int8_t(mi), int8_t(sec)}});
    }
  }
  bool missedYou = catchUpOnTimeApart(now);
  if (!greeted) { // First words after boot depend on how the pet was left.
    greeted = true;
    if (missedYou || *app.storageError) {
    } else if (app.life.boots <= 1 && !app.life.pets)
      say(FirstHello);
    else
      buddySay(app.energy < HungryBelow ? Topic::HelloHungry
               : app.bond < LonelyBelow ? Topic::HelloLonely
                                        : Topic::Hello);
  }
  // Unprompted chatter: irregular gaps, and never on top of a message someone is still reading.
  if (!app.alertOpen && uint32_t(now - lastChatter) > chatterGap &&
      uint32_t(now - app.messageAt) > 12000) {
    lastChatter = now;
    chatterGap = chatterGapMs(app.feeling, onTheRoad(), esp_random());
    RoadView road;
    road.recording = app.session.armed;
    road.moving = app.moving;
    road.fix = fix;
    road.sinceFindMs = now - lastFindAt;
    road.stoppedMs = now - stoppedAt;
    road.noFixMs = now - noFixAt;
    if (app.mood == Mood::Happy)
      feel(Mood::Curious, 5000);
    buddySay(chatterTopic(app.feeling, road, esp_random()));
  }
  if (jingleLeft && uint32_t(now - jingleAt) >= jingleWait) {
    if (!app.settings.sound || !app.settings.volume || app.alertOpen || soundActive)
      jingleLeft = 0;
    else {
      M5.Speaker.setVolume(app.settings.volume);
      M5.Speaker.tone(jingle->hz, jingle->ms);
      jingleAt = now;
      jingleWait = jingle->ms + 15;
      ++jingle;
      --jingleLeft;
    }
  }
  if (soundActive && (!app.settings.sound || !app.settings.volume)) {
    soundActive = false;
    M5.Speaker.stop();
  }
  if (soundActive && !M5.Speaker.isPlaying(0) && uint32_t(now - soundStepAt) >= 180) {
    soundStepAt = now;
    static const unsigned arcade[] = {784, 1047, 1319, 1568, 1319, 1047};
    unsigned frequency = playingTone == AlertTone::Siren   ? (soundStep % 2 ? 1300 : 600)
                         : playingTone == AlertTone::Chime ? (soundStep % 2 ? 1568 : 1047)
                                                           : arcade[soundStep % 6];
    unsigned steps = playingTone == AlertTone::Siren ? 16 : playingTone == AlertTone::Chime ? 2 : 6;
    if (soundStep++ < steps)
      M5.Speaker.tone(frequency, 150, 1, true);
    else
      soundActive = false;
  }
  if (app.alertOpen && app.settings.sound && app.settings.volume && app.settings.spicy &&
      app.settings.tone == AlertTone::Personality && app.active.match.family == Family::Axon &&
      !M5.Speaker.isPlaying(0) && uint32_t(now - lastSiren) > 220 &&
      uint32_t(now - app.alertAt) < 6500) {
    lastSiren = now;
    M5.Speaker.tone((now / 220) % 2 ? 650 : 1150, 180, 1, true);
  }
  if (app.alertOpen && app.settings.motion && uint32_t(now - lastMotion) >= panicWait) {
    lastMotion = now;
    auto cue = panicMotion(panicStep++);
    panicWait = cue.holdMs;
    if (app.settings.motionStyle == 1 || !app.settings.spicy ||
        app.active.match.family != Family::Axon) {
      cue.yaw /= 2;
      cue.pitch = 200 + (cue.pitch - 200) / 2;
      cue.speed = 260;
      panicWait += 300;
    }
    // BSP units are tenths of a degree. Stay well inside K151 travel limits.
    M5StackChan.Motion.move(cue.yaw, cue.pitch, cue.speed);
  } else if (!app.alertOpen) {
    MotionCue cue;
    if (idleMotion.tick(now, esp_random(), restingMood(app.feeling) != Mood::Happy,
                        app.settings.avatar == Avatar::Custom ? app.settings.customBase
                                                              : unsigned(app.settings.avatar),
                        app.session.armed, cue)) {
      if (app.settings.motionStyle == 1) {
        cue.yaw /= 2;
        cue.pitch = 200 + (cue.pitch - 200) / 2;
        cue.speed = 90;
      }
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
