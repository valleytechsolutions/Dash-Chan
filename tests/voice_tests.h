// Host checks for include/voice.h, called from tests/core_test.cpp (run from the repository
// root). The WAV files and the generated source are checked by tools/pack_voice.py --check,
// which tools/test.py also runs.
#pragma once
#include "audio_view.h"
#include "personality.h"
#include "voice.h"
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>
namespace dash {
#define CHECK(expr)                                                                                \
  do {                                                                                             \
    if (!(expr)) {                                                                                 \
      std::cerr << "voice_tests.h:" << __LINE__ << " failed: " << #expr << '\n';                   \
      std::exit(1);                                                                                \
    }                                                                                              \
  } while (0)
inline void checkVoice() {
  // Every family announces something. Each family that existed when the clips were recorded
  // (None..Ring) has its own line; a family added later falls back to the generic line until
  // someone records one, and the compiler's switch warning in clipFor() points that out.
  std::set<Clip> announced;
  for (unsigned f = 0; f < FamilyCount; ++f) {
    Clip clip = clipFor(Family(f));
    CHECK(unsigned(clip) < ClipCount);
    if (f > unsigned(Family::Ring))
      continue;
    CHECK((clip == Clip::Generic) == (Family(f) == Family::None));
    CHECK(announced.insert(clip).second);
  }
  CHECK(announced.size() == 25);
  CHECK(clipFor(Family::SamsungTag) == Clip::SamsungTag && clipFor(Family::Ring) == Clip::Ring);
  CHECK(clipFor(Family(200)) == Clip::Generic);
  CHECK(!strcmp(clipCaption(Clip::Count), "") && !strcmp(clipCaption(Clip(250)), ""));

  // The manifest that names the WAV files lists the same clips, in the same order, with the
  // same captions as the table the firmware shows.
  {
    std::ifstream manifest("tools/voice_phrases.tsv");
    CHECK(manifest.good());
    std::vector<std::vector<std::string>> rows;
    for (std::string line; std::getline(manifest, line);) {
      if (line.empty() || line[0] == '#')
        continue;
      std::vector<std::string> cells;
      std::stringstream split(line);
      for (std::string cell; std::getline(split, cell, '\t');)
        cells.push_back(cell);
      CHECK(cells.size() >= 3);
      rows.push_back(cells);
    }
    CHECK(rows.size() == ClipCount);
    std::set<std::string> ids;
    for (unsigned i = 0; i < ClipCount; ++i) {
      const ClipInfo &info = ClipInfos[i];
      CHECK(info.id && info.caption && rows[i][0] == info.id && rows[i][2] == info.caption);
      CHECK(ids.insert(info.id).second);
      CHECK(*info.caption && strlen(info.caption) <= BubbleChars);
      for (const char *c = info.caption; *c; ++c)
        CHECK(*c >= 32 && *c < 127);
    }
  }
  // Lines that exist both as text and as a recording must read the same.
  CHECK(!strcmp(clipCaption(Clip::Love), LoveYourFace));
  CHECK(strlen(NoCardHelp) <= BubbleChars &&
        !strncmp(NoCardHelp, clipCaption(Clip::NoSd), strlen(clipCaption(Clip::NoSd))));

  // The saved setting: a new key wins; otherwise the old on/off switch means ALL or OFF.
  CHECK(savedVoiceMode(false, 0, true) == VoiceMode::All);
  CHECK(savedVoiceMode(false, 0, false) == VoiceMode::Off);
  CHECK(savedVoiceMode(false, 1, false) == VoiceMode::Off);
  CHECK(savedVoiceMode(true, 0, true) == VoiceMode::Off);
  CHECK(savedVoiceMode(true, 1, false) == VoiceMode::Alerts);
  CHECK(savedVoiceMode(true, 2, false) == VoiceMode::All);
  CHECK(savedVoiceMode(true, 3, true) == VoiceMode::Off && // Garbage is never louder than off.
        savedVoiceMode(true, 255, true) == VoiceMode::Off);
  CHECK(nextVoiceMode(VoiceMode::Off) == VoiceMode::Alerts &&
        nextVoiceMode(VoiceMode::Alerts) == VoiceMode::All &&
        nextVoiceMode(VoiceMode::All) == VoiceMode::Off);
  CHECK(!strcmp(voiceModeName(VoiceMode::Off), "OFF") &&
        !strcmp(voiceModeName(VoiceMode::Alerts), "ALERTS") &&
        !strcmp(voiceModeName(VoiceMode::All), "ALL"));

  // Gates: master sound and volume silence everything; the mode picks which kinds speak.
  for (unsigned mode = 0; mode < 3; ++mode)
    for (unsigned kind = 0; kind < 3; ++kind)
      for (unsigned bits = 0; bits < 8; ++bits) {
        SpeechSettings s;
        s.mode = VoiceMode(mode);
        s.sound = bits & 1;
        s.volume = bits & 2 ? 170 : 0;
        s.silentTone = bits & 4;
        bool audible = s.sound && s.volume;
        bool expect = SpeechKind(kind) == SpeechKind::Test ? audible
                      : SpeechKind(kind) == SpeechKind::Alert
                          ? audible && mode >= 1 && !s.silentTone
                          : audible && mode == 2;
        CHECK(speechEnabled(SpeechKind(kind), s) == expect);
      }
  CHECK(!speechEnabled(SpeechKind::Alert, SpeechSettings{})); // Defaults are silent.

  // Rate limits. Start near the 32-bit wrap so the arithmetic is exercised across it.
  {
    SpeechGate gate;
    uint32_t t = 0xffffe000u;
    CHECK(gate.ready(SpeechKind::Alert, Clip::Flock, t, true));
    gate.mark(SpeechKind::Alert, Clip::Flock, t);
    // No second clip of any kind for four seconds; an alert never interrupts another alert.
    CHECK(!gate.ready(SpeechKind::Alert, Clip::Tile, t + 3999, true));
    CHECK(!gate.ready(SpeechKind::Pet, Clip::AllClear, t + 3999, false));
    CHECK(gate.ready(SpeechKind::Alert, Clip::Tile, t + 4000, true));
    CHECK(gate.ready(SpeechKind::Pet, Clip::AllClear, t + 4000, false));
    // Pet phrases wait for an open alert or its tone.
    CHECK(!gate.ready(SpeechKind::Pet, Clip::AllClear, t + 9000, true));
    gate.mark(SpeechKind::Pet, Clip::AllClear, t + 9000);
    // One pet phrase per twenty seconds...
    CHECK(!gate.ready(SpeechKind::Pet, Clip::Hungry, t + 9000 + 19999, false));
    CHECK(gate.ready(SpeechKind::Pet, Clip::Hungry, t + 9000 + 20000, false));
    // ...and the same one not again for five minutes, even with others in between.
    gate.mark(SpeechKind::Pet, Clip::Hungry, t + 30000);
    CHECK(!gate.ready(SpeechKind::Pet, Clip::AllClear, t + 60000, false));
    CHECK(!gate.ready(SpeechKind::Pet, Clip::AllClear, t + 9000 + PetRepeatMs - 1, false));
    CHECK(gate.ready(SpeechKind::Pet, Clip::AllClear, t + 9000 + PetRepeatMs, false));
    CHECK(gate.ready(SpeechKind::Pet, Clip::Sleepy, t + 60000, false));
    // A detection pre-empts a pet phrase at once, then holds the floor.
    gate.mark(SpeechKind::Pet, Clip::Sleepy, t + 60000);
    CHECK(gate.ready(SpeechKind::Alert, Clip::Drone, t + 60001, true));
    gate.mark(SpeechKind::Alert, Clip::Drone, t + 60001);
    CHECK(!gate.ready(SpeechKind::Alert, Clip::Drone, t + 62000, true));
    // The test button always speaks and yields to a real alert like a pet phrase does.
    CHECK(gate.ready(SpeechKind::Test, Clip::Hello, t + 60002, true));
    gate.mark(SpeechKind::Test, Clip::Hello, t + 62000);
    CHECK(gate.ready(SpeechKind::Alert, Clip::Ring, t + 62001, true));
    CHECK(!gate.ready(SpeechKind::Pet, Clip::Hello, t + 62001, false));
    CHECK(!gate.ready(SpeechKind::Pet, Clip::Count, t + 900000, false));
    CHECK(SpeechGapMs >= 4000 && PetSpeechGapMs >= 20000);
  }

  // Which clip an alert speaks.
  CHECK(alertClip(Family::Axon) == Clip::Axon && alertClip(Family::Axon, 2) == Clip::Axon);
  CHECK(alertClip(Family::Axon, 0) == Clip::SpicyPig &&
        alertClip(Family::Axon, 1) == Clip::SpicyFuzz);
  CHECK(alertClip(Family::Flock, 0) == Clip::Flock); // Spicy lines are body-camera lines only.
  for (unsigned f = 0; f < FamilyCount; ++f) {
    bool tag = Family(f) == Family::Tracker || Family(f) == Family::SamsungTag ||
               Family(f) == Family::GoogleTag || Family(f) == Family::Tile;
    CHECK(trackerFamily(Family(f)) == tag);
    CHECK(alertClip(Family(f), -1, AlertTwist::Following) ==
          (tag ? Clip::Following : clipFor(Family(f))));
    CHECK(alertClip(Family(f), 0, AlertTwist::Flood) == Clip::Flood);
  }

  // GPS news: only during a drive, only on a change that lasts, never for the starting state.
  {
    GpsNews news;
    CHECK(news.update(false, true, 0) == Clip::Count);
    CHECK(news.update(true, true, 1000) == Clip::Count); // Started with a fix: nothing to add.
    CHECK(news.update(true, false, 2000) == Clip::Count);
    CHECK(news.update(true, true, 3000) == Clip::Count); // A flicker.
    CHECK(news.update(true, false, 4000) == Clip::Count);
    // The hold runs from the last moment the fix still matched what was said.
    CHECK(news.update(true, false, 3000 + GpsNewsHoldMs - 1) == Clip::Count);
    CHECK(news.update(true, false, 3000 + GpsNewsHoldMs) == Clip::GpsLost);
    // Not spoken yet (rate limit, or Voice below ALL): still due, and dropped if the fix returns.
    CHECK(news.update(true, false, 9000) == Clip::GpsLost);
    CHECK(news.update(true, true, 9500) == Clip::Count);
    CHECK(news.update(true, false, 10000) == Clip::Count);
    CHECK(news.update(true, false, 10000 + GpsNewsHoldMs) == Clip::GpsLost);
    news.told(Clip::GpsLost);
    CHECK(news.update(true, false, 20000) == Clip::Count); // Said once.
    CHECK(news.update(true, true, 20000 + GpsNewsHoldMs - 1) == Clip::Count);
    CHECK(news.update(true, true, 20000 + GpsNewsHoldMs) == Clip::GpsLocked);
    news.told(Clip::GpsLocked);
    CHECK(news.update(true, true, 30000) == Clip::Count);
    CHECK(news.update(false, false, 40000) == Clip::Count); // Drive over: forget.
    CHECK(news.update(true, false, 41000) == Clip::Count);  // "Waiting for a fix" covered it.
    CHECK(news.update(true, true, 42000) == Clip::Count);
    CHECK(news.update(true, true, 41000 + GpsNewsHoldMs) == Clip::GpsLocked);
  }

  // Sound & Voice page: every control answers where it is drawn, and the gaps do nothing.
  {
    auto at = [](int x, int y) { return audioTap(x, y).action; };
    CHECK(at(160, 44) == AudioTap::Tone && at(8, 79) == AudioTap::Tone);
    CHECK(at(160, 43) == AudioTap::None && at(160, 80) == AudioTap::None);
    for (int i = 0; i < 5; ++i) {
      AudioTapTarget tap = audioTap(8 + i * 62 + 28, 124);
      CHECK(tap.action == AudioTap::Volume && tap.volume == VolumeLevels[i]);
    }
    CHECK(audioTap(0, 110).volume == 0 && audioTap(319, 138).volume == 255);
    CHECK(at(82, 151) == AudioTap::Sound && at(159, 184) == AudioTap::Sound);
    CHECK(at(160, 151) == AudioTap::Voice && at(238, 184) == AudioTap::Voice);
    CHECK(at(238, 190) == AudioTap::None); // The status line is not a button.
    CHECK(at(57, 200) == AudioTap::PreviewTone && at(108, 231) == AudioTap::PreviewTone);
    CHECK(at(111, 200) == AudioTap::TestVoice && at(209, 231) == AudioTap::TestVoice);
    CHECK(at(214, 200) == AudioTap::Demo && at(311, 231) == AudioTap::Demo);
    CHECK(at(160, 232) == AudioTap::None && at(160, 199) == AudioTap::None);
    // The status line always fits between the margins.
    AudioView view;
    char line[64];
    for (unsigned mode = 0; mode < 3; ++mode) {
      view.voice = VoiceMode(mode);
      audioStatus(line, sizeof(line), view);
      CHECK(!strcmp(line, voiceModeHelp(VoiceMode(mode))) && strlen(line) <= AudioStatusChars);
    }
    view.tested = true;
    audioStatus(line, sizeof(line), view);
    CHECK(!strncmp(line, "Muted", 5) && strlen(line) <= AudioStatusChars);
    for (unsigned i = 0; i < ClipCount; ++i) {
      view.said = ClipInfos[i].caption;
      audioStatus(line, sizeof(line), view);
      CHECK(strlen(line) <= AudioStatusChars && strstr(line, ClipInfos[i].caption));
    }
    view.said = "12345678901234567890123456789012345678901234567"; // A full-width caption.
    audioStatus(line, sizeof(line), view);
    CHECK(strlen(line) == AudioStatusChars);
  }

  // The spoken drive summary leads with the spoken words and still fits the bubble.
  for (uint32_t finds : {0u, 1u, 24u, 150u, 99999u, 4294967295u})
    for (bool record : {false, true}) {
      char line[64];
      driveSavedCaption(line, sizeof(line), finds, record);
      CHECK(strlen(line) <= BubbleChars && !strncmp(line, "Drive saved!", 12));
      CHECK(finds ? strstr(line, "networks") != nullptr : !strcmp(line, "Drive saved!"));
    }
}
#undef CHECK
} // namespace dash
