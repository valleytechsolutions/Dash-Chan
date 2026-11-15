// SPDX-License-Identifier: GPL-3.0-or-later
// Every recon screen, drawn from plain snapshots: the home panel and its radar, the Full Face
// chip, the alert card, the Recon page (LIVE / SEEN / STATS), the device card and the detection
// settings. Templates over the canvas type, like home_view.h, so the firmware (M5Canvas) and the
// PC preview draw the same pixels; every box is a constant here and both the drawing and the tap
// maps read it, so a control cannot be drawn in one place and listen in another.
//
// Nothing in this file touches the live ReconTable while drawing. The firmware copies the rows a
// screen needs into a ReconRow snapshot (reconHome(), reconList(), ...) and draws from the copy.
//
// The LCD font is 6x8 px per character, so a box of w pixels holds w / 6 characters.
#pragma once
#include "recon.h"
#include "themes.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace dash {
// ---- Tier colours ---------------------------------------------------------------------------
// One hue per tier on every theme: red surveillance, violet hacker gear, amber attacks, blue
// trackers, grey beacons. Each is tuned to stand off that theme's background and panel.
inline uint16_t tierColor(Theme theme, Tier tier) {
  static constexpr uint32_t table[4][TierCount] = {
      {0xff6b6b, 0xb99cff, 0xffc247, 0x5fd4ff, 0x8f9bb8},  // Dark
      {0xc22a3a, 0x6b3fc4, 0x9a5b00, 0x0a6fa8, 0x66707f},  // Light
      {0xff6a5c, 0xe58bff, 0xffd84a, 0x5ff0ff, 0x6f9a78},  // Matrix
      {0xff5a4d, 0xc9a0ff, 0xffbe52, 0x6fd3ff, 0xa09094}}; // BWM
  return rgb565(table[unsigned(theme) % 4][unsigned(tier) < TierCount ? unsigned(tier) : 4]);
}
// Text drawn on a tier colour: the Light theme's tier colours are deep and need white.
inline uint16_t onTier(Theme theme) {
  return theme == Theme::Light ? 0xffff : 0x1085;
}
inline const char *tierShort(Tier t) { // At most 7 characters.
  static const char *const names[] = {"SURVEIL", "HACKER", "ATTACK", "TRACKER", "BEACON"};
  return unsigned(t) < TierCount ? names[unsigned(t)] : names[TierCount - 1];
}

// ---- Snapshot rows --------------------------------------------------------------------------
struct ReconRow {
  uint8_t mac[6]{};
  bool ble = false;
  uint8_t addressType = 0, channel = 0;
  Family family = Family::None;
  Confidence confidence = Confidence::None;
  const char *evidence = ""; // Static text from the classifier.
  char name[32]{}, label[LabelChars]{};
  int8_t rssi = 0, peakRssi = 0, trend = 0; // trend: +1 closer, -1 further.
  bool live = false, restored = false, ignored = false, following = false, alerted = false,
       flood = false, hasPosition = false;
  bool ageKnown = true;          // False for a ledger row when the clock is not set.
  uint32_t ageS = 0, firstS = 0; // Seconds since it was last and first heard.
  uint32_t encounters = 0;
  AlertVerdict verdict = AlertVerdict::SameEncounter;
  float lat = 0, lon = 0;
  uint32_t followS = 0, followMeters = 0; // Company so far, when `following`.
  uint8_t places = 0;
};
// `clock` is seconds since 2000 UTC, or 0 while the time is unknown.
inline ReconRow reconRow(const ReconDevice &d, uint32_t now, uint32_t clock) {
  ReconRow r;
  memcpy(r.mac, d.mac, 6);
  r.ble = d.ble;
  r.addressType = d.addressType;
  r.channel = d.channel;
  r.family = d.family;
  r.confidence = d.confidence;
  r.evidence = d.evidence ? d.evidence : "";
  snprintf(r.name, sizeof(r.name), "%s", d.name);
  snprintf(r.label, sizeof(r.label), "%s", d.label);
  r.rssi = d.rssi;
  r.peakRssi = d.peakRssi;
  r.restored = d.restored;
  r.live = reconLive(d, now);
  r.trend = r.live ? int8_t(reconTrend(d)) : 0;
  r.ignored = d.ignored;
  r.following = d.following;
  r.alerted = d.alerted;
  r.flood = d.flood;
  r.encounters = d.encounters;
  r.verdict = d.verdict;
  r.hasPosition = d.lat != 0 || d.lon != 0;
  r.lat = d.lat;
  r.lon = d.lon;
  if (d.restored) {
    // A ledger row's millis() belong to another boot; only the wall clock can age it.
    r.ageKnown = clock && d.utc && clock >= d.utc;
    r.ageS = r.firstS = r.ageKnown ? clock - d.utc : 0;
  } else {
    r.ageS = uint32_t(now - d.lastAt) / 1000;
    r.firstS = uint32_t(now - d.firstAt) / 1000;
  }
  if (d.following) {
    r.followS = followSpanMs(d) / 1000;
    r.followMeters = uint32_t(followSpanMeters(d));
    r.places = d.pointCount;
  }
  return r;
}
inline bool sameDevice(const ReconRow &a, const ReconRow &b) {
  return a.family == b.family && a.ble == b.ble && a.addressType == b.addressType &&
         !memcmp(a.mac, b.mac, 6);
}

