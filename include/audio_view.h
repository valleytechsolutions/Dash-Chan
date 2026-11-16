// SPDX-License-Identifier: GPL-3.0-or-later
// The Sound & Voice page, drawn from a plain snapshot of the settings. A template, like
// home_view.h, so the firmware (M5Canvas) and the PC preview render the same layout and the tap
// map can be tested on a PC.
//
//   5- 31  heading and BACK (drawn by the caller; shared by every menu page)
//  44- 79  alert tone
//  92-138  volume: current value, then five presets
// 151-184  master sound | voice mode
// 188-195  status line: what the voice mode covers, or what TEST VOICE just said
// 200-231  preview tone | test voice | Axon demo
#pragma once
#include "home_view.h"
#include "voice.h"

namespace dash {
constexpr uint8_t VolumeLevels[] = {0, 100, 170, 220, 255};
enum class AudioTap : uint8_t { None, Tone, Volume, Sound, Voice, PreviewTone, TestVoice, Demo };
struct AudioTapTarget {
  AudioTap action = AudioTap::None;
  uint8_t volume = 0; // The preset for AudioTap::Volume.
};
inline AudioTapTarget audioTap(int x, int y) {
  if (y >= 44 && y < 80)
    return {AudioTap::Tone};
  if (y >= 109 && y < 139) {
    int preset = (x - 8) / 62;
    return {AudioTap::Volume, VolumeLevels[preset < 0 ? 0 : preset > 4 ? 4 : preset]};
  }
  if (y >= 151 && y < 185)
    return {x >= 160 ? AudioTap::Voice : AudioTap::Sound};
  if (y >= 200 && y < 232)
    return {x >= 212 ? AudioTap::Demo : x >= 109 ? AudioTap::TestVoice : AudioTap::PreviewTone};
  return {};
}

struct AudioView {
  Palette colors{};
  Theme theme = Theme::Dark;
  const char *tone = "Personality";
  bool sound = true;
  uint8_t volume = 245;
  VoiceMode voice = VoiceMode::Off;
  bool tested = false;        // TEST VOICE was pressed a moment ago...
  const char *said = nullptr; // ...and spoke this caption; null means it was muted.
};
constexpr unsigned AudioStatusChars = 50; // Fits the 300 px between the page margins.
inline const char *voiceModeHelp(VoiceMode m) {
  return m == VoiceMode::Off      ? "Voice OFF: tones only, no speech."
         : m == VoiceMode::Alerts ? "Voice ALERTS: speaks detections only."
                                  : "Voice ALL: detections and pet phrases.";
}
inline void audioStatus(char *out, size_t size, const AudioView &v) {
  if (!v.tested)
    snprintf(out, size, "%s", voiceModeHelp(v.voice));
  else if (v.said)
    snprintf(out, size, "Said: %.44s", v.said);
  else
    snprintf(out, size, "Muted: turn sound on and volume up.");
}

namespace audioui {
template <class G>
void button(G &g, const AudioView &v, int x, int y, int w, int h, const char *label,
            bool active = false) {
  g.fillRoundRect(x, y, w, h, 9, active ? v.colors.accent : v.colors.panel);
  homeui::centered(g, label, x + w / 2, y + h / 2 - 3,
                   active ? (v.theme == Theme::Light ? 0xffff : Ink) : v.colors.text);
}
} // namespace audioui

template <class G> void drawAudio(G &g, const AudioView &v) {
  char s[64];
  snprintf(s, sizeof(s), "Alert tone: %s  >", v.tone);
  audioui::button(g, v, 8, 44, 304, 36, s);
  snprintf(s, sizeof(s), "Volume: %u / 255", v.volume);
  homeui::text(g, s, 12, 92, v.colors.muted);
  for (int i = 0; i < 5; ++i) {
    snprintf(s, sizeof(s), "%u", VolumeLevels[i]);
    audioui::button(g, v, 8 + i * 62, 109, 56, 30, s, v.volume == VolumeLevels[i]);
  }
  audioui::button(g, v, 8, 151, 148, 34, v.sound ? "Master sound: ON" : "Master sound: OFF");
  snprintf(s, sizeof(s), "Voice: %s  >", voiceModeName(v.voice));
  audioui::button(g, v, 164, 151, 148, 34, s);
  audioStatus(s, sizeof(s), v);
  homeui::text(g, s, 12, 188, v.colors.muted);
  audioui::button(g, v, 8, 200, 98, 32, "PREVIEW TONE");
  audioui::button(g, v, 111, 200, 98, 32, "TEST VOICE");
  audioui::button(g, v, 214, 200, 98, 32, "AXON DEMO", true);
}
} // namespace dash
