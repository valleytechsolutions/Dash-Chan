// Host checks for include/recon_view.h: the recon screens' layout, tap maps and text fitting.
// core_test.cpp calls reconViewTests() from main.
#pragma once
#include "home_view.h"
#include "idle_motion.h"
#include "personality.h"
#include "recon_view.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <vector>
namespace recon_view_tests {
using namespace dash;
namespace rv = dash::reconview;
#define VIEW_CHECK(expr)                                                                           \
  do {                                                                                             \
    if (!(expr)) {                                                                                 \
      std::cerr << "recon view CHECK failed at " << __LINE__ << ": " << #expr << '\n';             \
      std::exit(1);                                                                                \
    }                                                                                              \
  } while (0)

// A canvas that only remembers what text was printed where.
struct Ink {
  struct Run {
    int x, y, w, h;
    std::string text;
  };
  std::vector<Run> runs;
  int size = 1, x = 0, y = 0;
  void setTextSize(int s) {
    size = s;
  }
  void setTextColor(uint16_t) {}
  void setCursor(int cx, int cy) {
    x = cx;
    y = cy;
  }
  void print(const char *s) {
    if (*s)
      runs.push_back({x, y, int(strlen(s)) * 6 * size, 8 * size, s});
  }
  void fillRect(int, int, int, int, uint16_t) {}
  void fillRoundRect(int, int, int, int, int, uint16_t) {}
  void fillCircle(int, int, int, uint16_t) {}
  void drawCircle(int, int, int, uint16_t) {}
  void drawEllipse(int, int, int, int, uint16_t) {}
  void drawLine(int, int, int, int, uint16_t) {}
  void fillTriangle(int, int, int, int, int, int, uint16_t) {}
  bool has(const char *words) const { // Wrapped text reads as one sentence.
    std::string all;
    for (const Run &r : runs)
      all += r.text + " ";
    return all.find(words) != std::string::npos;
  }
};
// Every run of text lies inside `box`, is plain ASCII, and no two runs touch.
inline bool tidy(const Ink &ink, const rv::Box &box) {
  for (size_t i = 0; i < ink.runs.size(); ++i) {
    const auto &a = ink.runs[i];
    if (a.x < box.x || a.y < box.y || a.x + a.w > box.x + box.w || a.y + a.h > box.y + box.h)
      return false;
    for (char c : a.text)
      if (c < 32 || c > 126)
        return false;
    for (size_t j = i + 1; j < ink.runs.size(); ++j) {
      const auto &b = ink.runs[j];
      if (a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h)
        return false;
    }
  }
  return true;
}
constexpr rv::Box Screen{0, 0, 320, 240}, Body{8, 34, 304, 206};
inline bool apart(const rv::Box &a, const rv::Box &b) {
  return a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y;
}
inline bool within(const rv::Box &a, const rv::Box &outer) {
  return a.x >= outer.x && a.y >= outer.y && a.x + a.w <= outer.x + outer.w &&
         a.y + a.h <= outer.y + outer.h;
}
inline rv::Style style(Theme theme = Theme::Dark) {
  return {palette(theme), theme};
}

// Relative luminance contrast of two RGB565 colours (WCAG formula).
inline double contrast(uint16_t a, uint16_t b) {
  auto luminance = [](uint16_t c) {
    double channel[3] = {((c >> 11) & 31) / 31.0, ((c >> 5) & 63) / 63.0, (c & 31) / 31.0};
    for (double &v : channel)
      v = v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
    return 0.2126 * channel[0] + 0.7152 * channel[1] + 0.0722 * channel[2];
  };
  double la = luminance(a), lb = luminance(b);
  return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

// The worst rows a screen can be asked to draw.
inline std::vector<ReconRow> worstRows() {
  std::vector<ReconRow> rows;
  const char *labels[] = {"", "WWWWWWWWWWWWWWWWWWWWWWW", "Axon Body 3", "following", "flood"};
  const char *names[] = {"", "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW", "a \"quoted\\name\""};
  unsigned n = 0;
  for (unsigned f = 0; f < FamilyCount; ++f)
    for (int state = 0; state < 7; ++state) {
      ReconRow r;
      for (unsigned i = 0; i < 6; ++i)
        r.mac[i] = uint8_t(0xf0 + i + f);
      r.family = Family(f);
      r.confidence = Confidence(1 + n % 3);
      r.ble = n % 2;
      r.addressType = r.ble ? uint8_t(n % 4) : 0;
      r.channel = r.ble ? 0 : uint8_t(n % 3 ? 165 : 0);
      r.evidence = n % 2 ? "W WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW "
                           "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW"
                         : "";
      snprintf(r.label, sizeof(r.label), "%s", labels[n % 5]);
      snprintf(r.name, sizeof(r.name), "%s", names[n % 3]);
      r.rssi = n % 2 ? -127 : 0;
      r.peakRssi = n % 3 ? -127 : 0;
      r.trend = int8_t(int(n % 3) - 1);
      r.encounters = n % 4 == 0 ? 0xffffffffu : n % 4 == 1 ? 1 : n % 4 == 2 ? 999 : 1000;
      r.verdict = AlertVerdict(n % 11);
      r.live = state < 3;
      // A live row is fresh for the first ten seconds; after that it shows an age instead.
      r.fresh = r.live && (state == 0 ? n % 2 == 0 : state == 2 ? n % 4 < 2 : false);
      r.ignored = state == 1 || state == 5;
      r.following = state == 2 || state == 4;
      r.restored = state >= 5;
      r.ageKnown = state != 6;
      r.alerted = n % 2;
      r.flood = n % 7 == 0;
      r.ageS = state == 3           ? 0xffffffffu
               : state == 4         ? 59
               : state == 5         ? 86400u * 400
               : r.live && !r.fresh ? ReconFreshS + 1 + n % 49
                                    : n % 5;
      r.firstS = 0xffffffffu;
      r.hasPosition = n % 2;
      r.lat = -89.999999f;
      r.lon = -179.999999f;
      r.followS = 0xffffffffu;
      r.followMeters = 0xffffffffu;
      r.places = 255;
      rows.push_back(r);
      ++n;
    }
  return rows;
}

inline void palettes() {
  for (Theme theme : {Theme::Dark, Theme::Light, Theme::Matrix, Theme::Bwm}) {
    Palette p = palette(theme);
    std::set<uint16_t> seen;
    for (unsigned t = 0; t < TierCount; ++t) {
      uint16_t c = tierColor(theme, Tier(t));
      VIEW_CHECK(seen.insert(c).second); // Five different colours per theme.
      // Readable as a bar or as text on the page and on a row, and under its own label.
      VIEW_CHECK(contrast(c, p.bg) >= 3.0 && contrast(c, p.panel) >= 3.0);
      VIEW_CHECK(contrast(onTier(theme), c) >= 3.0);
    }
    VIEW_CHECK(tierColor(theme, Tier(99)) == tierColor(theme, Tier::Beacon));
  }
  VIEW_CHECK(tierColor(Theme(7), Tier::Hacker) == tierColor(Theme::Bwm, Tier::Hacker));
  for (unsigned t = 0; t < TierCount; ++t)
    VIEW_CHECK(strlen(tierShort(Tier(t))) <= 7 && strlen(tierShort(Tier(t))) * 6 <= 58 - 8);
  VIEW_CHECK(!strcmp(tierShort(Tier(99)), "BEACON"));
}

inline void texts() {
  char out[32];
  // fit() never writes past its budget and marks a cut.
  rv::fit(out, sizeof(out), "Plate reader", 19);
  VIEW_CHECK(!strcmp(out, "Plate reader"));
  rv::fit(out, sizeof(out), "abcdefghij", 6);
  VIEW_CHECK(!strcmp(out, "abcd.."));
  rv::fit(out, 5, "abcdefghij", 19);
  VIEW_CHECK(!strcmp(out, "ab.."));
  rv::fit(out, sizeof(out), "abc", 1);
  VIEW_CHECK(!strcmp(out, "."));
  rv::fit(out, sizeof(out), "", 4);
  VIEW_CHECK(!*out);
  // Ages: at most four characters, and they read the way a person would say them.
  struct {
    uint32_t seconds;
    const char *age, *ago;
  } ages[] = {{0, "now", "just now"},
              {1, "now", "just now"},
              {2, "2s", "2s ago"},
              {59, "59s", "59s ago"},
              {60, "1m", "1m ago"},
              {3599, "59m", "59m ago"},
              {3600, "1h", "1h ago"},
              {172799, "47h", "47h ago"},
              {172800, "2d", "2d ago"},
              {86400u * 999, "999d", "999d ago"},
              {0xffffffffu, "999d", "999d ago"}};
  for (const auto &a : ages) {
    char age[8], ago[12];
    rv::ageText(age, a.seconds);
    rv::agoText(ago, a.seconds);
    VIEW_CHECK(!strcmp(age, a.age) && !strcmp(ago, a.ago) && strlen(age) <= 4 && strlen(ago) <= 8);
  }
  // The row title prefers a product label, but only one that says more and fits.
  ReconRow r;
  r.family = Family::Axon;
  char title[48];
  rv::rowTitle(title, sizeof(title), r, rv::HomeRowChars);
  VIEW_CHECK(!strcmp(title, "Body cam"));
  snprintf(r.label, sizeof(r.label), "Axon Body 3");
  rv::rowTitle(title, sizeof(title), r, rv::HomeRowChars);
  VIEW_CHECK(!strcmp(title, "Axon Body 3") && rv::labelAdds(r));
  rv::wideTitle(title, sizeof(title), r, 36);
  VIEW_CHECK(!strcmp(title, "Body cam  Axon Body 3"));
  snprintf(r.label, sizeof(r.label), "body CAM");
  VIEW_CHECK(!rv::labelAdds(r));
  for (const char *tag : {"following", "flood"}) { // Ledger tags are not product names.
    snprintf(r.label, sizeof(r.label), "%s", tag);
    rv::rowTitle(title, sizeof(title), r, rv::HomeRowChars);
    VIEW_CHECK(!rv::labelAdds(r) && !strcmp(title, "Body cam"));
  }
  snprintf(r.label, sizeof(r.label), "A label of 23 characte");
  rv::rowTitle(title, sizeof(title), r, rv::HomeRowChars);
  VIEW_CHECK(!strcmp(title, "Body cam")); // Too long for the home row: family name instead.
  rv::wideTitle(title, sizeof(title), r, 36);
  VIEW_CHECK(!strcmp(title, "Body cam  A label of 23 characte"));
  r.label[0] = 0;
  snprintf(r.name, sizeof(r.name), "AXON-X81234567");
  rv::wideTitle(title, sizeof(title), r, 36);
  VIEW_CHECK(!strcmp(title, "Body cam  AXON-X81234567"));
  // Every family's short name fits a home row and a filter cell.
  for (unsigned f = 0; f < FamilyCount; ++f)
    VIEW_CHECK(strlen(familyShort(Family(f))) <= rv::HomeRowChars &&
               strlen(familyName(Family(f))) <= 20);

  for (const ReconRow &row : worstRows()) {
    char left[40], right[12], reading[12], flag[12];
    rv::rowTitle(title, sizeof(title), row, rv::HomeRowChars);
    VIEW_CHECK(*title && strlen(title) <= rv::HomeRowChars);
    rv::wideTitle(title, sizeof(title), row, 36);
    VIEW_CHECK(*title && strlen(title) <= 36);
    rv::readingText(reading, row);
    VIEW_CHECK(*reading && strlen(reading) <= 8);
    rv::homeStatus(left, right, row);
    VIEW_CHECK(*left && strlen(left) <= 13 && strlen(right) <= 6);
    // Left text, a trend arrow (fresh rows only) and the count share one 19-character line.
    VIEW_CHECK(strlen(left) + (row.fresh ? 2 : 1) + strlen(right) <= rv::HomeRowChars);
    rv::listStatus(left, flag, row);
    VIEW_CHECK(strlen(left) <= 32 && strlen(flag) <= 9);
    VIEW_CHECK(strlen(rv::verdictWhy(row)) <= rv::DetailChars && *rv::verdictWhy(row));
    rv::DetailText t;
    rv::detailText(t, row);
    VIEW_CHECK(*t.title && strlen(t.title) <= 20);
    for (const auto &line : t.line)
      VIEW_CHECK(*line && strlen(line) <= rv::DetailChars);
    VIEW_CHECK(t.infoLines >= 1 && t.infoLines <= 3 && t.clueLines >= 1 && t.clueLines <= 2);
    for (int i = 0; i < t.infoLines; ++i)
      VIEW_CHECK(strlen(t.info[i]) <= rv::DetailChars);
    for (int i = 0; i < t.clueLines; ++i)
      VIEW_CHECK(strlen(t.clue[i]) <= rv::DetailChars);
    // The serial dump is one JSON object: quotes balance once escapes are skipped.
    char json[640];
    size_t n = rv::rowJson(json, sizeof(json), row, 255);
    VIEW_CHECK(n && n == strlen(json) && json[0] == '{' && json[n - 1] == '}');
    unsigned quotes = 0;
    for (size_t i = 0; i < n; ++i) {
      VIEW_CHECK(json[i] >= 32 && json[i] < 127);
      if (json[i] == '\\')
        ++i;
      else
        quotes += json[i] == '"';
    }
    VIEW_CHECK(quotes % 2 == 0);
    VIEW_CHECK(!rv::rowJson(json, 80, row, 0)); // Too small a buffer is refused, not cut.
  }
  // What each family is, in full: three lines of the device card, never cut.
  for (unsigned f = 0; f < FamilyCount; ++f) {
    char rows[4][64];
    VIEW_CHECK(rv::wrap(familyInfo(Family(f)), rv::DetailChars, rows, 4) <= 3);
    ReconRow row;
    row.family = Family(f);
    rv::DetailText t;
    rv::detailText(t, row);
    std::string joined;
    for (int i = 0; i < t.infoLines; ++i)
      joined += (i ? " " : "") + std::string(t.info[i]);
    VIEW_CHECK(joined == familyInfo(Family(f)));
  }
  // A cut wrap says so.
  char rows[2][64];
  VIEW_CHECK(rv::wrap("one two three four five six", 9, rows, 2) == 2 &&
             !strcmp(rows[0], "one two") && !strcmp(rows[1], "three.."));
  // The explanations match the engine's verdicts.
  ReconRow q;
  q.verdict = AlertVerdict::QuietRoad;
  VIEW_CHECK(std::string(rv::verdictWhy(q)).find("driving") != std::string::npos);
  q.verdict = AlertVerdict::BelowThreshold;
  VIEW_CHECK(std::string(rv::verdictWhy(q)).find("Listed") != std::string::npos);
  q.restored = true;
  VIEW_CHECK(std::string(rv::verdictWhy(q)).find("earlier boot") != std::string::npos);
  // The JSON carries what the bench needs.
  ReconRow j;
  j.family = Family::Flock;
  j.ble = false;
  j.rssi = -61;
  j.live = true;
  snprintf(j.name, sizeof(j.name), "say \"hi\"");
  char json[640];
  VIEW_CHECK(rv::rowJson(json, sizeof(json), j, 3));
  std::string text = json;
  VIEW_CHECK(text.find("\"i\":3,\"family\":\"flock\",\"tier\":\"Surveillance\"") == 1 &&
             text.find("\"name\":\"say \\\"hi\\\"\"") != std::string::npos &&
             text.find("\"rssi\":-61") != std::string::npos &&
             text.find("\"live\":true") != std::string::npos &&
             text.find("\"restored\":false}") != std::string::npos);
  j.ageKnown = false;
  VIEW_CHECK(rv::rowJson(json, sizeof(json), j, 3) &&
             std::string(json).find("\"age_s\":-1") != std::string::npos);
}

// The snapshot is a faithful copy of the table.
inline void snapshots() {
  std::vector<ReconDevice> store(40);
  ReconTable table;
  table.attach(store.data(), 40);
  AlertPolicy policy;
  AlertContext ctx;
  auto hear = [&](unsigned id, Family family, uint32_t at, int rssi, const char *label = "") {
    uint8_t mac[6] = {2, 0, 0, 0, uint8_t(id >> 8), uint8_t(id)};
    ReconSighting s;
    s.mac = mac;
    s.ble = true;
    s.addressType = 1;
    s.match = {family, Confidence::Medium, "test"};
    s.label = label;
    s.rssi = rssi;
    ReconSeen seen = table.observe(s, at);
    if (seen.row)
      policy.decide(*seen.row, seen, ctx, at);
    return seen.row;
  };
  ReconCounts counts;
  ReconHome home;
  const uint32_t now = 500000;
  table.counts(counts, now);
  reconHome(home, table, counts, now, 0);
  VIEW_CHECK(!home.count && !home.total && !home.live && !home.blipCount && home.topTier() == -1);

  hear(1, Family::Tracker, now - 5000, -70);
  hear(1, Family::Tracker, now - 1000, -60); // Ten dB closer two seconds later.
  hear(2, Family::Flock, now - 800, -50, "Flock Falcon");
  hear(3, Family::Flipper, now - 300000, -80); // Long gone.
  LedgerEntry entry;
  entry.family = Family::Ring;
  entry.confidence = Confidence::High;
  entry.mac[5] = 9;
  entry.rssi = -66;
  entry.utc = 800000000;
  entry.alerted = true;
  VIEW_CHECK(table.restore(entry));
  table.counts(counts, now);
  reconHome(home, table, counts, now, 800000600);
  VIEW_CHECK(home.count == 4 && home.total == 4 && home.live == 2 && home.blipCount == 2);
  VIEW_CHECK(home.topTier() == int(Tier::Surveillance) && home.liveTier[0] == 1 &&
             home.liveTier[unsigned(Tier::Tracker)] == 1);
  VIEW_CHECK(home.inRange == 2);
  // What is in range first, by tier; then what was heard most recently, whatever its tier.
  VIEW_CHECK(home.rows[0].family == Family::Flock && home.rows[0].live &&
             !strcmp(home.rows[0].label, "Flock Falcon") && home.rows[0].rssi == -50);
  VIEW_CHECK(home.rows[1].family == Family::Tracker && home.rows[1].trend == 1 &&
             home.rows[1].ageS == 1 && home.rows[1].firstS == 5 && home.rows[1].encounters == 1);
  // Out of range, recency decides: the Flipper that was here five minutes ago comes before a
  // camera from an earlier boot, although the camera is the higher tier.
  VIEW_CHECK(home.rows[2].family == Family::Flipper && !home.rows[2].live &&
             home.rows[2].ageS == 300 && home.rows[2].trend == 0);
  VIEW_CHECK(home.rows[3].family == Family::Ring && home.rows[3].restored &&
             home.rows[3].ageKnown && home.rows[3].ageS == 600 && home.rows[3].alerted);
  VIEW_CHECK(home.rows[0].fresh && home.rows[1].fresh && !home.rows[2].fresh);
  VIEW_CHECK(home.blips[0].tier == Tier::Surveillance && home.blips[0].rssi == -50 &&
             home.blips[1].tier == Tier::Tracker);
  VIEW_CHECK(sameDevice(home.rows[1], home.rows[1]) && !sameDevice(home.rows[0], home.rows[1]));
  // Without a clock a ledger row has no age to show.
  reconHome(home, table, counts, now, 0);
  VIEW_CHECK(!home.rows[3].ageKnown);
  reconHome(home, table, counts, now, 799999999); // A clock behind the ledger is not believed.
  VIEW_CHECK(!home.rows[3].ageKnown);
  {
    // With nothing in range the panel answers "what was just here": most recent first. Four
    // cameras from hours ago do not hide the Flipper that left two minutes ago.
    std::vector<ReconDevice> rows(40);
    ReconTable quiet;
    quiet.attach(rows.data(), 40);
    auto heard = [&](unsigned id, Family family, uint32_t at) {
      uint8_t mac[6] = {2, 0, 0, 1, 0, uint8_t(id)};
      ReconSighting s;
      s.mac = mac;
      s.match = {family, Confidence::Medium, "test"};
      s.rssi = -60;
      return quiet.observe(s, at).row;
    };
    const uint32_t t = 20000000;
    for (unsigned i = 0; i < 6; ++i)
      heard(i, Family::Flock, t - 7200000 - i * 1000);
    heard(10, Family::Flipper, t - 120000);
    heard(11, Family::Tile, t - 400000);
    ReconCounts c;
    quiet.counts(c, t);
    ReconHome h;
    reconHome(h, quiet, c, t, 0);
    VIEW_CHECK(h.count == 4 && !h.live && !h.inRange && !h.blipCount && h.total == 8);
    VIEW_CHECK(h.rows[0].family == Family::Flipper && h.rows[1].family == Family::Tile &&
               h.rows[2].family == Family::Flock && h.rows[3].family == Family::Flock);
    for (unsigned i = 1; i < h.count; ++i)
      VIEW_CHECK(h.rows[i - 1].ageS <= h.rows[i].ageS);
    VIEW_CHECK(!homeTopThreat(h));

    // The owner's own gear, ignored and in range, does not outrank a stranger's gadget, light
    // the chips or count in the Full Face chip. While driving only two rows are shown.
    ReconDevice *dashcam = heard(20, Family::Camera, t - 500);
    ReconDevice *doorbell = heard(21, Family::Ring, t - 400);
    heard(22, Family::Flipper, t - 300);
    heard(23, Family::Tile, t - 200);
    IgnoreList mine;
    mine.add(dashcam->mac, Family::Camera);
    mine.add(doorbell->mac, Family::Ring);
    quiet.applyIgnores(mine);
    quiet.counts(c, t);
    reconHome(h, quiet, c, t, 0);
    VIEW_CHECK(h.count == 4 && h.inRange == 4 && h.live == 2 && h.blipCount == 4);
    VIEW_CHECK(h.rows[0].family == Family::Flipper && h.rows[1].family == Family::Tile &&
               h.rows[2].ignored && h.rows[3].ignored &&
               tierOf(h.rows[2].family) == Tier::Surveillance);
    VIEW_CHECK(h.topTier() == int(Tier::Hacker) && !h.liveTier[unsigned(Tier::Surveillance)] &&
               h.liveTier[unsigned(Tier::Hacker)] == 1 && h.liveTier[unsigned(Tier::Tracker)] == 1);
    VIEW_CHECK(homeTopThreat(h) && homeTopThreat(h)->family == Family::Flipper);
    unsigned hollow = 0;
    for (unsigned i = 0; i < h.blipCount; ++i)
      hollow += h.blips[i].ignored;
    VIEW_CHECK(hollow == 2);
    // The LIVE tab uses the same order.
    static uint16_t scratch[40];
    ReconList live;
    reconList(live, quiet, rv::Tab::Live, 0, t, 0, scratch, 40);
    VIEW_CHECK(live.total == 4 && !live.rows[0].ignored && !live.rows[1].ignored &&
               live.rows[2].ignored && live.rows[3].ignored);
    // Only the owner's gear in range: nothing lit, nothing to remark on, "All quiet".
    mine.add(h.rows[0].mac, Family::Flipper);
    mine.add(h.rows[1].mac, Family::Tile);
    quiet.applyIgnores(mine);
    quiet.counts(c, t);
    reconHome(h, quiet, c, t, 0);
    VIEW_CHECK(h.inRange == 4 && !h.live && h.topTier() == -1 && !homeTopThreat(h) &&
               h.count == 4 && h.rows[0].ignored && h.rows[0].live);
  }
  {
    // A row is "live" for a minute but its reading is only current for ten seconds: after that
    // it shows how long ago it was heard, with no signal level and no closer/further arrow.
    std::vector<ReconDevice> rows(4);
    ReconTable t;
    t.attach(rows.data(), 4);
    uint8_t mac[6] = {2, 0, 0, 2, 0, 1};
    ReconSighting s;
    s.mac = mac;
    s.match = {Family::Alpr, Confidence::High, "test"};
    s.rssi = -70;
    t.observe(s, 1000);
    s.rssi = -47; // Passed at speed: much closer on the last packet.
    ReconDevice *d = t.observe(s, 4000).row;
    char reading[12], left[24], right[8];
    for (uint32_t age : {0u, 1u, 9u, 10u}) {
      ReconRow r = reconRow(*d, 4000 + age * 1000, 0);
      VIEW_CHECK(r.live && r.fresh && r.trend == 1);
      rv::readingText(reading, r);
      VIEW_CHECK(!strcmp(reading, "-47dB"));
      rv::DetailText text;
      rv::detailText(text, r);
      VIEW_CHECK(strstr(text.line[3], "dBm now") && strstr(text.line[3], "getting closer") &&
                 strstr(text.line[4], "in range now"));
    }
    for (uint32_t age : {11u, 30u, 55u, 59u}) {
      ReconRow r = reconRow(*d, 4000 + age * 1000, 0);
      VIEW_CHECK(r.live && !r.fresh && r.trend == 0 && r.ageS == age);
      rv::readingText(reading, r);
      VIEW_CHECK(!strstr(reading, "dB") && strstr(reading, "s ago") && strlen(reading) <= 8);
      rv::homeStatus(left, right, r);
      VIEW_CHECK(!strstr(left, "dB") && strstr(left, "s ago") && strlen(left) <= 13);
      rv::DetailText text;
      rv::detailText(text, r);
      VIEW_CHECK(!strstr(text.line[3], "now") && !strstr(text.line[3], "closer") &&
                 strstr(text.line[3], "s ago, peak -47") && !strstr(text.line[4], "in range now") &&
                 strstr(text.line[4], "| heard "));
      r.following = true;
      rv::homeStatus(left, right, r);
      VIEW_CHECK(!strcmp(left, "FOLLOWING") && !strstr(right, "dB") && strlen(right) <= 6);
    }
    ReconRow gone = reconRow(*d, 4000 + 60000, 0);
    VIEW_CHECK(!gone.live && !gone.fresh);
    VIEW_CHECK(ReconFreshS == 10 && ReconFreshS * 1000 < ReconLiveMs);
  }
  // A following tag carries its trail.
  ReconDevice &tag = *table.find(home.rows[1].mac, true, 1, Family::Tracker);
  tag.following = true;
  tag.pointCount = 3;
  tag.points[0] = {40.0f, -75.0f, now - 900000};
  tag.points[1] = {40.01f, -75.0f, now - 500000};
  tag.points[2] = {40.02f, -75.0f, now - 100000};
  tag.fixAt = now - 60000;
  ReconRow row = reconRow(tag, now, 0);
  VIEW_CHECK(row.following && row.places == 3 && row.followS == 840 && row.followMeters > 2100 &&
             row.followMeters < 2300);
  rv::DetailText t;
  rv::detailText(t, row);
  VIEW_CHECK(std::string(t.line[7]).find("FOLLOWING: 14 min across 3 places, 2.2 km") == 0);

  // More live rows than the home screen has room for: four rows, capped blips.
  for (unsigned i = 0; i < 30; ++i)
    hear(100 + i, i % 2 ? Family::Tile : Family::Camera, now - 100 - i, -40 - int(i));
  table.counts(counts, now);
  reconHome(home, table, counts, now, 0);
  VIEW_CHECK(home.count == rv::HomeRows && home.blipCount == rv::HomeBlips && home.live == 32 &&
             home.inRange == 32 && home.total == 34);
  for (unsigned i = 0; i < home.count; ++i)
    VIEW_CHECK(tierOf(home.rows[i].family) == Tier::Surveillance && home.rows[i].live);

  // The list pages: LIVE is only what is in range, SEEN is everything, newest first.
  static uint16_t order[40];
  ReconList list;
  reconList(list, table, rv::Tab::Live, 0, now, 0, order, 40);
  VIEW_CHECK(list.total == 32 && list.pages == 7 && list.page == 0 && list.count == 5);
  reconList(list, table, rv::Tab::Live, 6, now, 0, order, 40);
  VIEW_CHECK(list.page == 6 && list.count == 2 && list.rows[1].live);
  reconList(list, table, rv::Tab::Live, 99, now, 0, order, 40); // Past the end: the last page.
  VIEW_CHECK(list.page == 6 && list.count == 2);
  reconList(list, table, rv::Tab::Seen, 0, now, 0, order, 40);
  VIEW_CHECK(list.total == 34 && list.pages == 7 && list.count == 5 && list.rows[0].ageS == 0);
  for (unsigned i = 1; i < list.count; ++i)
    VIEW_CHECK(list.rows[i - 1].ageS <= list.rows[i].ageS);
  reconList(list, table, rv::Tab::Seen, 6, now, 0, order, 40);
  VIEW_CHECK(list.count == 4 && list.rows[3].restored && list.rows[2].family == Family::Flipper);
  table.clear();
  reconList(list, table, rv::Tab::Seen, 3, now, 0, order, 40);
  VIEW_CHECK(!list.total && list.pages == 1 && !list.page && !list.count);

  // Stats copy the lifetime counters and clamp the page.
  hear(1, Family::Tracker, now, -70);
  ReconStats stats;
  table.counts(counts, now);
  reconStats(stats, table, counts, 7, LedgerState::Failed, 99);
  VIEW_CHECK(stats.lifetime[unsigned(Family::Tracker)] == 2 &&
             stats.lifetime[unsigned(Family::Camera)] == 15 && stats.lifetimeTotal == 34 &&
             stats.ignoreList == 7 && stats.page == rv::StatPages - 1 &&
             stats.ledger == LedgerState::Failed && stats.counts.live == 1);
  Family sorted[FamilyCount - 1];
  rv::statOrder(sorted, stats.lifetime);
  std::set<Family> all(sorted, sorted + FamilyCount - 1);
  VIEW_CHECK(all.size() == FamilyCount - 1 && !all.count(Family::None));
  VIEW_CHECK(sorted[0] == Family::Camera && sorted[1] == Family::Tile);
  for (unsigned i = 1; i < FamilyCount - 1; ++i)
    VIEW_CHECK(stats.lifetime[unsigned(sorted[i - 1])] >= stats.lifetime[unsigned(sorted[i])]);
  VIEW_CHECK(strlen(ledgerStateName(LedgerState::NoCard)) <= 12 &&
             strlen(ledgerStateName(LedgerState::Failed)) <= 12 &&
             !strcmp(ledgerStateName(LedgerState::Ready), "OK"));
}

inline void paging() {
  VIEW_CHECK(rv::pageCount(0, 5) == 1 && rv::pageCount(1, 5) == 1 && rv::pageCount(5, 5) == 1 &&
             rv::pageCount(6, 5) == 2 && rv::pageCount(256, 5) == 52);
  VIEW_CHECK(rv::stepPage(0, 1, true) == 0 && rv::stepPage(0, 1, false) == 0 &&
             rv::stepPage(0, 0, true) == 0);
  VIEW_CHECK(rv::stepPage(0, 3, true) == 1 && rv::stepPage(2, 3, true) == 0 &&
             rv::stepPage(0, 3, false) == 2 && rv::stepPage(1, 3, false) == 0);
  VIEW_CHECK(rv::StatPages == 3 && rv::TypePages == 3); // 24 families, 10 and 8 to a page.
  VIEW_CHECK(rv::StatPages * rv::StatCells >= FamilyCount - 1 &&
             rv::TypePages * rv::TypeCells >= FamilyCount - 1);
}

// Home: the panel, the Full Face chip and the alert card.
inline void homeScreen() {
  // Rows and the radar stay inside the panel and clear of each other in every layout.
  for (bool recording : {false, true})
    for (unsigned shown = 1; shown <= rv::homeRowLimit(recording); ++shown) {
      std::vector<rv::Box> boxes;
      for (unsigned i = 0; i < shown; ++i)
        boxes.push_back(rv::homeRow(recording, shown, i));
      if (recording)
        boxes.push_back(rv::DriveBlock);
      else
        boxes.push_back(rv::homeRadar(shown));
      for (unsigned t = 0; t < TierCount; ++t)
        boxes.push_back(rv::tierChip(t));
      for (size_t i = 0; i < boxes.size(); ++i) {
        VIEW_CHECK(within(boxes[i], rv::Panel));
        for (size_t j = i + 1; j < boxes.size(); ++j)
          VIEW_CHECK(apart(boxes[i], boxes[j]));
      }
    }
  VIEW_CHECK(within(rv::homeRadar(0), rv::Panel) && within(rv::FaceChip, rv::FaceChipZone));
  VIEW_CHECK(rv::homeRowLimit(false) == 4 && rv::homeRowLimit(true) == 2);
  // The chip sits in the stage's bottom right: clear of the feeling tag (ends x 110, y 45),
  // the Full Face bubble (ends y 70) and the drive counters (end x 243).
  VIEW_CHECK(rv::FaceChip.x >= 244 && rv::FaceChip.y >= 71 &&
             rv::FaceChip.y + rv::FaceChip.h <= 175);
  for (const char *message : {"x", "Something just showed up. I wrote it down.",
                              "A message that is long enough to need all of the three rows a "
                              "bubble can have on the screen."}) {
    homeui::BubbleBox b;
    VIEW_CHECK(homeui::layoutBubble(message, true, b) && b.tipY < rv::FaceChipZone.y);
  }
  VIEW_CHECK(within(rv::AlertDetails, rv::AlertDetailsZone) &&
             within(rv::AlertDetailsZone, rv::AlertCard) && rv::AlertCard.y == 178);

  // Every pixel of the home screen goes to the control drawn there.
  for (int mode = 0; mode < 16; ++mode)
    for (unsigned rows = 0; rows <= 5; ++rows) {
      HomeTapState s;
      s.face = mode & 1;
      s.alert = mode & 2;
      s.demo = (mode & 4) && s.alert;
      s.recording = mode & 8;
      s.rows = rows;
      unsigned limit = rv::homeRowLimit(s.recording), shown = rows < limit ? rows : limit;
      for (int y = 0; y < 240; ++y)
        for (int x = 0; x < 320; ++x) {
          HomeTapTarget t = homeTap(x, y, s);
          HomeTap want = HomeTap::None;
          unsigned row = 0;
          if (y >= 208 && y < 236)
            want = x >= 244            ? HomeTap::Menu
                   : x >= 168          ? HomeTap::SwitchView
                   : x >= 8 && x < 162 ? HomeTap::Drive
                                       : HomeTap::None;
          else if (s.alert && !s.demo && rv::AlertDetailsZone.has(x, y))
            want = HomeTap::AlertDetail;
          else if (!s.face && rv::Panel.has(x, y)) {
            // A row owns its whole 22 px pitch. While an alert is open the panel only
            // dismisses: no device card opens under a tap meant for the alert.
            want = s.alert ? HomeTap::Dismiss : HomeTap::Recon;
            for (unsigned i = 0; i < shown && !s.alert; ++i)
              if (rv::homeRowZone(s.recording, shown, i).has(x, y)) {
                want = HomeTap::Row;
                row = i;
              }
          } else if (s.face && !s.alert && rv::FaceChipZone.has(x, y))
            want = HomeTap::Recon;
          else if (y >= 178)
            want = s.alert ? HomeTap::Dismiss : HomeTap::Stats;
          else if (y >= 28 && y < 176 && (s.face || x < 182))
            want = s.alert ? HomeTap::Dismiss : HomeTap::Pet;
          VIEW_CHECK(t.action == want && t.row == row);
        }
    }
  // Spot checks that read like the screen.
  HomeTapState s;
  VIEW_CHECK(homeTap(250, 40, s).action == HomeTap::Recon);  // Header.
  VIEW_CHECK(homeTap(250, 100, s).action == HomeTap::Recon); // The radar.
  s.rows = 4;
  VIEW_CHECK(homeTap(250, 85, s).action == HomeTap::Row && homeTap(250, 85, s).row == 0 &&
             homeTap(250, 150, s).row == 3 && homeTap(250, 170, s).action == HomeTap::Recon);
  s.rows = 1; // One row sits low, under the big radar.
  VIEW_CHECK(homeTap(250, 85, s).action == HomeTap::Recon &&
             homeTap(250, 130, s).action == HomeTap::Row);
  s.rows = 4; // Neighbouring rows meet: the pixel row between two cards belongs to the upper one.
  VIEW_CHECK(homeTap(250, 98, s).action == HomeTap::Row && homeTap(250, 98, s).row == 0 &&
             homeTap(250, 99, s).row == 1);
  for (bool recording : {false, true})
    for (unsigned shown = 1; shown <= rv::homeRowLimit(recording); ++shown)
      for (unsigned i = 0; i < shown; ++i) {
        rv::Box zone = rv::homeRowZone(recording, shown, i);
        VIEW_CHECK(within(rv::homeRow(recording, shown, i), zone) && within(zone, rv::Panel) &&
                   zone.h == rv::RowPitch);
        if (i)
          VIEW_CHECK(apart(zone, rv::homeRowZone(recording, shown, i - 1)));
        if (recording)
          VIEW_CHECK(apart(zone, rv::DriveBlock));
      }
  s.rows = 1;
  s.alert = true; // The whole panel dismisses, rows included; DETAILS is the way to the device.
  VIEW_CHECK(homeTap(250, 130, s).action == HomeTap::Dismiss &&
             homeTap(250, 40, s).action == HomeTap::Dismiss &&
             homeTap(90, 100, s).action == HomeTap::Dismiss &&
             homeTap(100, 190, s).action == HomeTap::Dismiss &&
             homeTap(280, 190, s).action == HomeTap::AlertDetail);
  s.demo = true; // The demo has no device behind it.
  VIEW_CHECK(homeTap(280, 190, s).action == HomeTap::Dismiss);
  s = {};
  s.face = true;
  VIEW_CHECK(homeTap(280, 162, s).action == HomeTap::Recon &&
             homeTap(200, 162, s).action == HomeTap::Pet &&
             homeTap(250, 100, s).action == HomeTap::Pet);

  // Drawing: every state keeps its text inside the panel, with nothing overlapping.
  std::vector<ReconRow> worst = worstRows();
  for (Theme theme : {Theme::Dark, Theme::Light})
    for (bool recording : {false, true})
      for (unsigned count = 0; count <= rv::HomeRows; ++count)
        for (size_t first = 0; first + rv::HomeRows <= worst.size(); first += 3) {
          ReconHome h;
          h.count = count;
          for (unsigned i = 0; i < count; ++i) {
            h.rows[i] = worst[first + i];
            h.live += h.rows[i].live;
          }
          h.total = first % 2 ? count : 65535;
          for (unsigned t = 0; t < TierCount; ++t)
            h.liveTier[t] = uint16_t(first % 3 == 0 ? 65535 : first % 3 == 1 ? 7 : 0);
          rv::ReconPanel panel;
          panel.style = style(theme);
          panel.recording = recording;
          panel.wifiSaved = panel.bleSaved = first % 2 ? 0xffffffffu : 7;
          Ink ink;
          rv::drawHomePanel(ink, panel, h);
          VIEW_CHECK(tidy(ink, rv::Panel) && ink.has("RECON"));
          unsigned limit = rv::homeRowLimit(recording);
          VIEW_CHECK(ink.has("more") == (h.total > (count < limit ? count : limit) && count > 0));
          if (!count) // The empty states.
            VIEW_CHECK(recording ? ink.has("No threats heard") : ink.has("All quiet"));
          VIEW_CHECK(ink.has("THIS DRIVE") == recording);
          if (count == 0 && first > 6)
            break;
        }
  // One row's text stays inside that row.
  for (const ReconRow &row : worst)
    for (bool recording : {false, true}) {
      Ink ink;
      rv::Box box = rv::homeRow(recording, 4, 3);
      rv::homeRowCard(ink, style(), box, row);
      VIEW_CHECK(ink.runs.size() >= 2 && tidy(ink, box));
    }
  // The chip: its count is capped so "RECON 99" always fits.
  for (unsigned live : {0u, 7u, 99u, 100u, 65535u}) {
    ReconHome h;
    h.live = live;
    h.liveTier[unsigned(Tier::Tracker)] = uint16_t(live);
    Ink ink;
    rv::drawFaceChip(ink, style(), h);
    VIEW_CHECK(ink.runs.size() == 1 && tidy(ink, rv::FaceChip));
    VIEW_CHECK(h.topTier() == (live ? int(Tier::Tracker) : -1));
  }
  // The alert card: two lines left of DETAILS, for every family, label and kind.
  for (unsigned f = 0; f < FamilyCount; ++f)
    for (uint8_t kind = 0; kind < 3; ++kind)
      for (const char *label : {"", "WWWWWWWWWWWWWWWWWWWWWWW", "following"})
        for (bool demo : {false, true}) {
          rv::AlertCardView a;
          a.kind = kind;
          a.demo = demo;
          a.family = Family(f);
          a.confidence = Confidence::Medium;
          a.label = label;
          a.rssi = -200;
          a.followS = a.followMeters = 0xffffffffu;
          char title[48], detail[48];
          rv::alertCardText(title, detail, a);
          VIEW_CHECK(strlen(title) <= rv::AlertCardChars && strlen(detail) <= rv::AlertCardChars);
          Ink ink;
          rv::drawAlertCard(ink, style(), a);
          VIEW_CHECK(tidy(ink, rv::AlertCard) && ink.has("DETAILS") == !demo);
          for (const auto &run : ink.runs)
            VIEW_CHECK(run.text == "DETAILS" || run.x + run.w <= rv::AlertDetails.x);
        }
  rv::AlertCardView a;
  char title[48], detail[48];
  a.kind = 1;
  a.family = Family::Tracker;
  a.rssi = -71;
  rv::alertCardText(title, detail, a);
  VIEW_CHECK(!strcmp(title, "Find My / tracker  -71dB") &&
             !strcmp(detail, "Tracker flood, probably spoofed"));
  a.kind = 2;
  a.followS = 19 * 60 + 57;
  a.followMeters = 4460;
  rv::alertCardText(title, detail, a);
  VIEW_CHECK(!strcmp(detail, "With us for 19 min / 4.4 km"));
  a = {};
  a.family = Family::Alpr;
  a.confidence = Confidence::High;
  a.label = "Motorola Vigilant";
  a.rssi = -49;
  rv::alertCardText(title, detail, a);
  VIEW_CHECK(!strcmp(title, "Motorola Vigilant  -49dB") &&
             !strcmp(detail, "Surveillance | high | tap to dismiss"));
  // The radar: every blip lands inside its dial, and an address keeps its bearing.
  for (unsigned shown : {0u, 1u, 2u, 3u, 4u}) {
    rv::Box dial = rv::homeRadar(shown);
    for (unsigned angle = 0; angle < 256; ++angle)
      for (int rssi : {-128, -95, -70, -40, 0, 127}) {
        int x, y;
        rv::blipPoint(dial, {uint8_t(angle), int8_t(rssi), Tier::Tracker, false}, x, y);
        VIEW_CHECK(x - 3 >= dial.x && x + 3 < dial.x + dial.w && y - 3 >= dial.y &&
                   y + 3 < dial.y + dial.h);
      }
  }
  uint8_t one[6] = {1, 2, 3, 4, 5, 6}, other[6] = {1, 2, 3, 4, 5, 7};
  VIEW_CHECK(rv::addressAngle(one) == rv::addressAngle(one) &&
             rv::addressAngle(one) != rv::addressAngle(other));
  int nearX, nearY, farX, farY;
  rv::Box dial = rv::homeRadar(0);
  rv::blipPoint(dial, {0, -40, Tier::Tracker, false}, nearX, nearY);
  rv::blipPoint(dial, {0, -95, Tier::Tracker, false}, farX, farY);
  VIEW_CHECK(nearX < farX && nearX > dial.x + dial.w / 2); // Stronger is nearer the centre.
}

// The Recon page, the device card, and the two settings pages.
inline void pages() {
  using rv::ListHit;
  // Boxes: on the screen, under the heading, apart, and big enough to tap.
  std::vector<rv::Box> list{rv::Prev, rv::Next};
  for (unsigned i = 0; i < rv::Tabs; ++i)
    list.push_back(rv::tab(i));
  for (unsigned i = 0; i < rv::ListRows; ++i)
    list.push_back(rv::listRow(i));
  std::vector<rv::Box> statBoxes{rv::Prev, rv::Next};
  for (unsigned i = 0; i < rv::Tabs; ++i)
    statBoxes.push_back(rv::tab(i));
  for (unsigned t = 0; t < TierCount; ++t)
    statBoxes.push_back(rv::tierBox(t));
  for (unsigned i = 0; i < rv::StatCells; ++i)
    statBoxes.push_back(rv::statCell(i));
  std::vector<rv::Box> settings{rv::NotifyButton,    rv::FamilyQuietButton, rv::RemindersButton,
                                rv::QuietRoadButton, rv::SnoozeButton,      rv::TypesButton,
                                rv::IgnoredLabel,    rv::ClearButton};
  std::vector<rv::Box> types{rv::Prev, rv::Next};
  for (unsigned t = 0; t < TierCount; ++t)
    types.push_back(rv::tierToggle(t));
  for (unsigned i = 0; i < rv::TypeCells; ++i)
    types.push_back(rv::typeCell(i));
  std::vector<rv::Box> card{rv::IgnoreButton, rv::CloseButton};
  for (const auto *set : {&list, &statBoxes, &settings, &types, &card})
    for (size_t i = 0; i < set->size(); ++i) {
      VIEW_CHECK(within((*set)[i], Body) && apart((*set)[i], rv::BackZone));
      for (size_t j = i + 1; j < set->size(); ++j)
        VIEW_CHECK(apart((*set)[i], (*set)[j]));
    }
  for (const auto *set : {&list, &settings, &types, &card})
    for (const rv::Box &b : *set)
      VIEW_CHECK(b.w >= 58 && b.h >= 22); // Finger sized.
  VIEW_CHECK(within(rv::BackButton, rv::BackZone));
  // What a finger has to hit is at least 27 px tall and wraps the control it belongs to. The
  // tab and tier-toggle zones stop one pixel row short of the BACK zone, so a slightly high tap
  // on STATS or BEACON does nothing rather than leave the page.
  for (unsigned i = 0; i < rv::Tabs; ++i) {
    rv::Box zone = rv::tabZone(i);
    VIEW_CHECK(within(rv::tab(i), zone) && zone.h >= 28 && apart(zone, rv::BackZone) &&
               zone.y > rv::BackZone.y + rv::BackZone.h && apart(zone, rv::listRowZone(0)) &&
               apart(zone, rv::tierBox(0)));
    if (i)
      VIEW_CHECK(apart(zone, rv::tabZone(i - 1)));
  }
  for (unsigned t = 0; t < TierCount; ++t) {
    rv::Box zone = rv::tierToggleZone(t);
    VIEW_CHECK(within(rv::tierToggle(t), zone) && zone.h >= 28 && apart(zone, rv::BackZone) &&
               zone.y > rv::BackZone.y + rv::BackZone.h);
    for (unsigned i = 0; i < rv::TypeCells; ++i)
      VIEW_CHECK(apart(zone, rv::typeCell(i)));
    if (t)
      VIEW_CHECK(apart(zone, rv::tierToggleZone(t - 1)));
  }
  for (unsigned i = 0; i < rv::ListRows; ++i) {
    rv::Box zone = rv::listRowZone(i);
    VIEW_CHECK(within(rv::listRow(i), zone) && zone.h == 27 && apart(zone, rv::Prev) &&
               apart(zone, rv::Next));
    if (i)
      VIEW_CHECK(apart(zone, rv::listRowZone(i - 1)));
  }
  VIEW_CHECK(rv::listTap(280, 35, 5, 1).hit == ListHit::Tab &&
             rv::listTap(280, 35, 5, 1).index == 2 &&
             rv::listTap(280, 34, 5, 1).hit == ListHit::None &&
             rv::listTap(280, 33, 5, 1).hit == ListHit::Back);
  VIEW_CHECK(rv::listTap(100, 88, 5, 1).hit == ListHit::Row &&
             rv::listTap(100, 88, 5, 1).index == 0 && rv::listTap(100, 89, 5, 1).index == 0 &&
             rv::listTap(100, 90, 5, 1).index == 1);
  VIEW_CHECK(rv::typesTap(280, 35, 0).hit == rv::TypesHit::Tier &&
             rv::typesTap(280, 35, 0).index == 4 &&
             rv::typesTap(280, 34, 0).hit == rv::TypesHit::None);

  // An alert that opens over any other page: its card takes the next tap, wherever it lands.
  for (int y = 0; y < 240; ++y)
    for (int x = 0; x < 320; ++x) {
      VIEW_CHECK(rv::overlayTap(x, y, false, false) == rv::OverlayTap::Pass &&
                 rv::overlayTap(x, y, false, true) == rv::OverlayTap::Pass);
      VIEW_CHECK(
          rv::overlayTap(x, y, true, false) ==
          (rv::AlertDetailsZone.has(x, y) ? rv::OverlayTap::Details : rv::OverlayTap::Dismiss));
      VIEW_CHECK(rv::overlayTap(x, y, true, true) == rv::OverlayTap::Dismiss); // No device.
    }
  // The card covers Detection's CLEAR ALL row and the foot of the device card, which is why
  // the tap is never passed on to the page while the alert is open.
  VIEW_CHECK(!apart(rv::AlertCard, rv::ClearButton) &&
             rv::overlayTap(rv::ClearButton.x + 70, rv::ClearButton.y + 10, true, false) !=
                 rv::OverlayTap::Pass &&
             rv::overlayTap(280, 20, true, false) == rv::OverlayTap::Dismiss); // BACK too.

  // Recon page: every pixel, for every number of rows and pages.
  for (unsigned rows = 0; rows <= rv::ListRows + 1; ++rows)
    for (unsigned pageTotal : {1u, 2u}) {
      for (int y = 0; y < 240; ++y)
        for (int x = 0; x < 320; ++x) {
          rv::ListTarget t = rv::listTap(x, y, rows, pageTotal);
          ListHit want = ListHit::None;
          unsigned index = 0;
          if (rv::BackZone.has(x, y))
            want = ListHit::Back;
          for (unsigned i = 0; i < rv::Tabs; ++i)
            if (rv::tabZone(i).has(x, y)) {
              want = ListHit::Tab;
              index = i;
            }
          for (unsigned i = 0; i < rows && i < rv::ListRows; ++i)
            if (rv::listRowZone(i).has(x, y)) {
              want = ListHit::Row;
              index = i;
            }
          if (pageTotal > 1 && rv::Prev.has(x, y))
            want = ListHit::Prev;
          if (pageTotal > 1 && rv::Next.has(x, y))
            want = ListHit::Next;
          VIEW_CHECK(t.hit == want && t.index == index);
        }
    }
  // Device card and settings: likewise.
  for (int y = 0; y < 240; ++y)
    for (int x = 0; x < 320; ++x) {
      rv::DetailHit d = rv::detailTap(x, y);
      VIEW_CHECK(d == (rv::BackZone.has(x, y)       ? rv::DetailHit::Back
                       : rv::IgnoreButton.has(x, y) ? rv::DetailHit::Ignore
                       : rv::CloseButton.has(x, y)  ? rv::DetailHit::Close
                                                    : rv::DetailHit::None));
      rv::SettingsHit h = rv::settingsTap(x, y);
      rv::SettingsHit want = rv::BackZone.has(x, y)            ? rv::SettingsHit::Back
                             : rv::NotifyButton.has(x, y)      ? rv::SettingsHit::Notify
                             : rv::FamilyQuietButton.has(x, y) ? rv::SettingsHit::FamilyQuiet
                             : rv::RemindersButton.has(x, y)   ? rv::SettingsHit::Reminders
                             : rv::QuietRoadButton.has(x, y)   ? rv::SettingsHit::QuietRoad
                             : rv::SnoozeButton.has(x, y)      ? rv::SettingsHit::Snooze
                             : rv::TypesButton.has(x, y)       ? rv::SettingsHit::Types
                             : rv::ClearButton.has(x, y)       ? rv::SettingsHit::Clear
                                                               : rv::SettingsHit::None;
      VIEW_CHECK(h == want);
      VIEW_CHECK(!rv::IgnoredLabel.has(x, y) || h == rv::SettingsHit::None); // A label only.
    }
  // The type filter: every family is on exactly one page, in tier order.
  Family order[FamilyCount - 1];
  rv::typeOrder(order);
  std::set<Family> placed(order, order + FamilyCount - 1);
  VIEW_CHECK(placed.size() == FamilyCount - 1 && !placed.count(Family::None));
  for (unsigned i = 1; i < FamilyCount - 1; ++i)
    VIEW_CHECK(tierOf(order[i - 1]) <= tierOf(order[i]));
  std::set<Family> reachable;
  for (unsigned page = 0; page < rv::TypePages; ++page)
    for (int y = 0; y < 240; ++y)
      for (int x = 0; x < 320; ++x) {
        rv::TypesTarget t = rv::typesTap(x, y, page);
        rv::TypesHit want = rv::TypesHit::None;
        unsigned index = 0;
        Family family = Family::None;
        if (rv::BackZone.has(x, y))
          want = rv::TypesHit::Back;
        for (unsigned tier = 0; tier < TierCount; ++tier)
          if (rv::tierToggleZone(tier).has(x, y)) {
            want = rv::TypesHit::Tier;
            index = tier;
          }
        for (unsigned i = 0; i < rv::TypeCells; ++i)
          if (rv::typeCell(i).has(x, y) && page * rv::TypeCells + i < FamilyCount - 1) {
            want = rv::TypesHit::Family;
            family = order[page * rv::TypeCells + i];
          }
        if (rv::Prev.has(x, y))
          want = rv::TypesHit::Prev;
        if (rv::Next.has(x, y))
          want = rv::TypesHit::Next;
        VIEW_CHECK(t.hit == want && t.index == index && t.family == family);
        if (t.hit == rv::TypesHit::Family)
          reachable.insert(t.family);
      }
  VIEW_CHECK(reachable == placed);
  // Tier toggles: all on, unless all already are.
  uint32_t masks = 0;
  for (unsigned t = 0; t < TierCount; ++t) {
    uint32_t mask = rv::tierMask(Tier(t));
    VIEW_CHECK(mask && !(mask & masks) && !(mask & 1));
    masks |= mask;
    VIEW_CHECK(rv::tierState(TypeFilter::All, Tier(t)) == 2 && rv::tierState(0, Tier(t)) == 0);
    uint32_t off = rv::toggleTier(TypeFilter::All, Tier(t));
    VIEW_CHECK(off == (TypeFilter::All & ~mask) && rv::tierState(off, Tier(t)) == 0);
    VIEW_CHECK(rv::toggleTier(off, Tier(t)) == TypeFilter::All);
  }
  VIEW_CHECK(masks == TypeFilter::All);
  uint32_t partial = TypeFilter::All & ~TypeFilter::flag(Family::Ring);
  VIEW_CHECK(rv::tierState(partial, Tier::Surveillance) == 1 &&
             rv::toggleTier(partial, Tier::Surveillance) == TypeFilter::All);
  VIEW_CHECK(rv::tierState(TypeFilter::Defaults, Tier::Beacon) == 0); // Beacons start off.
  {
    // The Types page edits a draft. One slip on a tier (which would drop every listed row of
    // it once in force) is undone by the next tap, and nothing but tier and family taps edit.
    const uint32_t saved = TypeFilter::Defaults;
    rv::TypesTarget tier{rv::TypesHit::Tier, unsigned(Tier::Surveillance)};
    uint32_t draft = rv::typesEdit(saved, tier);
    VIEW_CHECK(draft != saved && rv::tierState(draft, Tier::Surveillance) == 0);
    VIEW_CHECK(rv::typesEdit(draft, tier) == saved); // Back where it started: nothing to apply.
    rv::TypesTarget ring{rv::TypesHit::Family, 0, Family::Ring};
    draft = rv::typesEdit(saved, ring);
    VIEW_CHECK(draft == (saved & ~TypeFilter::flag(Family::Ring)) &&
               rv::typesEdit(draft, ring) == saved);
    for (rv::TypesHit other :
         {rv::TypesHit::None, rv::TypesHit::Back, rv::TypesHit::Prev, rv::TypesHit::Next})
      VIEW_CHECK(rv::typesEdit(saved, {other, 0, Family::Ring}) == saved);
    VIEW_CHECK(rv::typesEdit(0xffffffffu, {}) == TypeFilter::All);
  }
  // The cycles offer exactly what the engine accepts.
  VIEW_CHECK(rv::nextSnooze(0) == 5 && rv::nextSnooze(5) == 10 && rv::nextSnooze(10) == 0 &&
             rv::nextSnooze(7) == 10 && rv::nextSnooze(200) == 0);
  for (uint8_t v : {uint8_t(0), uint8_t(5), uint8_t(10)})
    VIEW_CHECK(autoSnoozeChoice(rv::nextSnooze(v)) == rv::nextSnooze(v));
  VIEW_CHECK(rv::nextFamilyQuiet(60000) == 180000 && rv::nextFamilyQuiet(180000) == 300000 &&
             rv::nextFamilyQuiet(300000) == 60000 && rv::nextFamilyQuiet(1) == 60000);

  // Drawing. Text stays on the page, inside the margins, and nothing overlaps.
  std::vector<ReconRow> worst = worstRows();
  for (Theme theme : {Theme::Dark, Theme::Light, Theme::Matrix, Theme::Bwm}) {
    rv::Style s = style(theme);
    for (rv::Tab tab : {rv::Tab::Live, rv::Tab::Seen})
      for (size_t first = 0; first + rv::ListRows <= worst.size(); first += 5) {
        ReconList rows;
        rows.count = rv::ListRows;
        for (unsigned i = 0; i < rv::ListRows; ++i)
          rows.rows[i] = worst[first + i];
        rows.total = 65535;
        rows.pages = 13107;
        rows.page = 13106;
        Ink ink;
        rv::drawReconList(ink, s, tab, rows, {});
        VIEW_CHECK(tidy(ink, Screen) && ink.has("LIVE") && ink.has("SEEN") && ink.has("STATS") &&
                   ink.has("PREV") && ink.has("13107 / 13107"));
      }
    for (const ReconRow &row : worst) {
      Ink ink;
      rv::listRowCard(ink, s, rv::listRow(2), row);
      VIEW_CHECK(ink.runs.size() >= 3 && tidy(ink, rv::listRow(2)));
      for (bool present : {true, false}) {
        Ink page;
        rv::drawReconDetail(page, s, row, present);
        VIEW_CHECK(tidy(page, Screen) && page.has("DEVICE") && page.has("BACK"));
        VIEW_CHECK(page.has("NO LONGER LISTED") == !present);
        VIEW_CHECK(page.has(row.ignored ? "UNIGNORE" : "IGNORE (IT'S MINE)") == present);
        // With the ignore list full the button says so instead of being a dead tap. A device
        // already on the list can still be taken off.
        Ink full;
        rv::drawReconDetail(full, s, row, present, true);
        VIEW_CHECK(
            tidy(full, Screen) && full.has("IGNORE LIST FULL") == (present && !row.ignored) &&
            full.has("UNIGNORE") == (present && row.ignored) && !full.has("IGNORE (IT'S MINE)"));
        for (const auto &run : full.runs)
          if (rv::IgnoreButton.has(run.x + run.w / 2, run.y + 4))
            VIEW_CHECK(run.x >= rv::IgnoreButton.x + 3 &&
                       run.x + run.w <= rv::IgnoreButton.x + rv::IgnoreButton.w - 3);
        for (const auto &run : page.runs) // Nothing runs into the buttons or off the margins.
          VIEW_CHECK(run.x >= 8 && run.x + run.w <= 312 &&
                     (run.y + run.h <= rv::IgnoreButton.y || run.y >= rv::IgnoreButton.y));
      }
    }
    // Empty states say what is true: everything classified is listed, alert filter or not,
    // except the types the owner has switched off.
    for (bool cardIn : {true, false})
      for (bool radio : {true, false})
        for (unsigned seen : {0u, 1u, 256u}) {
          rv::ListContext ctx;
          ctx.card = cardIn;
          ctx.radio = radio;
          ctx.seenTotal = seen;
          Ink live, all;
          rv::drawReconList(live, s, rv::Tab::Live, ReconList(), ctx);
          rv::drawReconList(all, s, rv::Tab::Seen, ReconList(), ctx);
          VIEW_CHECK(tidy(live, Screen) && tidy(all, Screen));
          VIEW_CHECK(live.has(radio ? "Nothing sketchy in range" : "radios are not running"));
          VIEW_CHECK(live.has("whatever your") && live.has("Switched-off types are not") &&
                     live.has("under SEEN") == (seen > 0));
          VIEW_CHECK(live.has("1 earlier device is") == (seen == 1));
          VIEW_CHECK(all.has("No classified devices yet") &&
                     all.has(cardIn ? "saved to the SD card" : "until power off"));
          for (const Ink *ink : {&live, &all}) {
            VIEW_CHECK(!ink->has("alert filter yet") && !ink->has("PREV") && ink->has("1 / 1"));
            for (const auto &run : ink->runs)
              VIEW_CHECK(run.x >= 8 && run.x + run.w <= 312);
          }
        }
    // STATS with the largest numbers the counters can hold.
    for (unsigned page = 0; page < rv::StatPages; ++page)
      for (LedgerState ledger : {LedgerState::NoCard, LedgerState::Ready, LedgerState::Failed}) {
        ReconStats stats;
        for (unsigned f = 1; f < FamilyCount; ++f)
          stats.lifetime[f] = f % 3 ? 0xffffffffu : 0;
        stats.lifetimeTotal = 0xffffffffu;
        for (unsigned t = 0; t < TierCount; ++t)
          stats.counts.liveTier[t] = stats.counts.totalTier[t] = 256;
        stats.counts.followingLive = 256;
        stats.ignoreList = IgnoreList::Capacity;
        stats.ledger = ledger;
        stats.page = page;
        Ink ink;
        rv::drawReconStats(ink, s, stats);
        VIEW_CHECK(tidy(ink, Screen) && ink.has(ledgerStateName(ledger)) && ink.has("256/256"));
        for (const auto &run : ink.runs)
          VIEW_CHECK(run.x >= 8 && run.x + run.w <= 312);
      }
    // Settings, in every combination that changes a label.
    for (int bits = 0; bits < 32; ++bits) {
      rv::SettingsView v;
      v.threshold = Confidence(1 + bits % 3);
      v.familyQuietMs = bits & 1 ? 300000 : 60000;
      v.repeat = bits & 2;
      v.quietRoad = bits & 4;
      v.autoSnooze = bits & 8 ? 10 : bits & 1 ? 5 : 0;
      v.ignored = bits & 16 ? IgnoreList::Capacity : bits & 1;
      v.confirmClear = bits & 2;
      v.typeFilter = bits & 4 ? TypeFilter::All : 0;
      Ink ink;
      rv::drawReconSettings(ink, s, v);
      VIEW_CHECK(tidy(ink, Screen) && ink.has("DETECTION") && ink.has("clue, not proof"));
      // The note must not promise that everything is listed: switched-off types are not.
      VIEW_CHECK(ink.has("Types switched off are not listed") && !ink.has("Everything classified"));
      // Auto snooze counts alerts, and says so beside settings measured in seconds and minutes.
      VIEW_CHECK(ink.has(v.autoSnooze == 10  ? "Auto snooze: 10 alerts"
                         : v.autoSnooze == 5 ? "Auto snooze: 5 alerts"
                                             : "Auto snooze: OFF") &&
                 ink.has(v.quietRoad ? "Quiet road: ON" : "Quiet road: OFF"));
      VIEW_CHECK(ink.has(!v.ignored       ? "NONE TO CLEAR"
                         : v.confirmClear ? "TAP AGAIN: CLEAR"
                                          : "CLEAR ALL"));
      VIEW_CHECK(ink.has(bits & 4 ? "24 of 24 on" : "0 of 24 on"));
      // Each button's label fits inside that button.
      for (const auto &run : ink.runs) {
        VIEW_CHECK(run.x >= 8 && run.x + run.w <= 312);
        for (const rv::Box &b : settings)
          if (b.has(run.x + run.w / 2, run.y + 4))
            VIEW_CHECK(run.x >= b.x + 3 && run.x + run.w <= b.x + b.w - 3);
      }
    }
    for (unsigned page = 0; page < rv::TypePages; ++page)
      for (uint32_t filter : {uint32_t(0), TypeFilter::Defaults, TypeFilter::All, partial}) {
        Ink ink;
        rv::drawReconTypes(ink, s, filter, page);
        VIEW_CHECK(tidy(ink, Screen) && ink.has("DEVICE TYPES") && ink.has("PREV") &&
                   !ink.has("BACK applies"));
        Ink edited; // An edit that is not in force yet says how to apply it.
        rv::drawReconTypes(edited, s, filter, page, true);
        VIEW_CHECK(tidy(edited, Screen) && edited.has("BACK applies"));
        for (const auto &run : edited.runs)
          VIEW_CHECK(run.y > rv::BackZone.h || run.x + run.w <= rv::BackButton.x ||
                     run.x >= rv::BackButton.x);
        for (const auto &run : ink.runs)
          for (const rv::Box &b : types)
            if (b.has(run.x + run.w / 2, run.y + 4))
              VIEW_CHECK(run.x >= b.x + 3 && run.x + run.w <= b.x + b.w - 3);
      }
  }
}

// The firmware pieces this feature added outside recon_view.h.
inline void wiring() {
  // A label goes when another rule's match replaces the one it described.
  char label[LabelChars] = "Axon Body 3";
  Match match{Family::Axon, Confidence::Medium, "signature"};
  promoteLabelled(match, label, {Family::Watchlist, Confidence::Low, "rule"});
  VIEW_CHECK(match.family == Family::Axon && !strcmp(label, "Axon Body 3"));
  promoteLabelled(match, label, {Family::Camera, Confidence::Medium, "hint"}); // A tie keeps it.
  VIEW_CHECK(match.family == Family::Axon && !strcmp(label, "Axon Body 3"));
  promoteLabelled(match, label, {Family::Watchlist, Confidence::High, "rule"});
  VIEW_CHECK(match.family == Family::Watchlist && !*label);
  promoteLabelled(match, nullptr, {Family::None, Confidence::None, ""});

  // The flinch grows with the tier and the signal, and stays in the drive-safe box on the road.
  VIEW_CHECK(flinchPercent(0, -40) == 100 && flinchPercent(0, -50) == 100 &&
             flinchPercent(0, -90) == 40 && flinchPercent(0, -120) == 40);
  for (unsigned tier = 0; tier < 5; ++tier) {
    for (int rssi = -100; rssi < -30; ++rssi)
      VIEW_CHECK(flinchPercent(tier, rssi) <= flinchPercent(tier, rssi + 1) &&
                 flinchPercent(tier, rssi) >= 16 && flinchPercent(tier, rssi) <= 100);
    VIEW_CHECK(flinchPercent(tier, -60) <= flinchPercent(0, -60));
  }
  VIEW_CHECK(
      flinchPercent(unsigned(Tier::Tracker), -60) < flinchPercent(unsigned(Tier::Hacker), -60) &&
      flinchPercent(unsigned(Tier::Beacon), -60) < flinchPercent(unsigned(Tier::Tracker), -60) &&
      flinchPercent(99, -60) == flinchPercent(4, -60));
  for (unsigned step = 0; step < 8; ++step) {
    MotionCue full = panicMotion(step);
    MotionCue same = flinchCue(full, 100, false);
    VIEW_CHECK(same.yaw == full.yaw && same.pitch == full.pitch && same.speed == full.speed);
    VIEW_CHECK(flinchCue(full, 250, false).yaw == full.yaw);
    MotionCue half = flinchCue(full, 50, false);
    VIEW_CHECK(half.yaw == full.yaw / 2 && half.pitch == 200 + (full.pitch - 200) / 2);
    MotionCue still = flinchCue(full, 0, false);
    VIEW_CHECK(still.yaw == 0 && still.pitch == 200);
    for (unsigned percent : {0u, 16u, 40u, 100u}) {
      MotionCue road = flinchCue(full, percent, true);
      VIEW_CHECK(road.yaw >= -DriveSafeYaw && road.yaw <= DriveSafeYaw &&
                 road.pitch >= 200 - DriveSafePitch && road.pitch <= 200 + DriveSafePitch &&
                 road.speed <= DriveSafeSpeed && road.holdMs == full.holdMs);
    }
  }

  // Bubble lines for the two behaviour alerts.
  char line[64];
  VIEW_CHECK(strlen(FloodLine) <= BubbleChars);
  followLine(line, sizeof(line), 19 * 60 + 57, 4460);
  VIEW_CHECK(!strcmp(line, "This tag has been with us 19 min / 4.4 km."));
  followLine(line, sizeof(line), 0xffffffffu, 0xffffffffu);
  VIEW_CHECK(strlen(line) <= BubbleChars);

  // Recon small talk: one remark per event, rate limited, never while busy.
  const uint32_t minute = 60000;
  {
    ReconBanter talk;
    ReconPulse p;
    uint32_t now = 1000;
    talk.begin(now);
    unsigned said = 0;
    for (; now < 19 * minute; now += 500) // Quiet, but not for long enough yet.
      said += talk.update(p, now, false) != Topic::Count;
    VIEW_CHECK(!said);
    Topic quiet = Topic::Count;
    for (; now < 21 * minute && quiet == Topic::Count; now += 500)
      quiet = talk.update(p, now, false);
    VIEW_CHECK(quiet == Topic::ReconQuiet && now >= 20 * minute);
    for (uint32_t until = now + 19 * minute; now < until; now += 500) // And not again soon.
      VIEW_CHECK(talk.update(p, now, false) == Topic::Count);
    // The first device of the session is announced once.
    p.live = 1;
    now += 2 * minute;
    VIEW_CHECK(talk.update(p, now, false) == Topic::ReconFirst);
    for (unsigned i = 0; i < 100; ++i)
      VIEW_CHECK(talk.update(p, now += 500, false) == Topic::Count);
    // Three cameras at once, a few minutes later.
    now += 2 * minute;
    p.live = p.surveillance = 3;
    VIEW_CHECK(talk.update(p, now, false) == Topic::ReconCrowd);
    p.surveillance = 2;
    VIEW_CHECK(talk.update(p, now += 2 * minute, false) == Topic::Count);
    p.surveillance = 3; // The same crowd again inside ten minutes is not news.
    VIEW_CHECK(talk.update(p, now += 500, false) == Topic::Count);
    // The last one leaves.
    p = {};
    VIEW_CHECK(talk.update(p, now += 2 * minute, false) == Topic::ReconLost);
    p.live = 1;
    VIEW_CHECK(talk.update(p, now += 2 * minute, false) == Topic::Count);
    p.live = 0; // Lost again within five minutes: quiet.
    VIEW_CHECK(talk.update(p, now += 500, false) == Topic::Count);
    // The quiet spell counts from when the last device left, not from boot.
    uint32_t left = now;
    Topic next = Topic::Count;
    while (next == Topic::Count && now < left + 30 * minute)
      next = talk.update(p, now += 500, false);
    VIEW_CHECK(next == Topic::ReconQuiet && now - left >= ReconQuietMs &&
               now - left < ReconQuietMs + 1000);
  }
  {
    // Busy (an alert is open): the first device is the alert's news, not small talk.
    ReconBanter talk;
    ReconPulse p;
    talk.begin(0);
    p.live = 1;
    VIEW_CHECK(talk.update(p, 1000, true) == Topic::Count);
    VIEW_CHECK(talk.update(p, 20000, false) == Topic::Count);
    // A follower that stays in range says nothing; five minutes after it goes, one remark.
    p.following = 1;
    uint32_t now = 30000;
    for (; now < 10 * minute; now += 500)
      VIEW_CHECK(talk.update(p, now, false) == Topic::Count);
    p.following = 0;
    uint32_t gone = now - 500;
    Topic cleared = Topic::Count;
    while (cleared == Topic::Count && now < gone + 10 * minute) {
      cleared = talk.update(p, now, now < gone + 6 * minute); // Busy for the first minute after.
      now += 500;
    }
    VIEW_CHECK(cleared == Topic::ReconCleared && now - gone >= 6 * minute &&
               now - gone < 6 * minute + 1500);
    for (unsigned i = 0; i < 100; ++i)
      VIEW_CHECK(talk.update(p, now += 500, false) != Topic::ReconCleared);
  }
  {
    // The owner marks the following tag as their own: it drops out of the counts while still
    // in the car, and must not be announced as "gone" five minutes later.
    ReconBanter talk;
    ReconPulse p;
    p.live = p.following = 1;
    uint32_t now = 1000;
    for (; now < 3 * minute; now += 500)
      talk.update(p, now, false);
    talk.forget();
    p.live = p.following = 0; // Ignored devices are not in the pulse.
    for (; now < 40 * minute; now += 500)
      VIEW_CHECK(talk.update(p, now, false) != Topic::ReconCleared);
    // A second follower still in range keeps the watch going after the first is ignored.
    ReconBanter two;
    p.live = p.following = 2;
    for (now = 1000; now < 3 * minute; now += 500)
      two.update(p, now, false);
    two.forget();
    p.live = p.following = 1;
    two.update(p, now, false);
    p.live = p.following = 0;
    Topic cleared = Topic::Count;
    for (uint32_t end = now + 10 * minute; now < end && cleared != Topic::ReconCleared; now += 500)
      cleared = two.update(p, now, false);
    VIEW_CHECK(cleared == Topic::ReconCleared);
  }
  for (const char *line : rv::SettingsNote) // Two lines at x 12 inside a 320 px screen.
    VIEW_CHECK(strlen(line) <= 49);
  VIEW_CHECK(rv::ignoreLabel(true, false, false).active &&
             !rv::ignoreLabel(true, false, true).active &&
             !strcmp(rv::ignoreLabel(true, true, true).text, "UNIGNORE") &&
             !strcmp(rv::ignoreLabel(false, false, true).text, "NO LONGER LISTED"));
  {
    // Whatever the counts do, two remarks are never closer than the gap.
    ReconBanter talk;
    uint32_t last = 0, seed = 12345;
    bool any = false;
    for (uint32_t now = 0; now < 6 * 60 * minute; now += 500) {
      seed = seed * 1664525u + 1013904223u;
      ReconPulse p;
      p.live = (seed >> 8) % 5;
      p.surveillance = p.live ? (seed >> 12) % (p.live + 1) : 0;
      p.following = (seed >> 16) % 7 == 0 && p.live;
      Topic t = talk.update(p, now, (seed >> 20) % 4 == 0);
      if (t == Topic::Count)
        continue;
      VIEW_CHECK(t >= Topic::ReconQuiet && t <= Topic::ReconCleared);
      VIEW_CHECK(!any || now - last >= ReconRemarkGapMs);
      any = true;
      last = now;
    }
    VIEW_CHECK(any);
  }
  // Every recon topic has lines for every buddy, gentle and spicy.
  for (Topic t : {Topic::ReconQuiet, Topic::ReconFirst, Topic::ReconCrowd, Topic::ReconLost,
                  Topic::ReconIgnore, Topic::ReconCleared})
    for (unsigned who = 0; who < 5; ++who) {
      VIEW_CHECK(Banter::count(t, who, false) >= 5);
      VIEW_CHECK(Banter::count(t, who, true) > Banter::count(t, who, false));
    }
}

inline void run() {
  palettes();
  texts();
  snapshots();
  paging();
  homeScreen();
  pages();
  wiring();
  std::cout << "Recon view layout, tap map, text fitting and banter checks passed\n";
}
} // namespace recon_view_tests
inline void reconViewTests() {
  recon_view_tests::run();
}