namespace reconview {
struct Box {
  int x, y, w, h;
  constexpr bool has(int px, int py) const {
    return px >= x && px < x + w && py >= y && py < y + h;
  }
};

// ---- Text builders (pure; the tests feed them the worst cases) --------------------------------
// Copy at most `chars` characters; a longer text ends in "..".
inline void fit(char *out, size_t size, const char *text, unsigned chars) {
  if (!size)
    return;
  if (chars > size - 1)
    chars = unsigned(size - 1);
  size_t n = strlen(text);
  if (n <= chars) {
    memcpy(out, text, n + 1);
    return;
  }
  unsigned keep = chars >= 2 ? chars - 2 : 0;
  memcpy(out, text, keep);
  for (unsigned i = keep; i < chars; ++i)
    out[i] = '.';
  out[chars] = 0;
}
// "now", "42s", "4m", "3h", "12d": at most 4 characters.
inline void ageText(char out[8], uint32_t seconds) {
  if (seconds < 2)
    snprintf(out, 8, "now");
  else if (seconds < 60)
    snprintf(out, 8, "%us", unsigned(seconds));
  else if (seconds < 3600)
    snprintf(out, 8, "%um", unsigned(seconds / 60));
  else if (seconds < 172800)
    snprintf(out, 8, "%uh", unsigned(seconds / 3600));
  else
    snprintf(out, 8, "%ud", unsigned(seconds / 86400 > 999 ? 999 : seconds / 86400));
}
// "just now", "4m ago": at most 8 characters.
inline void agoText(char out[12], uint32_t seconds) {
  char age[8];
  ageText(age, seconds);
  if (seconds < 2)
    snprintf(out, 12, "just now");
  else
    snprintf(out, 12, "%s ago", age);
}
inline bool sameText(const char *a, const char *b) {
  for (; *a && *b; ++a, ++b)
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
      return false;
  return !*a && !*b;
}
// The label says more than the family name does (and is not one of the engine's ledger tags).
inline bool labelAdds(const ReconRow &r) {
  return *r.label && !ledgerTag(r.label) && !sameText(r.label, familyShort(r.family)) &&
         !sameText(r.label, familyName(r.family));
}
// One-line name for a narrow row: the product label when it fits, else the short family name.
inline void rowTitle(char *out, size_t size, const ReconRow &r, unsigned chars) {
  const char *text = labelAdds(r) && strlen(r.label) <= chars ? r.label : familyShort(r.family);
  fit(out, size, text, chars);
}
// For a wide row: "Plate reader  Motorola", falling back to the advertised name.
inline void wideTitle(char *out, size_t size, const ReconRow &r, unsigned chars) {
  char text[80];
  const char *extra = labelAdds(r) ? r.label : r.name;
  if (*extra)
    snprintf(text, sizeof(text), "%s  %s", familyShort(r.family), extra);
  else
    snprintf(text, sizeof(text), "%s", familyShort(r.family));
  fit(out, size, text, chars);
}
inline void countText(char out[8], uint32_t encounters) { // "x3", at most "x999+".
  if (encounters > 999)
    snprintf(out, 8, "x999+");
  else
    snprintf(out, 8, "x%u", unsigned(encounters));
}
// Right-hand reading of a row: signal while live, otherwise how long ago. At most 8 characters.
inline void readingText(char out[12], const ReconRow &r) {
  if (r.live)
    snprintf(out, 12, "%ddB", int(r.rssi));
  else if (!r.ageKnown)
    snprintf(out, 12, "earlier");
  else
    agoText(out, r.ageS);
}
// Second line of a home row. `left` is at most 13 characters, `right` at most 6.
constexpr unsigned HomeRowChars = 19;
inline void homeStatus(char left[24], char right[8], const ReconRow &r) {
  char reading[12];
  readingText(reading, r);
  right[0] = 0;
  if (r.following && r.live) {
    snprintf(left, 24, "FOLLOWING");
    snprintf(right, 8, "%ddB", int(r.rssi));
    return;
  }
  if (r.ignored)
    snprintf(left, 24, "mine %s", reading);
  else
    snprintf(left, 24, "%s %s", r.ble ? "BLE" : "WiFi", reading);
  if (r.encounters > 1)
    countText(right, r.encounters);
}
// Second line of a Recon page row. `left` is at most 32 characters, `flag` at most 9.
inline void listStatus(char left[40], char flag[12], const ReconRow &r) {
  char seen[8], radio[12];
  countText(seen, r.encounters);
  if (r.ble)
    snprintf(radio, sizeof(radio), "BLE");
  else if (r.channel)
    snprintf(radio, sizeof(radio), "WiFi ch%u", unsigned(r.channel));
  else
    snprintf(radio, sizeof(radio), "WiFi");
  snprintf(left, 40, "%s | %s | seen %s", radio, confidenceName(r.confidence), seen);
  snprintf(flag, 12, "%s",
           r.following  ? "FOLLOWING"
           : r.ignored  ? "IGNORED"
           : r.flood    ? "FLOOD"
           : r.restored ? "LEDGER"
           : r.alerted  ? "ALERTED"
                        : "");
}
// Why the last sighting did or did not interrupt, as a sentence of at most 50 characters.
inline const char *verdictWhy(const ReconRow &r) {
  if (r.restored)
    return r.alerted ? "It alerted on an earlier boot."
                     : "No alert on record from the earlier boot.";
  switch (r.verdict) {
  case AlertVerdict::Alert:
    return "It passed every quiet rule, so I spoke up.";
  case AlertVerdict::Flood:
    return "One alert for a burst of new tag addresses.";
  case AlertVerdict::FilteredType:
    return "This device type is switched off.";
  case AlertVerdict::Ignored:
    return "It is on your ignore list.";
  case AlertVerdict::BelowThreshold:
    return "Listed only: under your Notify confidence.";
  case AlertVerdict::FloodQuiet:
    return "Part of a tag flood, which is probably spoofed.";
  case AlertVerdict::QuietRoad:
    return "Tags and Ring stay list-only while driving.";
  case AlertVerdict::SameEncounter:
    return r.alerted ? "Already announced; quiet until it leaves."
                     : "Nothing new since it was first listed.";
  case AlertVerdict::Snoozed:
    return "Auto snooze: it must come closer first.";
  case AlertVerdict::FamilyCooldown:
    return "Same kind of device alerted a moment ago.";
  case AlertVerdict::GlobalCooldown:
    return "Another alert fired seconds ago.";
  }
  return "";
}
// Word wrap into rows of `width` characters; text that does not fit `maxLines` ends in "..".
inline int wrap(const char *s, int width, char rows[][64], int maxLines) {
  int count = 0;
  if (width > 63)
    width = 63;
  while (*s && count < maxLines) {
    while (*s == ' ')
      ++s;
    int n = int(strlen(s));
    if (n > width) {
      n = width;
      while (n > 0 && s[n] != ' ')
        --n;
      if (n <= 0)
        n = width;
    }
    if (!n)
      break;
    memcpy(rows[count], s, n);
    rows[count++][n] = 0;
    s += n;
  }
  while (*s == ' ')
    ++s;
  if (*s && count) { // Ran out of rows: mark the cut.
    char *last = rows[count - 1];
    int n = int(strlen(last));
    if (n > width - 2)
      n = width - 2;
    memcpy(last + n, "..", 3);
  }
  return count;
}
// The device card's text, one screen line each. Every line is at most DetailChars long.
constexpr unsigned DetailChars = 50;
struct DetailText {
  char title[24];   // familyName, drawn at size 2.
  char line[8][64]; // Tier, model/name, address, signal, seen, decision, why, place.
  char info[3][64]; // familyInfo, wrapped.
  int infoLines = 0;
  char clue[2][64]; // The classifier's evidence, wrapped.
  int clueLines = 0;
};
inline void detailText(DetailText &t, const ReconRow &r) {
  char text[160], first[12], last[12], mac[18];
  fit(t.title, sizeof(t.title), familyName(r.family), 20);
  snprintf(text, sizeof(text), "%s tier | %s confidence%s", tierName(tierOf(r.family)),
           confidenceName(r.confidence),
           r.ignored    ? " | IGNORED"
           : r.restored ? " | LEDGER"
                        : "");
  fit(t.line[0], 64, text, DetailChars);
  bool model = labelAdds(r);
  if (model && *r.name)
    snprintf(text, sizeof(text), "Model: %s | name: %s", r.label, r.name);
  else if (model)
    snprintf(text, sizeof(text), "Model: %s", r.label);
  else if (*r.name)
    snprintf(text, sizeof(text), "Name: %s", r.name);
  else
    snprintf(text, sizeof(text), "No name advertised");
  fit(t.line[1], 64, text, DetailChars);
  macText(mac, r.mac);
  const char *kind = addressKindName(addressKind(r.ble, r.addressType, r.mac));
  if (r.ble)
    snprintf(text, sizeof(text), "BLE %s (%s)", mac, kind);
  else if (r.channel)
    snprintf(text, sizeof(text), "WiFi ch%u %s (%s)", unsigned(r.channel), mac, kind);
  else
    snprintf(text, sizeof(text), "WiFi %s (%s)", mac, kind);
  fit(t.line[2], 64, text, DetailChars);
  if (r.live)
    snprintf(text, sizeof(text), "Signal %d dBm now, peak %d, %s", int(r.rssi), int(r.peakRssi),
             r.trend > 0   ? "getting closer"
             : r.trend < 0 ? "moving away"
                           : "steady");
  else
    snprintf(text, sizeof(text), "Signal %d dBm when last heard, peak %d", int(r.rssi),
             int(r.peakRssi));
  fit(t.line[3], 64, text, DetailChars);
  agoText(first, r.firstS);
  agoText(last, r.ageS);
  unsigned long seen = r.encounters > 99999 ? 99999 : r.encounters;
  if (r.restored && !r.ageKnown)
    snprintf(text, sizeof(text), "Encounters: %lu | last heard on an earlier boot", seen);
  else if (r.restored)
    snprintf(text, sizeof(text), "Encounters: %lu | last heard %s", seen, last);
  else if (r.live)
    snprintf(text, sizeof(text), "Encounters: %lu | first %s | in range now", seen, first);
  else
    snprintf(text, sizeof(text), "Encounters: %lu | first %s | last %s", seen, first, last);
  fit(t.line[4], 64, text, DetailChars);
  snprintf(text, sizeof(text), "Alert decision: %s",
           r.restored ? (r.alerted ? "alerted" : "none") : verdictName(r.verdict));
  fit(t.line[5], 64, text, DetailChars);
  fit(t.line[6], 64, verdictWhy(r), DetailChars);
  if (r.following)
    snprintf(text, sizeof(text), "FOLLOWING: %lu min across %u places, %lu.%lu km",
             (unsigned long)(r.followS / 60 > 9999 ? 9999 : r.followS / 60), unsigned(r.places),
             (unsigned long)(r.followMeters / 1000 > 999 ? 999 : r.followMeters / 1000),
             (unsigned long)(r.followMeters % 1000 / 100));
  else if (r.hasPosition)
    snprintf(text, sizeof(text), "Last heard at %.5f, %.5f", double(r.lat), double(r.lon));
  else
    snprintf(text, sizeof(text), "No GPS position for this encounter");
  fit(t.line[7], 64, text, DetailChars);
  t.infoLines = wrap(familyInfo(r.family), DetailChars, t.info, 3);
  snprintf(text, sizeof(text), "Clue: %s", *r.evidence ? r.evidence : "none recorded");
  t.clueLines = wrap(text, DetailChars, t.clue, 2);
}
// One JSON object per row for the serial RECON command. Returns the length, or 0 if it does not
// fit. Names come from the air, so quotes, backslashes and anything unprintable are escaped.
inline size_t rowJson(char *out, size_t size, const ReconRow &r, unsigned index) {
  char mac[18], name[2 * sizeof(r.name)], label[2 * sizeof(r.label)];
  macText(mac, r.mac);
  auto escape = [](char *to, const char *from) {
    for (; *from; ++from) {
      char c = *from >= 32 && *from < 127 ? *from : '?';
      if (c == '"' || c == '\\')
        *to++ = '\\';
      *to++ = c;
    }
    *to = 0;
  };
  escape(name, r.name);
  escape(label, r.label);
  auto flag = [](bool b) { return b ? "true" : "false"; };
  int n =
      snprintf(out, size,
               "{\"i\":%u,\"family\":\"%s\",\"tier\":\"%s\",\"confidence\":\"%s\","
               "\"transport\":\"%s\",\"address_type\":%u,\"mac\":\"%s\",\"name\":\"%s\","
               "\"label\":\"%s\",\"rssi\":%d,\"peak\":%d,\"trend\":%d,\"channel\":%u,"
               "\"live\":%s,\"age_s\":%ld,\"first_s\":%lu,\"encounters\":%lu,"
               "\"verdict\":\"%s\",\"alerted\":%s,\"ignored\":%s,\"following\":%s,"
               "\"flood\":%s,\"restored\":%s}",
               index, familyKey(r.family), tierName(tierOf(r.family)), confidenceName(r.confidence),
               r.ble ? "BLE" : "WIFI", unsigned(r.addressType), mac, name, label, int(r.rssi),
               int(r.peakRssi), int(r.trend), unsigned(r.channel), flag(r.live),
               r.ageKnown ? long(r.ageS) : -1L, (unsigned long)r.firstS,
               (unsigned long)r.encounters, verdictName(r.verdict), flag(r.alerted),
               flag(r.ignored), flag(r.following), flag(r.flood), flag(r.restored));
  return n > 0 && size_t(n) < size ? size_t(n) : 0;
}

// ---- Home panel -----------------------------------------------------------------------------
//  29-174  the panel, right of the buddy in Recon view. Tapping it opens the Recon page; tapping
//          a row opens that device.
//    watching: header + tier chips, radar (whole panel when nothing is listed, a strip when
//              rows are), then up to four rows and "+N more"
//    driving:  header + tier chips, the top two rows, then this drive's counters
constexpr Box Panel{184, 29, 130, 146};
constexpr unsigned HomeRows = 4, DriveRows = 2, HomeBlips = 24;
constexpr int RowPitch = 22, RowH = 21;
constexpr Box DriveBlock{188, 106, 122, 64};
constexpr unsigned homeRowLimit(bool recording) {
  return recording ? DriveRows : HomeRows;
}
// With one or two rows the radar keeps most of the panel.
constexpr int homeRowsTop(bool recording, unsigned shown) {
  return recording ? 47 : shown <= 2 ? 121 : 77;
}
constexpr Box homeRow(bool recording, unsigned shown, unsigned i) {
  return {188, homeRowsTop(recording, shown) + int(i) * RowPitch, 122, RowH};
}
constexpr Box homeRadar(unsigned shown) {
  return shown ? Box{189, 47, 120, homeRowsTop(false, shown) - 50} : Box{189, 48, 120, 84};
}
constexpr Box tierChip(unsigned tier) {
  return {226 + int(tier) * 17, 32, 16, 11};
}
// Full Face: a chip in the stage's bottom right corner, clear of the feeling tag (top left), the
// speech bubble (top) and the drive counters (bottom centre). Its touch zone is a little larger.
constexpr Box FaceChip{246, 154, 68, 18};
constexpr Box FaceChipZone{238, 148, 82, 27};
// The alert card replaces the vitals row; DETAILS jumps to the device, the rest dismisses.
constexpr Box AlertCard{6, 178, 308, 26};
constexpr Box AlertDetails{250, 181, 60, 20};
constexpr Box AlertDetailsZone{244, 178, 70, 26};

struct Blip {
  uint8_t angle = 0; // 0..255 around the dial, fixed per address so a blip does not jump about.
  int8_t rssi = 0;
  Tier tier = Tier::Beacon;
  bool ignored = false;
};
inline uint8_t addressAngle(const uint8_t *mac) {
  uint32_t h = 2166136261u;
  for (unsigned i = 0; i < 6; ++i)
    h = (h ^ mac[i]) * 16777619u;
  return uint8_t(h ^ (h >> 8) ^ (h >> 16) ^ (h >> 24));
}
// Where a blip sits: strong signals near the centre, weak ones at the rim, always inside `box`.
inline void blipPoint(const Box &box, const Blip &b, int &x, int &y) {
  int rssi = b.rssi > -40 ? -40 : b.rssi < -95 ? -95 : b.rssi;
  float reach = 0.15f + 0.85f * float(-40 - rssi) / 55.0f;
  float angle = float(b.angle) * 6.2831853f / 256.0f;
  int rx = box.w / 2 - 7, ry = box.h / 2 - 5;
  x = box.x + box.w / 2 + int(lroundf(cosf(angle) * reach * float(rx)));
  y = box.y + box.h / 2 + int(lroundf(sinf(angle) * reach * float(ry)));
}
} // namespace reconview

