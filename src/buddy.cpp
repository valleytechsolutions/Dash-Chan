#include "app.h"
#include "idle_motion.h"
#include "petting.h"
#include "voice.h"
#include <M5StackChan.h>
#include <algorithm>
namespace dash {
static uint32_t lastMotion = 0, lastNeed = 0, lastChatter = 0, lastSave = 0, lastSiren = 0;
static Cooldown petTimer;
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
static ReconBanter reconBanter;
static ChatterPace chatterPace;
static uint32_t wokenAt = 0;
static bool woken = false; // buddyPoke() has just woken the buddy and said so.
static uint32_t reconAt = 0, ignoreSaidAt = 0;
static unsigned alertFlinch = 100; // Percent of the full panic gesture for the open alert.
static PetCombo petCombo;
// lastTouch is the "someone is paying attention to me" clock that drives boredom and naps.
static uint32_t lastTouch = 0, chatterGap = 20000, moodAt = 0, moodHold = 0, burstAt = 0,
                lastFindAt = 0, lastJackpot = 0, lastRemark = 0, stoppedAt = 0, noFixAt = 0,
                needMinute = 0, lastSnackFuss = 0;
static unsigned burstFinds = 0;
static bool greeted = false, burstCheered = false;
static Milestone petMilestone, findMilestone;
static DriveBest driveBest;
static NapSettle napSettle;
static GpsNews gpsNews;
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
// ---- Speech ---------------------------------------------------------------------------------
// Clips play on speaker channel 0. Alert tones use channel 1 and pet chirps take a free channel
// from the top, so "channel 0 is busy" always means "the buddy is talking".
static SpeechGate speechGate;
static const ClipData *speechQueued = nullptr; // Waiting for the attention chirp to finish.
static SpeechKind speechKind = SpeechKind::Pet;
static unsigned speechStep = 0;
static uint32_t speechAt = 0, speechWait = 0;
static const Note chirpAttention[] = {{1319, 55}, {1976, 85}};
constexpr unsigned AttentionNotes = sizeof(chirpAttention) / sizeof(*chirpAttention);
static SpeechSettings speechSettings() {
  SpeechSettings s;
  s.sound = app.settings.sound;
  s.volume = app.settings.volume;
  s.mode = app.settings.voiceMode;
  s.silentTone = app.settings.tone == AlertTone::Silent;
  return s;
}
static bool speaking() {
  return speechQueued || M5.Speaker.isPlaying(0);
}
static void speechStop() {
  speechQueued = nullptr;
  M5.Speaker.stop(0);
}
static void speechTick(uint32_t now) {
  if (speaking() && !speechEnabled(speechKind, speechSettings()))
    speechStop(); // Muted or switched off mid-sentence.
  if (!speechQueued || uint32_t(now - speechAt) < speechWait)
    return;
  speechAt = now;
  if (speechStep < AttentionNotes) {
    // The chirp says "listen". It stands in for the long alert tone, so the two never overlap.
    const Note &note = chirpAttention[speechStep++];
    M5.Speaker.tone(note.hz, note.ms, 1, true);
    speechWait = note.ms + (speechStep == AttentionNotes ? 60 : 15);
  } else {
    M5.Speaker.playWav(speechQueued->wav, speechQueued->size, 1, 0, true);
    speechQueued = nullptr;
  }
}
bool voiceSpeak(Clip clip, SpeechKind kind) {
  uint32_t now = millis();
  if (unsigned(clip) >= ClipCount || !VoiceClips[unsigned(clip)].wav ||
      !speechEnabled(kind, speechSettings()) ||
      !speechGate.ready(kind, clip, now, app.alertOpen || soundActive))
    return false;
  speechGate.mark(kind, clip, now);
  jingleLeft = 0;
  soundActive = false;
  M5.Speaker.stop();
  M5.Speaker.setVolume(app.settings.volume);
  speechQueued = &VoiceClips[unsigned(clip)];
  speechKind = kind;
  // Only detections get the attention chirp; pet phrases just start talking.
  speechStep = kind == SpeechKind::Alert ? 0 : AttentionNotes;
  speechAt = now;
  speechWait = 0;
  speechTick(now);
  return true;
}
// A pet or system phrase. When it is spoken the bubble shows those words, so the screen and the
// voice agree; when it is not (Voice below ALL, or too soon) the caller picks a line as before.
static bool petSpeak(Clip clip) {
  if (!voiceSpeak(clip, SpeechKind::Pet))
    return false;
  say(clipCaption(clip));
  return true;
}
// Each press speaks the next clip, so the whole set can be heard from the Sound & Voice page.
const char *buddyVoiceTest() {
  static unsigned next = unsigned(Clip::Hello);
  // Gentle banter never says the spicy lines, not even as a sample.
  while (!app.settings.spicy && (Clip(next) == Clip::SpicyPig || Clip(next) == Clip::SpicyFuzz))
    next = (next + 1) % ClipCount;
  Clip clip = Clip(next);
  if (!voiceSpeak(clip, SpeechKind::Test))
    return nullptr;
  next = (next + 1) % ClipCount;
  return clipCaption(clip);
}
template <size_t N> static void chirp(const Note (&notes)[N]) {
  if (!app.settings.sound || !app.settings.volume || soundActive || app.alertOpen || speaking() ||
      M5.Speaker.isPlaying())
    return;
  jingle = notes;
  jingleLeft = N;
  jingleAt = millis();
  jingleWait = 0;
}
static unsigned who() {
  return speaker(app.settings.avatar == Avatar::Custom, app.settings.customPersona,
                 avatarBase(app.settings.avatar, app.settings.customBase));
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
// The face while nothing else is going on. Something in range makes the buddy curious: a tag or
// a beacon only when it was otherwise content, a camera or a hacker gadget even mid-sulk. A
// sleepy buddy stays sleepy, and Alarm is kept for real alerts.
static Mood watchfulMood(Feeling f) {
  Mood mood = restingMood(f);
  unsigned level = watchLevel(reconCounts());
  if (mood == Mood::Sleepy || !level)
    return mood;
  return level == 2 || mood == Mood::Happy ? Mood::Curious : mood;
}
// Any touch or tap counts as attention. A sleeping pet wakes up, a bit groggy.
void buddyPoke() {
  lastTouch = millis();
  // buddyTick re-evaluates the feeling on the next pass.
  if (app.feeling == Feeling::Napping && !app.alertOpen) {
    if (!justWoken(woken, wokenAt, lastTouch) && !petSpeak(Clip::GoodMorning))
      buddySay(Topic::Wake);
    woken = true;
    wokenAt = lastTouch;
  }
}
// Someone leaned in (proximity sensor). A hello, not a pet: it wakes the buddy and warms it a
// little without counting toward pets or combos.
void buddyGreet() {
  bool asleep = app.feeling == Feeling::Napping;
  lastTouch = millis();
  app.bond = std::min(100, int(app.bond) + 1);
  feel(Mood::Love, 2500);
  if (!petSpeak(asleep ? Clip::GoodMorning : Clip::Hello))
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
  uint32_t hours = awayHoursAtBoot(app.life.lastSeen, seconds, now);
  app.life.lastSeen = seconds;
  if (!hours)
    return false;
  applyAway(app.energy, app.bond, hours);
  char text[64];
  // "Missed you" is a boot greeting. When the clock is learned later (first GPS fix mid-drive)
  // the catch-up stays silent, and it never talks over an alert or a startup failure.
  if (greeted || app.alertOpen || !app.radioReady || *app.storageError ||
      !awayLine(text, sizeof(text), hours, esp_random()))
    return false;
  if (!petSpeak(Clip::MissedYou))
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
  // The board clock is only believed once GPS has set it (remembered in settings) and its
  // backup supply has held; otherwise the time stays unknown until the first fix.
  if (app.life.clockSet && M5.Rtc.isEnabled()) {
    auto t = M5.Rtc.getDateTime();
    char stamp[24];
    snprintf(stamp, sizeof(stamp), "%04d-%02d-%02d %02d:%02d:%02d", t.date.year, t.date.month,
             t.date.date, t.time.hours, t.time.minutes, t.time.seconds);
    uint32_t seconds = utcSeconds(stamp);
    if (boardClockTrusted(true, M5.Rtc.getVoltLow(), seconds, app.life.lastSeen))
      clockLearn(seconds, millis());
  }
}
void buddyPet(bool allowVoice) {
  uint32_t now = millis();
  // An alert owns the face, the voice and the head until it is dismissed.
  if (app.alertOpen || !petTimer.ready(now, 800))
    return;
  petTimer.mark(now);
  Feeling before = app.feeling;
  lastTouch = now;
  unsigned streak = petCombo.pet(now);
  ++app.pets;
  ++app.life.pets;
  petMilestone.count(app.life.pets);
  app.bond = std::min(100, int(app.bond) + int(petBondGain(streak)));
  Topic topic = petTopic(streak, before, app.moving, app.bond, esp_random());
  bool enough = topic == Topic::PetEnough;
  feel(enough ? Mood::Curious : Mood::Love, 3000);
  idleMotion.reset(now, 3500);
  gazeTarget = 0;
  if (app.settings.motion) {
    // On the road the turn stays inside the drive-safe box (idle_motion.h).
    MotionCue cue{enough ? (esp_random() & 1 ? 140 : -140) : 0, 230, 110, 0};
    cue = flinchCue(cue, 100, onTheRoad());
    M5StackChan.Motion.move(cue.yaw, cue.pitch, cue.speed);
  }
  // The tap that woke the buddy has already had its good morning from buddyPoke(); the love and
  // the nuzzle above still count, but a second wake line would replace the words being spoken.
  if (topic == Topic::Wake && justWoken(woken, wokenAt, now)) {
    if (allowVoice)
      chirp(chirpYawn); // Stays silent while the good morning clip is playing.
    return;
  }
  const char *text = line(topic);
  char counted[64];
  if (petMilestone.due() && petMilestoneFits(topic)) {
    petMilestoneLine(counted, sizeof(counted), petMilestone.due(), esp_random());
    text = counted;
    petMilestone.clear();
  }
  say(text);
  // Sensor affection is visual; only deliberate screen pets make noise.
  if (!allowVoice || !app.settings.sound || !app.settings.volume)
    return;
  M5.Speaker.setVolume(app.settings.volume);
  // A clip only plays when the pet actually says that line: a counted milestone keeps its own
  // text and gets the usual chirp.
  bool plain = text != counted;
  if (text == LoveYourFace && voiceSpeak(Clip::Love, SpeechKind::Pet)) {
  } else if (enough)
    chirp(chirpEnough);
  else if (topic == Topic::Wake && plain && petSpeak(Clip::GoodMorning)) {
  } else if (topic == Topic::Wake)
    chirp(chirpYawn);
  else if (topic == Topic::PetGiggle && plain && petSpeak(Clip::Tickles)) {
  } else if (topic == Topic::PetGiggle)
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
  speechQueued = nullptr;
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
  buddyAlert(a, true);
}
// Opening the Recon page in Voice ALL names the top thing in range, with one of the family
// clips that already exist. The pet-phrase limits apply, so it cannot chatter.
void buddyReconPeek(Family top) {
  // It is the alert clip for that family, so the Silent alert tone keeps it quiet as well.
  if (top == Family::None || app.alertOpen || app.feeling == Feeling::Napping ||
      app.settings.tone == AlertTone::Silent)
    return;
  petSpeak(clipFor(top));
}
void buddyReconIgnored() {
  uint32_t now = millis();
  reconBanter.forget(); // Before the early return: an ignored follower has not "gone".
  if (app.alertOpen || (ignoreSaidAt && uint32_t(now - ignoreSaidAt) < 5000))
    return;
  ignoreSaidAt = now | 1;
  buddySay(Topic::ReconIgnore);
}
void buddyDriveStart(bool fix) {
  uint32_t now = millis();
  lastTouch = lastFindAt = stoppedAt = noFixAt = now;
  burstFinds = 0;
  burstCheered = false;
  findMilestone.clear();
  ++app.life.drives;
  driveBest.start(app.life.bestDrive);
  feel(Mood::Curious, 4000);
  // Without a fix nothing gets logged yet, so say that plainly instead of cheering.
  if (!fix)
    say("Ready! Waiting for a good GPS fix.");
  else if (!petSpeak(Clip::DriveStart))
    say(line(Topic::DriveStart));
  // A dashboard drive often ends with the ignition, not the Stop button: count it now.
  lastSave = now;
  saveSettings();
}
void buddyDriveEnd(uint32_t finds) {
  lastTouch = millis();
  bool record = driveBest.record(finds);
  DriveBest::found(app.life.bestDrive, finds);
  char text[64];
  if (voiceSpeak(Clip::DriveSaved, SpeechKind::Pet)) {
    driveSavedCaption(text, sizeof(text), finds, record);
    say(text);
  } else if (driveSummaryLine(text, sizeof(text), finds, record))
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
    burstCheered = false;
  }
  ++burstFinds;
  uint32_t finds = app.food.discoveries();
  DriveBest::found(app.life.bestDrive, finds);
  findMilestone.count(finds, 25);
  bool spoke = false;
  auto oldLevel = petLevel(app.settings.xp);
  earnDiscoveryXp(app.settings.xp);
  if (petLevel(app.settings.xp) != oldLevel) {
    app.levelUpAt = now;
    if (!app.alertOpen && !petSpeak(Clip::LevelUp))
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
  char text[64];
  // Snacks can come every few seconds downtown. The food meter always moves, but the buddy
  // only makes a fuss about one when it has been quiet for a while.
  bool fuss = snack && uint32_t(now - lastSnackFuss) > 20000;
  if (fuss) {
    lastSnackFuss = now;
    feel(Mood::Love, 3000);
    chirp(chirpSnack);
  }
  // The level-up line (and its clip) keeps the bubble; a milestone that landed on the same
  // network stays due and is said with the next one.
  if (spoke)
    return;
  if (findMilestone.due()) {
    findMilestoneLine(text, sizeof(text), findMilestone.due(), esp_random());
    say(text);
    findMilestone.clear();
  } else if (fuss) {
    if (stuffed || !petSpeak(Clip::NewWifi))
      buddySay(stuffed ? Topic::Stuffed : Topic::Snack);
  } else if (snack)
    return;
  else if (jackpotDue(burstFinds, burstCheered, now - lastJackpot)) {
    lastJackpot = now;
    burstCheered = true;
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
  if (!demo)
    logAlert(a);
  bool police = a.match.family == Family::Axon;
  unsigned line = demo ? 0 : esp_random() % 3;
  Family family = a.match.family;
  bool spicyCop = police && app.settings.spicy;
  bool audible =
      app.settings.sound && app.settings.volume && app.settings.tone != AlertTone::Silent;
  AlertTwist twist = a.kind == AlertFollowing ? AlertTwist::Following
                     : a.kind == AlertFlood   ? AlertTwist::Flood
                                              : AlertTwist::None;
  if (twist != AlertTwist::None)
    spicyCop = false; // The behaviour is the news, not the brand.
  Clip clip = alertClip(family, spicyCop ? int(line) : -1, twist);
  // A follow warning is the top tier whatever the tag is; a flood is only as urgent as a tag.
  alertFlinch = flinchPercent(twist == AlertTwist::Following ? 0 : unsigned(tierOf(family)),
                              demo ? -50 : a.rssi);
  soundActive = false;
  // Whatever was being said gives way, a pet phrase or an earlier alert's clip, even when this
  // alert is silent: the bubble and the card now belong to the new alert.
  speechStop();
  if (audible)
    M5.Speaker.stop();
  if (voiceSpeak(clip, SpeechKind::Alert)) {
    // Spoken: the screen shows the same words, and the chirp stands in for the long tone.
    say(clipCaption(clip));
    return;
  }
  char text[64];
  if (twist == AlertTwist::Flood)
    say(FloodLine);
  else if (twist == AlertTwist::Following) {
    followLine(text, sizeof(text), a.followMs / 1000, a.followMeters);
    say(text);
  } else if (spicyCop) // The first two are the recorded lines, shown as text when not spoken.
    say(line == 0   ? clipCaption(Clip::SpicyPig)
        : line == 1 ? clipCaption(Clip::SpicyFuzz)
                    : "Wee-woo! Tiny copilot panic!");
  else
    buddySay(police ? Topic::AlertBodyCam
             : family == Family::Flock || family == Family::Alpr || family == Family::Raven ||
                     family == Family::Camera || family == Family::Ring
                 ? Topic::AlertCamera
             : family == Family::Tracker || family == Family::SamsungTag ||
                     family == Family::GoogleTag || family == Family::Tile
                 ? Topic::AlertTracker
             : family == Family::Meta      ? Topic::AlertGlasses
             : family == Family::Flipper   ? Topic::AlertFlipper
             : family == Family::Drone     ? Topic::AlertDrone
             : family == Family::Watchlist ? Topic::AlertWatchlist
             : family == Family::Biscuit || family == Family::Pineapple ||
                     family == Family::Pwnagotchi || family == Family::Nyanbox ||
                     family == Family::Halehound
                 ? Topic::AlertGadget
                 : Topic::AlertOdd);
  if (audible) {
    M5.Speaker.setVolume(app.settings.volume);
    beginTone(app.settings.tone);
  }
}
void buddyDismiss(bool byTimeout) {
  // Read what the alert was about before anything is cleared.
  Topic closing =
      dismissTopic(byTimeout, app.active.kind == AlertFollowing, reconCounts().followingLive != 0);
  app.alertOpen = false;
  app.demo = false;
  feel(Mood::Curious, 4000);
  speechQueued = nullptr;
  M5.Speaker.stop();
  soundActive = false;
  idleMotion.reset(millis(), 4500);
  gazeTarget = 0;
  if (app.settings.motion)
    M5StackChan.Motion.move(0, 200, 100);
  // "All clear" is only said when that is what happened (see dismissTopic); otherwise a neutral
  // line, in text only. A silent alert ends silently too.
  if (closing != Topic::AllClear || app.settings.tone == AlertTone::Silent ||
      !petSpeak(Clip::AllClear))
    buddySay(closing);
}
void buddyTick() {
  uint32_t now = millis();
  bool headPressed = false;
  for (auto intensity : M5StackChan.TouchSensor.getIntensities())
    headPressed |= intensity >= 2;
  if (headTouch.update(now, headPressed) && !app.alertOpen)
    buddyPet(false); // Sensor affection is visual; only deliberate screen pets can speak.
  if (app.alertOpen && uint32_t(now - app.alertAt) > 8500)
    buddyDismiss(true);
  bool fix = usableFix(app.fix);
  // "Stopped" means known to be stationary, so it only accumulates while there is a fix.
  if (app.moving || !fix || !app.session.armed)
    stoppedAt = now;
  if (fix || !app.session.armed)
    noFixAt = now;
  Clip news = gpsNews.update(app.session.armed, fix, now);
  if (news != Clip::Count && petSpeak(news))
    gpsNews.told(news);
  Vitals vitals;
  vitals.energy = app.energy;
  vitals.bond = app.bond;
  vitals.onTheRoad = onTheRoad();
  lastTouch = attentionAt(lastTouch, now, vitals.onTheRoad);
  vitals.quietMs = now - lastTouch;
  vitals.recentFinds = uint32_t(now - burstAt) < 45000 ? burstFinds : 0;
  app.feeling = feelingFor(vitals);
  if (!app.alertOpen && uint32_t(now - moodAt) >= moodHold)
    app.mood = watchfulMood(app.feeling);
  // Recon small talk, from the counts the radio refreshes twice a second.
  if (uint32_t(now - reconAt) >= 500) {
    reconAt = now;
    const ReconCounts &counts = reconCounts();
    ReconPulse pulse;
    for (unsigned t = 0; t < TierCount; ++t)
      pulse.live += counts.watchTier[t];
    pulse.surveillance = counts.watchTier[unsigned(Tier::Surveillance)];
    pulse.following = counts.followingLive;
    bool busy = app.alertOpen || app.feeling == Feeling::Napping || !greeted ||
                uint32_t(now - app.messageAt) < 8000;
    Topic remark = reconBanter.update(pulse, now, busy);
    if (remark != Topic::Count) {
      lastChatter = now; // Counts as chatter, so idle talk does not follow straight after.
      buddySay(remark);
    }
  }
  if (uint32_t(now - lastNeed) > 60000) {
    lastNeed = now;
    decayNeeds(app.energy, app.bond, app.feeling, onTheRoad(), ++needMinute);
    if (clockNow(now))
      app.life.lastSeen = clockNow(now);
  }
  if (fix) { // GPS time is the source of truth; also teach it to the board clock once.
    clockLearn(utcSeconds(app.fix.utc), now);
    if (!rtcSynced && M5.Rtc.isEnabled()) {
      int y, mo, d, h, mi, sec;
      if (sscanf(app.fix.utc, "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &sec) == 6) {
        M5.Rtc.setDateTime(
            {{int16_t(y), int8_t(mo), int8_t(d)}, {int8_t(h), int8_t(mi), int8_t(sec)}});
        rtcSynced = true;
        if (!app.life.clockSet) { // Remember that the board clock now holds real time.
          app.life.clockSet = true;
          saveSettings();
        }
      }
    }
  }
  bool missedYou = catchUpOnTimeApart(now);
  if (!greeted) { // First words after boot depend on how the pet was left.
    greeted = true;
    // A radio or storage failure reported during setup must stay on screen.
    if (missedYou || *app.storageError || !app.radioReady) {
    } else if (app.life.boots <= 1 && !app.life.pets)
      say(FirstHello);
    else {
      bool hungry = app.energy < HungryBelow, lonely = app.bond < LonelyBelow;
      if (!petSpeak(hungry ? Clip::Hungry : lonely ? Clip::MissedYou : Clip::Hello))
        buddySay(hungry ? Topic::HelloHungry : lonely ? Topic::HelloLonely : Topic::Hello);
    }
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
    Topic topic = chatterPace.pace(chatterTopic(app.feeling, road, esp_random()), now);
    Clip clip = topic == Topic::IdleHungry || topic == Topic::RoadHungry ? Clip::Hungry
                : topic == Topic::IdleDrowsy                             ? Clip::Sleepy
                                                                         : Clip::Count;
    if (clip == Clip::Count || !petSpeak(clip))
      buddySay(topic);
  }
  speechTick(now);
  if (jingleLeft && uint32_t(now - jingleAt) >= jingleWait) {
    if (!app.settings.sound || !app.settings.volume || app.alertOpen || soundActive || speaking())
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
  // The wee-woo waits for the alert sting: both use speaker channel 1 and would cut each
  // other off.
  if (!soundActive && app.alertOpen && app.settings.sound && app.settings.volume &&
      app.settings.spicy && app.settings.tone == AlertTone::Personality &&
      app.active.match.family == Family::Axon && !speaking() && uint32_t(now - lastSiren) > 220 &&
      uint32_t(now - app.alertAt) < 6500) {
    lastSiren = now;
    M5.Speaker.tone((now / 220) % 2 ? 650 : 1150, 180, 1, true);
  }
  if (app.alertOpen) {
    if (app.settings.motion && uint32_t(now - lastMotion) >= panicWait) {
      lastMotion = now;
      auto cue = panicMotion(panicStep++);
      panicWait = cue.holdMs;
      // The spicy body-camera panic keeps its full wee-woo thrash; everything else flinches in
      // proportion to its tier and signal strength, and the Gentle style halves it again.
      bool wild = app.settings.motionStyle != 1 && app.settings.spicy &&
                  app.active.kind == AlertDevice && app.active.match.family == Family::Axon;
      unsigned percent = wild ? 100 : alertFlinch;
      if (app.settings.motionStyle == 1)
        percent /= 2;
      if (!wild) {
        cue.speed = 260;
        panicWait += 300;
      }
      // On the road the gesture is held inside the drive-safe box (idle_motion.h).
      cue = flinchCue(cue, percent, onTheRoad());
      // BSP units are tenths of a degree. Stay well inside K151 travel limits.
      M5StackChan.Motion.move(cue.yaw, cue.pitch, cue.speed);
    }
  } else if (app.feeling == Feeling::Napping) {
    // Asleep: lower the head once, then leave the servos alone so auto torque release can
    // rest them until someone wakes the buddy.
    if (napSettle.update(true)) {
      gazeTarget = 0;
      if (app.settings.motion)
        M5StackChan.Motion.move(0, 190, 75);
    }
  } else {
    if (napSettle.resting())
      idleMotion.reset(now, 2500);
    napSettle.update(false);
    MotionCue cue;
    if (idleMotion.tick(now, esp_random(), restingMood(app.feeling) != Mood::Happy,
                        app.settings.avatar == Avatar::Custom ? app.settings.customBase
                                                              : unsigned(app.settings.avatar),
                        onTheRoad(), cue)) {
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
  // While recording, save often: snacks, bond and the best-drive count must survive a power cut.
  if (uint32_t(now - lastSave) > (app.session.armed ? 60000u : 300000u)) {
    lastSave = now;
    saveSettings();
  }
}
} // namespace dash
