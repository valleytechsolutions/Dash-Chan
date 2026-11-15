// SPDX-License-Identifier: GPL-3.0-or-later
// Spoken clips: which ones exist, when one may play, and what the screen shows meanwhile.
// Pure logic with no hardware calls, so tests/voice_tests.h can exercise it on a PC. The samples
// themselves live in the generated src/voice_data.cpp.
//
// TO ADD A CLIP: add a row to tools/voice_phrases.tsv and the matching entry, at the same
// position, to Clip and ClipInfos below. Then run tools/generate_voice.py and commit the new WAV
// together with src/voice_data.cpp. The host tests check that all four stay in step.
#pragma once
#include "core.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
namespace dash {
// ---- Setting --------------------------------------------------------------------------------
// Saved as a number, so new modes go at the end.
enum class VoiceMode : uint8_t { Off, Alerts, All };
inline const char *voiceModeName(VoiceMode m) {
  static const char *const names[] = {"OFF", "ALERTS", "ALL"};
  return names[unsigned(m) % 3];
}
inline VoiceMode nextVoiceMode(VoiceMode m) {
  return VoiceMode((unsigned(m) + 1) % 3);
}
// Older firmware saved one on/off switch that covered every clip, so "on" becomes ALL.
inline VoiceMode savedVoiceMode(bool hasMode, uint8_t mode, bool oldSwitch) {
  if (hasMode)
    return mode <= uint8_t(VoiceMode::All) ? VoiceMode(mode) : VoiceMode::Off;
  return oldSwitch ? VoiceMode::All : VoiceMode::Off;
}

// ---- Clips ----------------------------------------------------------------------------------
enum class Clip : uint8_t {
  Generic,
  Axon,
  Flock,
  Alpr,
  Meta,
  Tracker,
  Flipper,
  Biscuit,
  Pineapple,
  Pwnagotchi,
  Nyanbox,
  Halehound,
  Skimmer,
  Raven,
  Drone,
  Camera,
  Beacon,
  Deauth,
  EvilTwin,
  BleSpam,
  Watchlist,
  SamsungTag,
  GoogleTag,
  Tile,
  Ring,
  Following,
  Flood,
  AllClear,
  DriveStart,
  DriveSaved,
  GpsLocked,
  GpsLost,
  NewWifi,
  MissedYou,
  Hungry,
  Tickles,
  Sleepy,
  LevelUp,
  NoSd,
  Hello,
  GoodMorning,
  SpicyPig,
  SpicyFuzz,
  Love,
  Count
};
constexpr unsigned ClipCount = unsigned(Clip::Count);
struct ClipInfo {
  const char *id;      // File name in assets/voice.
  const char *caption; // Shown on screen while the clip plays.
};
inline constexpr ClipInfo ClipInfos[ClipCount] = {
    {"generic", "Something sketchy!"},
    {"axon", "Body camera spotted!"},
    {"flock", "Flock camera spotted!"},
    {"alpr", "Plate camera spotted!"},
    {"meta", "Camera glasses spotted!"},
    {"tracker", "Tracker spotted!"},
    {"flipper", "Flipper Zero spotted!"},
    {"biscuit", "Biscuit device spotted!"},
    {"pineapple", "Pineapple spotted!"},
    {"pwnagotchi", "Pwnagotchi spotted!"},
    {"nyanbox", "Nyanbox spotted!"},
    {"halehound", "HaleHound spotted!"},
    {"skimmer", "Careful! Card skimmer!"},
    {"raven", "Raven spotted!"},
    {"drone", "Drone overhead!"},
    {"camera", "Camera spotted!"},
    {"beacon", "Strange beacon!"},
    {"deauth", "Disconnect burst!"},
    {"eviltwin", "Evil twin Wi-Fi!"},
    {"blespam", "Bluetooth spam!"},
    {"watchlist", "Watch list hit!"},
    {"samsungtag", "SmartTag spotted!"},
    {"googletag", "Google tag spotted!"},
    {"tile", "Tile tracker spotted!"},
    {"ring", "Ring camera spotted!"},
    {"following", "We're being followed!"},
    {"flood", "Signal flood!"},
    {"all_clear", "All clear!"},
    {"drive_start", "Let's go for a drive!"},
    {"drive_saved", "Drive saved!"},
    {"gps_locked", "GPS locked!"},
    {"gps_lost", "GPS lost!"},
    {"new_wifi", "Found new Wi-Fi!"},
    {"missed_you", "I missed you!"},
    {"hungry", "I'm hungry!"},
    {"tickles", "That really tickles!"},
    {"sleepy", "So sleepy."},
    {"level_up", "Level up! Nice!"},
    {"no_sd", "No SD card!"},
    {"hello", "Hello there!"},
    {"good_morning", "Good morning!"},
    {"spicy_pig", "PIG DETECTED! ACT NATURAL!"},
    {"spicy_fuzz", "Oh man, the fuzz!"},
    {"love", "i Love Your Face"},
};
inline const char *clipCaption(Clip c) {
  return unsigned(c) < ClipCount ? ClipInfos[unsigned(c)].caption : "";
}
// A whole WAV file (mono 16 kHz 8-bit unsigned PCM, 44-byte header) in flash.
struct ClipData {
  const uint8_t *wav;
  size_t size;
};
extern const ClipData VoiceClips[ClipCount]; // src/voice_data.cpp

// ---- Which clip an alert speaks -------------------------------------------------------------
// No default case: the compiler points here when a family is added without a clip.
inline Clip clipFor(Family f) {
  switch (f) {
  case Family::None:
    break;
  case Family::Axon:
    return Clip::Axon;
  case Family::Flock:
    return Clip::Flock;
  case Family::Alpr:
    return Clip::Alpr;
  case Family::Meta:
    return Clip::Meta;
  case Family::Tracker:
    return Clip::Tracker;
  case Family::Flipper:
    return Clip::Flipper;
  case Family::Biscuit:
    return Clip::Biscuit;
  case Family::Pineapple:
    return Clip::Pineapple;
  case Family::Pwnagotchi:
    return Clip::Pwnagotchi;
  case Family::Nyanbox:
    return Clip::Nyanbox;
  case Family::Halehound:
    return Clip::Halehound;
  case Family::Skimmer:
    return Clip::Skimmer;
  case Family::Raven:
    return Clip::Raven;
  case Family::Drone:
    return Clip::Drone;
  case Family::Camera:
    return Clip::Camera;
  case Family::Beacon:
    return Clip::Beacon;
  case Family::Deauth:
    return Clip::Deauth;
  case Family::EvilTwin:
    return Clip::EvilTwin;
  case Family::BleSpam:
    return Clip::BleSpam;
  case Family::Watchlist:
    return Clip::Watchlist;
  case Family::SamsungTag:
    return Clip::SamsungTag;
  case Family::GoogleTag:
    return Clip::GoogleTag;
  case Family::Tile:
    return Clip::Tile;
  case Family::Ring:
    return Clip::Ring;
  }
  return Clip::Generic;
}
inline bool trackerFamily(Family f) {
  return f == Family::Tracker || f == Family::SamsungTag || f == Family::GoogleTag ||
         f == Family::Tile;
}
// What a detector saw the device doing, when that matters more than what brand it is.
enum class AlertTwist : uint8_t { None, Following, Flood };
// Spicy banter has two recorded body-camera lines; `spicyLine` 0 and 1 pick them and anything
// else (or Gentle banter, passed as -1) names the family plainly.
inline Clip alertClip(Family f, int spicyLine = -1, AlertTwist twist = AlertTwist::None) {
  if (twist == AlertTwist::Flood)
    return Clip::Flood;
  if (twist == AlertTwist::Following && trackerFamily(f))
    return Clip::Following;
  if (f == Family::Axon && spicyLine == 0)
    return Clip::SpicyPig;
  if (f == Family::Axon && spicyLine == 1)
    return Clip::SpicyFuzz;
  return clipFor(f);
}

// ---- When a clip may play -------------------------------------------------------------------
enum class SpeechKind : uint8_t {
  Alert, // Detection announcement.
  Pet,   // Pet and system phrases: greetings, drive and GPS news, chatter.
  Test   // The TEST VOICE button: the owner asked, so only the mute settings apply.
};
struct SpeechSettings {
  bool sound = true;  // Master sound.
  uint8_t volume = 0; // 0 is mute.
  VoiceMode mode = VoiceMode::Off;
  bool silentTone = false; // The Silent alert tone keeps detections quiet, spoken or not.
};
inline bool speechEnabled(SpeechKind kind, const SpeechSettings &s) {
  if (!s.sound || !s.volume)
    return false;
  if (kind == SpeechKind::Test)
    return true;
  if (kind == SpeechKind::Alert)
    return s.mode != VoiceMode::Off && !s.silentTone;
  return s.mode == VoiceMode::All;
}
constexpr uint32_t SpeechGapMs = 4000;     // Between any two clips.
constexpr uint32_t PetSpeechGapMs = 20000; // Between two pet phrases.
constexpr uint32_t PetRepeatMs = 300000;   // Before the same pet phrase is heard again.
class SpeechGate {
public:
  // `alertBusy`: an alert is on screen or its tone is playing; those own the speaker.
  bool ready(SpeechKind kind, Clip clip, uint32_t now, bool alertBusy) const {
    if (kind == SpeechKind::Test)
      return true;
    if (kind == SpeechKind::Alert) // A detection cuts a pet phrase short, never another alert.
      return yielding_ || any_.ready(now, SpeechGapMs);
    return !alertBusy && unsigned(clip) < ClipCount && any_.ready(now, SpeechGapMs) &&
           pet_.ready(now, PetSpeechGapMs) && said_[unsigned(clip)].ready(now, PetRepeatMs);
  }
  void mark(SpeechKind kind, Clip clip, uint32_t now) {
    any_.mark(now);
    yielding_ = kind != SpeechKind::Alert;
    if (kind == SpeechKind::Pet && unsigned(clip) < ClipCount) {
      pet_.mark(now);
      said_[unsigned(clip)].mark(now);
    }
  }

private:
  Cooldown any_, pet_, said_[ClipCount];
  bool yielding_ = false;
};

// ---- Pet and system phrases that need a little logic ----------------------------------------
// GPS news for a drive in progress: one phrase when the fix is gained or lost, and only after
// the change has held for a few seconds, so a flickering fix stays quiet.
constexpr uint32_t GpsNewsHoldMs = 3000;
class GpsNews {
public:
  // Clip::GpsLocked or Clip::GpsLost while there is news the owner has not heard, otherwise
  // Clip::Count. News stays due until told(), so a rate-limited phrase is said late rather than
  // never, and it is dropped if the fix goes back to what was last heard.
  Clip update(bool recording, bool fix, uint32_t now) {
    if (!recording) {
      known_ = false;
      return Clip::Count;
    }
    if (!known_ || fix == told_) { // The drive-start line already covered the first state.
      known_ = true;
      told_ = fix;
      steadyAt_ = now;
      return Clip::Count;
    }
    if (uint32_t(now - steadyAt_) < GpsNewsHoldMs)
      return Clip::Count;
    return fix ? Clip::GpsLocked : Clip::GpsLost;
  }
  void told(Clip news) {
    told_ = news == Clip::GpsLocked;
  }

private:
  bool known_ = false, told_ = false;
  uint32_t steadyAt_ = 0;
};
// Starting a drive with no card: the spoken "No SD card!" first, then how to fix it.
inline constexpr const char *NoCardHelp = "No SD card! Insert FAT32; tap Retry SD";
// Caption for the "Drive saved!" clip: the spoken words first, then the count the unspoken
// summary line would have given.
inline void driveSavedCaption(char *out, size_t size, uint32_t finds, bool record) {
  const char *saved = clipCaption(Clip::DriveSaved);
  if (!finds)
    snprintf(out, size, "%s", saved);
  else
    snprintf(out, size, record ? "%s Record: %lu networks!" : "%s %lu networks.", saved,
             (unsigned long)finds);
}

// ---- Firmware (src/buddy.cpp) ---------------------------------------------------------------
// Plays the clip if the sound settings, the voice mode and the rate limits allow it, and says
// whether it did. Callers show clipCaption() when it returns true so the screen matches the voice.
bool voiceSpeak(Clip, SpeechKind);
} // namespace dash