// What the home screen shows of the table: the top rows, the live counts and the radar blips.
struct ReconHome {
  ReconRow rows[reconview::HomeRows];
  unsigned count = 0; // Rows filled, in ReconSort::Priority order.
  unsigned total = 0; // Every row in the table, live or not.
  unsigned live = 0;
  uint16_t liveTier[TierCount]{};
  reconview::Blip blips[reconview::HomeBlips];
  unsigned blipCount = 0;
  int topTier() const { // Highest live tier, or -1 when nothing is in range.
    for (unsigned t = 0; t < TierCount; ++t)
      if (liveTier[t])
        return int(t);
    return -1;
  }
};
inline void reconHome(ReconHome &out, const ReconTable &table, const ReconCounts &counts,
                      uint32_t now, uint32_t clock) {
  out = ReconHome();
  uint16_t order[reconview::HomeBlips];
  unsigned n = table.sorted(order, reconview::HomeBlips, now, ReconSort::Priority);
  for (unsigned i = 0; i < n; ++i) {
    const ReconDevice &d = table.at(order[i]);
    if (i < reconview::HomeRows)
      out.rows[out.count++] = reconRow(d, now, clock);
    if (reconLive(d, now))
      out.blips[out.blipCount++] = {reconview::addressAngle(d.mac), d.rssi, tierOf(d.family),
                                    d.ignored};
  }
  out.total = counts.total;
  out.live = counts.live;
  for (unsigned t = 0; t < TierCount; ++t)
    out.liveTier[t] = counts.liveTier[t];
}

// ---- Recon page -----------------------------------------------------------------------------
//   5- 31  heading and BACK (shared by every menu page)
//  37- 58  LIVE | SEEN | STATS
//  63-197  five rows (LIVE, SEEN) or the totals (STATS)
// 202-231  PREV, page, NEXT
namespace reconview {
enum class Tab : uint8_t { Live, Seen, Stats };
constexpr unsigned Tabs = 3, ListRows = 5, StatCells = 10;
constexpr const char *TabNames[Tabs] = {"LIVE", "SEEN", "STATS"};
constexpr Box BackButton{254, 5, 58, 27};
constexpr Box BackZone{250, 0, 70, 34}; // The rule src/ui.cpp applies on every menu page.
constexpr Box tab(unsigned i) {
  return {8 + int(i) * 103, 37, 98, 22};
}
constexpr Box listRow(unsigned i) {
  return {8, 63 + int(i) * 27, 304, 25};
}
constexpr Box Prev{8, 202, 84, 30}, Next{228, 202, 84, 30};
constexpr Box tierBox(unsigned tier) {
  return {8 + int(tier) * 61, 63, 58, 30};
}
constexpr Box statCell(unsigned i) {
  return {8 + int(i % 2) * 156, 124 + int(i / 2) * 15, 148, 14};
}
constexpr unsigned pageCount(unsigned total, unsigned perPage) {
  return total ? (total + perPage - 1) / perPage : 1;
}
constexpr unsigned stepPage(unsigned page, unsigned pages, bool forward) {
  return pages <= 1 ? 0 : (page + (forward ? 1 : pages - 1)) % pages;
}
constexpr unsigned StatPages = (FamilyCount - 1 + StatCells - 1) / StatCells;
} // namespace reconview

struct ReconList {
  ReconRow rows[reconview::ListRows];
  unsigned count = 0, total = 0, page = 0, pages = 1;
};
// LIVE is what is in range now, most important first; SEEN is everything, newest first. `order`
// is scratch space; give it table.capacity() entries.
inline void reconList(ReconList &out, const ReconTable &table, reconview::Tab tab, unsigned page,
                      uint32_t now, uint32_t clock, uint16_t *order, unsigned orderSize) {
  out = ReconList();
  bool liveOnly = tab == reconview::Tab::Live;
  unsigned n =
      table.sorted(order, orderSize, now, liveOnly ? ReconSort::Priority : ReconSort::Recent);
  if (liveOnly) { // Priority order puts every live row first.
    unsigned live = 0;
    while (live < n && reconLive(table.at(order[live]), now))
      ++live;
    n = live;
  }
  out.total = n;
  out.pages = reconview::pageCount(n, reconview::ListRows);
  out.page = page < out.pages ? page : out.pages - 1;
  for (unsigned i = out.page * reconview::ListRows; i < n && out.count < reconview::ListRows; ++i)
    out.rows[out.count++] = reconRow(table.at(order[i]), now, clock);
}
enum class LedgerState : uint8_t { NoCard, Ready, Failed };
inline const char *ledgerStateName(LedgerState s) { // At most 12 characters.
  return s == LedgerState::Ready ? "OK" : s == LedgerState::Failed ? "write failed" : "no SD card";
}
struct ReconStats {
  uint32_t lifetime[FamilyCount]{}; // Devices ever seen, per family, across restarts.
  uint32_t lifetimeTotal = 0;
  ReconCounts counts;
  unsigned ignoreList = 0;
  LedgerState ledger = LedgerState::NoCard;
  unsigned page = 0;
};
inline void reconStats(ReconStats &out, const ReconTable &table, const ReconCounts &counts,
                       unsigned ignoreList, LedgerState ledger, unsigned page) {
  out = ReconStats();
  for (unsigned f = 1; f < FamilyCount; ++f)
    out.lifetime[f] = table.lifetime(Family(f));
  out.lifetimeTotal = table.lifetimeTotal();
  out.counts = counts;
  out.ignoreList = ignoreList;
  out.ledger = ledger;
  out.page = page < reconview::StatPages ? page : reconview::StatPages - 1;
}
namespace reconview {
// Families for the STATS grid: most seen first, then by tier, so the zeros trail in a fixed order.
inline void statOrder(Family out[FamilyCount - 1], const uint32_t lifetime[FamilyCount]) {
  unsigned n = 0;
  for (unsigned t = 0; t < TierCount; ++t)
    for (unsigned f = 1; f < FamilyCount; ++f)
      if (unsigned(tierOf(Family(f))) == t)
        out[n++] = Family(f);
  for (unsigned i = 1; i < n; ++i) { // Stable insertion sort by count, descending.
    Family f = out[i];
    unsigned at = i;
    for (; at > 0 && lifetime[unsigned(out[at - 1])] < lifetime[unsigned(f)]; --at)
      out[at] = out[at - 1];
    out[at] = f;
  }
}

enum class ListHit : uint8_t { None, Back, Tab, Row, Prev, Next };
struct ListTarget {
  ListHit hit = ListHit::None;
  unsigned index = 0; // Tab or row number.
};
// `rows` is how many rows the page is showing (0 on STATS); `pages` how many pages it has.
inline ListTarget listTap(int x, int y, unsigned rows, unsigned pages) {
  if (BackZone.has(x, y))
    return {ListHit::Back};
  for (unsigned i = 0; i < Tabs; ++i)
    if (tab(i).has(x, y))
      return {ListHit::Tab, i};
  for (unsigned i = 0; i < rows && i < ListRows; ++i)
    if (listRow(i).has(x, y))
      return {ListHit::Row, i};
  if (pages > 1 && Prev.has(x, y))
    return {ListHit::Prev};
  if (pages > 1 && Next.has(x, y))
    return {ListHit::Next};
  return {};
}

// ---- Device card ----------------------------------------------------------------------------
constexpr Box IgnoreButton{8, 205, 148, 29}, CloseButton{164, 205, 148, 29};
enum class DetailHit : uint8_t { None, Back, Ignore, Close };
inline DetailHit detailTap(int x, int y) {
  return BackZone.has(x, y)       ? DetailHit::Back
         : IgnoreButton.has(x, y) ? DetailHit::Ignore
         : CloseButton.has(x, y)  ? DetailHit::Close
                                  : DetailHit::None;
}

// ---- Detection settings ---------------------------------------------------------------------
//  40- 69  Notify confidence            74-103  family quiet | reminders
// 108-137  quiet road | auto snooze    142-171  device types (opens the type filter)
// 176-205  ignored count | CLEAR ALL   212-231  note
constexpr Box NotifyButton{8, 40, 304, 30}, FamilyQuietButton{8, 74, 148, 30},
    RemindersButton{164, 74, 148, 30}, QuietRoadButton{8, 108, 148, 30},
    SnoozeButton{164, 108, 148, 30}, TypesButton{8, 142, 304, 30}, IgnoredLabel{8, 176, 148, 30},
    ClearButton{164, 176, 148, 30};
enum class SettingsHit : uint8_t {
  None,
  Back,
  Notify,
  FamilyQuiet,
  Reminders,
  QuietRoad,
  Snooze,
  Types,
  Clear
};
inline SettingsHit settingsTap(int x, int y) {
  return BackZone.has(x, y)            ? SettingsHit::Back
         : NotifyButton.has(x, y)      ? SettingsHit::Notify
         : FamilyQuietButton.has(x, y) ? SettingsHit::FamilyQuiet
         : RemindersButton.has(x, y)   ? SettingsHit::Reminders
         : QuietRoadButton.has(x, y)   ? SettingsHit::QuietRoad
         : SnoozeButton.has(x, y)      ? SettingsHit::Snooze
         : TypesButton.has(x, y)       ? SettingsHit::Types
         : ClearButton.has(x, y)       ? SettingsHit::Clear
                                       : SettingsHit::None;
}
constexpr uint8_t nextSnooze(uint8_t value) { // OFF, 5, 10.
  return value >= 10 ? 0 : value >= 5 ? 10 : 5;
}
constexpr uint32_t nextFamilyQuiet(uint32_t ms) { // 60, 180, 300 seconds.
  return ms == 60000 ? 180000 : ms == 180000 ? 300000 : 60000;
}

// The type filter: a whole tier at once along the top, single families below, eight to a page.
constexpr unsigned TypeCells = 8;
constexpr unsigned TypePages = (FamilyCount - 1 + TypeCells - 1) / TypeCells;
constexpr Box tierToggle(unsigned tier) {
  return {8 + int(tier) * 61, 38, 58, 22};
}
constexpr Box typeCell(unsigned i) {
  return {8 + int(i % 2) * 156, 66 + int(i / 2) * 32, 148, 28};
}
// Families in screen order: by tier, then in the order they were added.
inline void typeOrder(Family out[FamilyCount - 1]) {
  unsigned n = 0;
  for (unsigned t = 0; t < TierCount; ++t)
    for (unsigned f = 1; f < FamilyCount; ++f)
      if (unsigned(tierOf(Family(f))) == t)
        out[n++] = Family(f);
}
inline uint32_t tierMask(Tier tier) {
  uint32_t mask = 0;
  for (unsigned f = 1; f < FamilyCount; ++f)
    if (tierOf(Family(f)) == tier)
      mask |= TypeFilter::flag(Family(f));
  return mask;
}
// 0 none of the tier is tracked, 1 some of it, 2 all of it.
inline unsigned tierState(uint32_t filter, Tier tier) {
  uint32_t mask = tierMask(tier), on = filter & mask;
  return !on ? 0 : on == mask ? 2 : 1;
}
// Tapping a tier switches all of it on, unless all of it already is: then off.
inline uint32_t toggleTier(uint32_t filter, Tier tier) {
  uint32_t mask = tierMask(tier);
  return (tierState(filter, tier) == 2 ? filter & ~mask : filter | mask) & TypeFilter::All;
}
enum class TypesHit : uint8_t { None, Back, Tier, Family, Prev, Next };
struct TypesTarget {
  TypesHit hit = TypesHit::None;
  unsigned index = 0;           // Tier number.
  Family family = Family::None; // For TypesHit::Family.
};
inline TypesTarget typesTap(int x, int y, unsigned page) {
  if (BackZone.has(x, y))
    return {TypesHit::Back};
  for (unsigned t = 0; t < TierCount; ++t)
    if (tierToggle(t).has(x, y))
      return {TypesHit::Tier, t};
  Family order[FamilyCount - 1];
  typeOrder(order);
  for (unsigned i = 0; i < TypeCells; ++i) {
    unsigned n = page * TypeCells + i;
    if (n < FamilyCount - 1 && typeCell(i).has(x, y))
      return {TypesHit::Family, 0, order[n]};
  }
  if (Prev.has(x, y))
    return {TypesHit::Prev};
  if (Next.has(x, y))
    return {TypesHit::Next};
  return {};
}

// ---- Drawing --------------------------------------------------------------------------------
struct Style {
  Palette colors{};
  Theme theme = Theme::Dark;
};
inline uint16_t blend(uint16_t a, uint16_t b, int amount, int total) { // amount/total of b.
  int r = ((a >> 11) & 31) + (((b >> 11) & 31) - ((a >> 11) & 31)) * amount / total;
  int g = ((a >> 5) & 63) + (((b >> 5) & 63) - ((a >> 5) & 63)) * amount / total;
  int bl = (a & 31) + ((b & 31) - (a & 31)) * amount / total;
  return uint16_t((r << 11) | (g << 5) | bl);
}
template <class G> void text(G &g, const char *s, int x, int y, uint16_t color, int size = 1) {
  g.setTextSize(size);
  g.setTextColor(color);
  g.setCursor(x, y);
  g.print(s);
}
template <class G>
void right(G &g, const char *s, int xRight, int y, uint16_t color, int size = 1) {
  text(g, s, xRight - int(strlen(s)) * 6 * size, y, color, size);
}
template <class G> void centered(G &g, const char *s, int cx, int y, uint16_t color) {
  text(g, s, cx - int(strlen(s)) * 3, y, color);
}
template <class G>
void button(G &g, const Style &s, const Box &b, const char *label, bool active = false) {
  g.fillRoundRect(b.x, b.y, b.w, b.h, 9, active ? s.colors.accent : s.colors.panel);
  centered(g, label, b.x + b.w / 2, b.y + b.h / 2 - 3,
           active ? (s.theme == Theme::Light ? 0xffff : 0x1085) : s.colors.text);
}
// The heading every menu page shares (src/ui.cpp draws the same thing for the other pages).
template <class G> void heading(G &g, const Style &s, const char *name) {
  text(g, name, 12, 12, s.colors.accent);
  button(g, s, BackButton, "BACK");
}
// Closer points up, further points down; steady draws nothing. 5 px wide.
template <class G> void trendArrow(G &g, int x, int y, int trend, uint16_t color) {
  if (trend > 0)
    g.fillTriangle(x, y + 5, x + 4, y + 5, x + 2, y + 1, color);
  else if (trend < 0)
    g.fillTriangle(x, y + 1, x + 4, y + 1, x + 2, y + 5, color);
}
template <class G> void tierChips(G &g, const Style &s, const uint16_t liveTier[TierCount]) {
  char n[8];
  for (unsigned t = 0; t < TierCount; ++t) {
    Box c = tierChip(t);
    uint16_t color = tierColor(s.theme, Tier(t));
    if (!liveTier[t]) { // A dim pip keeps each tier's place in the row.
      g.fillRoundRect(c.x + 5, c.y + 4, 6, 3, 1, blend(s.colors.bg, color, 2, 5));
      continue;
    }
    g.fillRoundRect(c.x, c.y, c.w, c.h, 4, color);
    snprintf(n, sizeof(n), "%u", unsigned(liveTier[t] > 99 ? 99 : liveTier[t]));
    centered(g, n, c.x + c.w / 2, c.y + 2, onTier(s.theme));
  }
}
template <class G>
void radar(G &g, const Style &s, const Box &b, uint32_t now, const Blip *blips, unsigned count) {
  int cx = b.x + b.w / 2, cy = b.y + b.h / 2, rx = b.w / 2 - 4, ry = b.h / 2 - 2;
  if (rx > ry * 3 / 2 && b.h > 60) // The big dial stays close to round.
    rx = ry * 3 / 2;
  for (int k = 1; k <= 3; ++k)
    g.drawEllipse(cx, cy, rx * k / 3, ry * k / 3, s.colors.panel);
  float sweep = now / 1800.0f;
  g.drawLine(cx, cy, cx + int(cosf(sweep) * float(rx - 1)), cy + int(sinf(sweep) * float(ry - 1)),
             s.colors.accent);
  g.fillCircle(cx, cy, 2, s.colors.accent);
  Box field{cx - rx - 4, b.y, 2 * rx + 8, b.h};
  for (unsigned i = count; i-- > 0;) { // Most important last, so it is drawn on top.
    int x, y;
    blipPoint(field, blips[i], x, y);
    uint16_t color = tierColor(s.theme, blips[i].tier);
    g.fillCircle(x, y, 3, s.colors.bg);
    if (blips[i].ignored)
      g.drawCircle(x, y, 2, color);
    else
      g.fillCircle(x, y, 2, color);
  }
}
template <class G> void homeRowCard(G &g, const Style &s, const Box &b, const ReconRow &r) {
  char title[24], left[24], extra[8];
  bool dim = !r.live || r.ignored;
  // A tag that is following us is the top tier's business, whatever it is.
  uint16_t tier = tierColor(s.theme, r.following && r.live ? Tier::Surveillance : tierOf(r.family));
  g.fillRoundRect(b.x, b.y, b.w, b.h, 5, s.colors.panel);
  g.fillRoundRect(b.x, b.y, 3, b.h, 1, r.live ? tier : s.colors.muted);
  rowTitle(title, sizeof(title), r, HomeRowChars);
  text(g, title, b.x + 6, b.y + 2, dim ? s.colors.muted : s.colors.text);
  homeStatus(left, extra, r);
  uint16_t status = dim ? s.colors.muted : tier;
  text(g, left, b.x + 6, b.y + 11, status);
  if (r.live && !r.following)
    trendArrow(g, b.x + 8 + int(strlen(left)) * 6, b.y + 11, r.trend, status);
  right(g, extra, b.x + b.w - 3, b.y + 11, dim ? s.colors.muted : s.colors.text);
}
struct ReconPanel {
  Style style;
  uint32_t now = 0;
  bool recording = false;
  uint32_t wifiSaved = 0, bleSaved = 0;
};
template <class G> void drawHomePanel(G &g, const ReconPanel &p, const ReconHome &h) {
  const Style &s = p.style;
  char n[24];
  g.fillRoundRect(Panel.x, Panel.y, Panel.w, Panel.h, 10, s.colors.bg);
  text(g, "RECON", 191, 34, s.colors.accent);
  tierChips(g, s, h.liveTier);
  unsigned limit = homeRowLimit(p.recording);
  unsigned shown = h.count < limit ? h.count : limit;
  if (!p.recording) {
    Box dial = homeRadar(shown);
    radar(g, s, dial, p.now, h.blips, h.blipCount);
    if (!shown) {
      centered(g, "All quiet", 249, 139, s.colors.text);
      centered(g, "Nothing sketchy near", 249, 153, s.colors.muted);
    } else if (!h.live) {
      g.fillRoundRect(219, dial.y + dial.h / 2 - 6, 60, 12, 5, s.colors.bg);
      centered(g, "All quiet", 249, dial.y + dial.h / 2 - 4, s.colors.muted);
    }
  } else if (!shown)
    centered(g, "No threats heard", 249, 66, s.colors.muted);
  for (unsigned i = 0; i < shown; ++i)
    homeRowCard(g, s, homeRow(p.recording, shown, i), h.rows[i]);
  if (h.total > shown && shown) {
    snprintf(n, sizeof(n), "+%u more", h.total - shown);
    right(g, n, 309, p.recording ? 94 : 166, s.colors.muted);
  }
  if (p.recording) {
    const Box &d = DriveBlock;
    g.fillRoundRect(d.x, d.y, d.w, d.h, 6, s.colors.panel);
    text(g, "THIS DRIVE", d.x + 7, d.y + 5, s.colors.muted);
    for (int i = 0; i < 2; ++i) {
      int y = d.y + 19 + i * 22;
      uint32_t value = i ? p.bleSaved : p.wifiSaved;
      g.fillCircle(d.x + 10, y + 8, 3, i ? s.colors.heart : s.colors.accent);
      text(g, i ? "BLE" : "WiFi", d.x + 18, y + 5, s.colors.text);
      snprintf(n, sizeof(n), "~%lu", (unsigned long)(value > 99999 ? 99999 : value));
      right(g, n, d.x + d.w - 6, y, s.colors.text, 2);
    }
  }
}
template <class G> void drawFaceChip(G &g, const Style &s, const ReconHome &h) {
  char n[16];
  const Box &c = FaceChip;
  int top = h.topTier();
  g.fillRoundRect(c.x, c.y, c.w, c.h, 7, s.colors.bg);
  g.fillCircle(c.x + 9, c.y + 9, 3, top < 0 ? s.colors.muted : tierColor(s.theme, Tier(top)));
  snprintf(n, sizeof(n), "RECON %u", h.live > 99 ? 99u : h.live);
  text(g, n, c.x + 16, c.y + 5, top < 0 ? s.colors.muted : s.colors.text);
}
// What the alert card says: two lines of at most AlertCardChars, left of the DETAILS button.
constexpr unsigned AlertCardChars = 38;
struct AlertCardView {
  uint8_t kind = 0; // 0 device, 1 tracker flood, 2 following (app.h AlertKind).
  bool demo = false;
  Family family = Family::None;
  Confidence confidence = Confidence::None;
  const char *label = "";
  int rssi = 0;
  uint32_t followS = 0, followMeters = 0;
};
inline void alertCardText(char title[48], char detail[48], const AlertCardView &a) {
  char name[32];
  bool model = *a.label && !ledgerTag(a.label) && !sameText(a.label, familyName(a.family));
  fit(name, sizeof(name), model ? a.label : familyName(a.family), 22);
  snprintf(title, 48, "%s%s  %ddB", a.demo ? "DEMO " : "", name,
           a.rssi < -127 ? -127
           : a.rssi > 0  ? 0
                         : a.rssi);
  if (a.kind == 1)
    snprintf(detail, 48, "Tracker flood, probably spoofed");
  else if (a.kind == 2)
    snprintf(detail, 48, "With us for %lu min / %lu.%lu km",
             (unsigned long)(a.followS / 60 > 999 ? 999 : a.followS / 60),
             (unsigned long)(a.followMeters / 1000 > 99 ? 99 : a.followMeters / 1000),
             (unsigned long)(a.followMeters % 1000 / 100));
  else
    snprintf(detail, 48, "%s | %s | tap to dismiss", tierName(tierOf(a.family)),
             confidenceName(a.confidence));
}
template <class G> void drawAlertCard(G &g, const Style &s, const AlertCardView &a) {
  char title[48], detail[48];
  const Box &c = AlertCard;
  uint16_t tier = tierColor(s.theme, a.kind == 2 ? Tier::Surveillance : tierOf(a.family));
  g.fillRoundRect(c.x, c.y, c.w, c.h, 8, tier);
  g.fillRoundRect(c.x + 4, c.y + 1, c.w - 5, c.h - 2, 7, s.colors.bg);
  alertCardText(title, detail, a);
  text(g, title, 16, 182, tier);
  text(g, detail, 16, 193, s.colors.text);
  if (!a.demo) {
    const Box &d = AlertDetails;
    g.fillRoundRect(d.x, d.y, d.w, d.h, 7, tier);
    centered(g, "DETAILS", d.x + d.w / 2, d.y + 6, onTier(s.theme));
  }
}

template <class G> void listRowCard(G &g, const Style &s, const Box &b, const ReconRow &r) {
  char title[48], reading[12], left[40], flag[12];
  bool dim = !r.live || r.ignored;
  uint16_t tier = tierColor(s.theme, tierOf(r.family));
  g.fillRoundRect(b.x, b.y, b.w, b.h, 6, s.colors.panel);
  g.fillRoundRect(b.x, b.y, 4, b.h, 2, r.live ? tier : s.colors.muted);
  wideTitle(title, sizeof(title), r, 36);
  text(g, title, b.x + 10, b.y + 3, dim ? s.colors.muted : s.colors.text);
  readingText(reading, r);
  int edge = b.x + b.w - 6 - (r.live && r.trend ? 8 : 0);
  right(g, reading, edge, b.y + 3, r.live ? tier : s.colors.muted);
  if (r.live)
    trendArrow(g, edge + 2, b.y + 3, r.trend, tier);
  listStatus(left, flag, r);
  text(g, left, b.x + 10, b.y + 14, s.colors.muted);
  right(g, flag, b.x + b.w - 6, b.y + 14,
        r.following ? tierColor(s.theme, Tier::Surveillance) : s.colors.muted);
}
template <class G> void pager(G &g, const Style &s, unsigned page, unsigned pages) {
  char n[24];
  if (pages > 1) {
    button(g, s, Prev, "PREV");
    button(g, s, Next, "NEXT");
  }
  snprintf(n, sizeof(n), "%u / %u", page + 1, pages);
  centered(g, n, 160, 213, s.colors.muted);
}
template <class G> void tabs(G &g, const Style &s, Tab current) {
  for (unsigned i = 0; i < Tabs; ++i)
    button(g, s, tab(i), TabNames[i], unsigned(current) == i);
}
template <class G> void paragraph(G &g, const char *words, int y, uint16_t color, int lines = 3) {
  char rows[4][64];
  int n = wrap(words, 48, rows, lines > 4 ? 4 : lines);
  for (int i = 0; i < n; ++i)
    text(g, rows[i], 16, y + i * 12, color);
}
// LIVE and SEEN. `card` and `radio` choose the wording of the empty states.
struct ListContext {
  bool card = true, radio = true;
  unsigned seenTotal = 0; // Rows under SEEN, mentioned when LIVE is empty.
};
template <class G>
void drawReconList(G &g, const Style &s, Tab current, const ReconList &list,
                   const ListContext &ctx) {
  char n[80];
  heading(g, s, "RECON");
  tabs(g, s, current);
  for (unsigned i = 0; i < list.count; ++i)
    listRowCard(g, s, listRow(i), list.rows[i]);
  if (!list.count && current == Tab::Live) {
    text(g, ctx.radio ? "Nothing sketchy in range right now." : "The radios are not running.", 16,
         80, s.colors.text);
    paragraph(g,
              "Every device I can classify is listed here while it is in range, whatever your "
              "alert settings say.",
              100, s.colors.muted);
    if (ctx.seenTotal) {
      snprintf(n, sizeof(n), "%u earlier %s under SEEN.", ctx.seenTotal,
               ctx.seenTotal == 1 ? "device is" : "devices are");
      text(g, n, 16, 146, s.colors.accent);
    }
  } else if (!list.count) {
    text(g, "No classified devices yet.", 16, 80, s.colors.text);
    paragraph(g,
              ctx.card ? "Everything I classify is kept here and saved to the SD card, so the "
                         "list comes back after a restart."
                       : "Everything I classify is kept here until power off. Insert an SD card "
                         "to keep the list across restarts.",
              100, s.colors.muted);
  }
  pager(g, s, list.page, list.pages);
}
template <class G> void drawReconStats(G &g, const Style &s, const ReconStats &v) {
  char n[80];
  heading(g, s, "RECON");
  tabs(g, s, Tab::Stats);
  for (unsigned t = 0; t < TierCount; ++t) {
    Box b = tierBox(t);
    g.fillRoundRect(b.x, b.y, b.w, b.h, 6, s.colors.panel);
    centered(g, tierShort(Tier(t)), b.x + b.w / 2, b.y + 4, tierColor(s.theme, Tier(t)));
    snprintf(n, sizeof(n), "%u/%u", unsigned(v.counts.liveTier[t]),
             unsigned(v.counts.totalTier[t]));
    centered(g, n, b.x + b.w / 2, b.y + 17, s.colors.text);
  }
  snprintf(n, sizeof(n), "Live/listed above. Following %u | Ignored %u",
           unsigned(v.counts.followingLive), v.ignoreList);
  text(g, n, 12, 98, s.colors.muted);
  snprintf(n, sizeof(n), "Ever seen: %lu | Ledger: %s",
           (unsigned long)(v.lifetimeTotal > 9999999 ? 9999999 : v.lifetimeTotal),
           ledgerStateName(v.ledger));
  text(g, n, 12, 110, v.ledger == LedgerState::Failed ? s.colors.danger : s.colors.muted);
  Family order[FamilyCount - 1];
  statOrder(order, v.lifetime);
  for (unsigned i = 0; i < StatCells; ++i) {
    unsigned at = v.page * StatCells + i;
    if (at >= FamilyCount - 1)
      break;
    Family f = order[at];
    Box b = statCell(i);
    uint32_t seen = v.lifetime[unsigned(f)];
    g.fillRoundRect(b.x, b.y + 2, 3, 9, 1, seen ? tierColor(s.theme, tierOf(f)) : s.colors.panel);
    text(g, familyShort(f), b.x + 8, b.y + 3, seen ? s.colors.text : s.colors.muted);
    snprintf(n, sizeof(n), "%lu", (unsigned long)(seen > 999999 ? 999999 : seen));
    right(g, n, b.x + b.w - 2, b.y + 3, seen ? s.colors.accent : s.colors.muted);
  }
  pager(g, s, v.page, StatPages);
}
// `present` is false when the row has left the table since the card was opened.
template <class G> void drawReconDetail(G &g, const Style &s, const ReconRow &r, bool present) {
  DetailText t;
  detailText(t, r);
  heading(g, s, "DEVICE");
  uint16_t tier = tierColor(s.theme, tierOf(r.family));
  g.fillRoundRect(8, 38, 4, 16, 2, tier);
  text(g, t.title, 18, 38, s.colors.text, 2);
  static constexpr int rows[8] = {58, 69, 80, 91, 102, 113, 124, 135};
  for (int i = 0; i < 8; ++i)
    text(g, t.line[i], 10, rows[i],
         i == 0                  ? tier
         : i == 7 && r.following ? tierColor(s.theme, Tier::Surveillance)
         : i == 5                ? s.colors.accent
                                 : s.colors.text);
  for (int i = 0; i < t.infoLines; ++i)
    text(g, t.info[i], 10, 148 + i * 11, s.colors.muted);
  for (int i = 0; i < t.clueLines; ++i)
    text(g, t.clue[i], 10, 182 + i * 11, s.colors.muted);
  if (present)
    button(g, s, IgnoreButton, r.ignored ? "UNIGNORE" : "IGNORE (IT'S MINE)", !r.ignored);
  else
    button(g, s, IgnoreButton, "NO LONGER LISTED");
  button(g, s, CloseButton, "BACK");
}

struct SettingsView {
  Confidence threshold = Confidence::Medium;
  uint32_t familyQuietMs = 60000, typeFilter = TypeFilter::Defaults;
  bool repeat = false, quietRoad = true;
  uint8_t autoSnooze = 5;
  unsigned ignored = 0;
  bool confirmClear = false; // CLEAR ALL was tapped once.
};
template <class G> void drawReconSettings(G &g, const Style &s, const SettingsView &v) {
  char n[64];
  heading(g, s, "DETECTION");
  snprintf(n, sizeof(n), "Notify: %s confidence or higher", confidenceName(v.threshold));
  button(g, s, NotifyButton, n);
  snprintf(n, sizeof(n), "Family quiet: %lu s", (unsigned long)(v.familyQuietMs / 1000));
  button(g, s, FamilyQuietButton, n);
  button(g, s, RemindersButton, v.repeat ? "Reminders: 5 min" : "Reminders: OFF");
  button(g, s, QuietRoadButton, v.quietRoad ? "Quiet road: ON" : "Quiet road: OFF");
  if (v.autoSnooze)
    snprintf(n, sizeof(n), "Auto snooze: %u", unsigned(v.autoSnooze));
  else
    snprintf(n, sizeof(n), "Auto snooze: OFF");
  button(g, s, SnoozeButton, n);
  TypeFilter filter;
  filter.mask = v.typeFilter & TypeFilter::All;
  snprintf(n, sizeof(n), "Device types: %u of %u on  >", filter.enabled(), TypeFilter::total());
  button(g, s, TypesButton, n);
  snprintf(n, sizeof(n), "Ignored: %u device%s", v.ignored, v.ignored == 1 ? "" : "s");
  centered(g, n, IgnoredLabel.x + IgnoredLabel.w / 2, IgnoredLabel.y + 12, s.colors.text);
  button(g, s, ClearButton,
         !v.ignored       ? "NONE TO CLEAR"
         : v.confirmClear ? "TAP AGAIN: CLEAR"
                          : "CLEAR ALL",
         v.confirmClear && v.ignored);
  text(g, "Everything classified is listed; these only", 12, 212, s.colors.muted);
  text(g, "shape alerts. A match is a clue, not proof.", 12, 224, s.colors.muted);
}
template <class G> void drawReconTypes(G &g, const Style &s, uint32_t typeFilter, unsigned page) {
  char n[40];
  heading(g, s, "DEVICE TYPES");
  for (unsigned t = 0; t < TierCount; ++t) {
    Box b = tierToggle(t);
    unsigned state = tierState(typeFilter, Tier(t));
    uint16_t color = tierColor(s.theme, Tier(t));
    g.fillRoundRect(b.x, b.y, b.w, b.h, 7, state == 2 ? color : s.colors.panel);
    if (state == 1)
      g.fillRoundRect(b.x, b.y + b.h - 4, b.w, 4, 2, color);
    centered(g, tierShort(Tier(t)), b.x + b.w / 2, b.y + 7,
             state == 2 ? onTier(s.theme)
             : state    ? s.colors.text
                        : s.colors.muted);
  }
  Family order[FamilyCount - 1];
  typeOrder(order);
  TypeFilter filter;
  filter.mask = typeFilter & TypeFilter::All;
  for (unsigned i = 0; i < TypeCells; ++i) {
    unsigned at = page * TypeCells + i;
    if (at >= FamilyCount - 1)
      break;
    Family f = order[at];
    Box b = typeCell(i);
    bool on = filter.tracks(f);
    g.fillRoundRect(b.x, b.y, b.w, b.h, 8, s.colors.panel);
    g.fillRoundRect(b.x, b.y, 4, b.h, 2, on ? tierColor(s.theme, tierOf(f)) : s.colors.muted);
    text(g, familyShort(f), b.x + 10, b.y + 10, on ? s.colors.text : s.colors.muted);
    right(g, on ? "ON" : "OFF", b.x + b.w - 8, b.y + 10, on ? s.colors.accent : s.colors.muted);
  }
  button(g, s, Prev, "PREV");
  button(g, s, Next, "NEXT");
  snprintf(n, sizeof(n), "%u/%u  %u of %u on", page + 1, TypePages, filter.enabled(),
           TypeFilter::total());
  centered(g, n, 160, 213, s.colors.muted);
}
} // namespace reconview
} // namespace dash
