// PC preview of the buddy art and home screen. Run through tools/preview.py.
#include "audio_view.h"
#include "avatar_limits.h"
#include "core.h"
#include "home_view.h"
#include "host_canvas.h"
#include "profile.h"
#include "recon_view.h"
#include "studio_view.h"
#include <string>
#include <vector>
using namespace dash;
static std::string out;
// A real recon table fed with made-up sightings, so the sheets exercise the same snapshot code
// the firmware runs. Times are millis(); `clock` is seconds since 2000 for ledger rows.
struct Scene {
  std::vector<ReconDevice> rows;
  ReconTable table;
  AlertPolicy policy;
  AlertContext ctx;
  uint32_t now = 900000, clock = 846000000;
  unsigned next = 1;
  Scene() : rows(96) {
    table.attach(rows.data(), 96);
  }
  // Heard `ago` ms before now. A second, stronger or weaker reading sets the trend arrow.
  ReconDevice *hear(Family family, int rssi, uint32_t ago = 1000, bool ble = false,
                    const char *label = "", const char *name = "", int trend = 0,
                    Confidence confidence = Confidence::Medium, uint32_t since = 20000) {
    uint8_t mac[6] = {uint8_t(ble ? 0x5a : 0x00), 0x25,         0xdf, uint8_t(next * 37),
                      uint8_t(next * 91),         uint8_t(next)};
    ++next;
    ReconSighting s;
    s.mac = mac;
    s.ble = ble;
    s.addressType = ble ? 1 : 0;
    s.match = {family, confidence, "Preview sighting; nothing was detected"};
    s.label = label;
    s.name = name;
    s.channel = ble ? 0 : 6;
    s.rssi = rssi - trend * 6;
    uint32_t first = now - ago - since;
    ReconSeen seen = table.observe(s, first);
    if (!seen.row)
      return nullptr;
    policy.decide(*seen.row, seen, ctx, first);
    s.rssi = rssi;
    seen = table.observe(s, now - ago);
    policy.decide(*seen.row, seen, ctx, now - ago);
    return seen.row;
  }
  void ledger(Family family, int rssi, uint32_t secondsAgo, bool ble, const char *label = "",
              bool alerted = false) {
    LedgerEntry e;
    e.utc = secondsAgo ? clock - secondsAgo : 0;
    e.uptimeMs = 5000 + next * 1000;
    e.boot = 3;
    e.family = family;
    e.confidence = Confidence::Medium;
    e.ble = ble;
    e.addressType = ble ? 1 : 0;
    e.mac[0] = 0x7a;
    e.mac[5] = uint8_t(next++);
    e.rssi = int8_t(rssi);
    e.alerted = alerted;
    snprintf(e.label, sizeof(e.label), "%s", label);
    table.restore(e);
  }
  // A tag heard at four places over thirty minutes: the follow rule trips.
  ReconDevice *follower() {
    uint8_t mac[6] = {0x5e, 0x11, 0x22, 0x33, 0x44, 0x55};
    ReconSeen seen;
    for (int place = 0; place < 4; ++place) {
      ReconSighting s;
      s.mac = mac;
      s.ble = true;
      s.addressType = 1;
      s.match = {Family::Tracker, Confidence::High, "Apple Find My advert, separated from owner"};
      s.rssi = -58 - place;
      s.fix = true;
      s.lat = 39.9526f + 0.011f * float(place);
      s.lon = -75.1652f + 0.006f * float(place);
      uint32_t at = now - 1800000 + uint32_t(place) * 599000;
      seen = table.observe(s, at);
      policy.decide(*seen.row, seen, ctx, at);
    }
    table.followDue(*seen.row);
    return seen.row;
  }
  ReconCounts counts() const {
    ReconCounts c;
    table.counts(c, now);
    return c;
  }
  ReconHome home() const {
    ReconHome h;
    reconHome(h, table, counts(), now, clock);
    return h;
  }
};
// The usual street: a plate reader, a tag, a Flipper that has gone, plus one from the ledger.
static void street(Scene &s) {
  s.hear(Family::Tracker, -59, 1500, true, "", "", 1);
  s.hear(Family::Alpr, -49, 800, false, "Motorola Vigilant", "", 0, Confidence::High);
  s.hear(Family::Flipper, -80, 240000, true, "", "Flipper Rokeo");
}
static HostCanvas sheetOf(int columns, int rows) {
  return HostCanvas(columns * 324 + 4, rows * 244 + 4);
}
static void place(HostCanvas &sheet, int column, int row, const HomeView &v) {
  HostCanvas screen(320, 240);
  screen.fillScreen(v.colors.bg);
  drawHome(screen, v);
  sheet.blit(screen, 4 + column * 324, 4 + row * 244);
}
static HomeView base(Theme theme = Theme::Dark) {
  HomeView v;
  v.now = 61000;
  v.theme = theme;
  v.colors = palette(theme);
  v.level = 4;
  v.xp = 19;
  return v;
}
// A custom buddy in neutral colours, so a gear sheet shows the gear and not the palette.
static Look plain(int shape) {
  static const uint32_t bodies[] = {0xfff3d2, 0xa5edc0, 0xc9c2ff, 0x17141f, 0xff6a3d};
  static const uint32_t eyes[] = {0x101029, 0x245549, 0x9affdd, 0xedffff, 0x201029};
  Look look;
  look.custom = true;
  look.base = shape;
  look.body = rgb565(bodies[shape]);
  look.accent = rgb565(shape == 3 ? 0xb18cff : 0xff5c8a);
  look.eye = rgb565(eyes[shape]);
  return look;
}
// One sheet per studio slot: a row per shape, a column per option, on a mid-tone backdrop.
// Wide slots are split so every cell stays at the LCD's own scale.
static void slotSheet(const char *name, int count, int Look::*slot) {
  const int w = 184, h = 140, perSheet = 6;
  for (int first = 0; first < count; first += perSheet) {
    int columns = count - first < perSheet ? count - first : perSheet;
    HostCanvas sheet(columns * w, 5 * h);
    sheet.fillScreen(0x3a4f);
    for (int shape = 0; shape < 5; ++shape)
      for (int i = 0; i < columns; ++i) {
        Look look = plain(shape);
        look.*slot = first + i;
        Pose pose;
        pose.phase = 3;
        drawBuddy(sheet, look, w / 2 + i * w, 72 + shape * h, pose);
      }
    sheet.savePpm((out + "/slot-" + name + (first ? "-2" : "") + ".ppm").c_str());
  }
}
// A style pack's signature look on a given shape, as the robot would show it.
static Look packLook(unsigned pack, int shape) {
  const PackStyle &style = PackStyles[pack];
  Look look;
  look.custom = true;
  look.base = shape;
  look.body = rgb565(style.body);
  look.accent = rgb565(style.accent);
  look.eye = rgb565(style.eye);
  look.eyes = style.eyes;
  look.headwear = style.headwear;
  look.eyewear = style.eyewear;
  look.neckwear = style.neckwear;
  look.mark = style.mark;
  look.back = style.back;
  look.held = style.held;
  look.aura = style.aura;
  return look;
}
// Every pack on every shape, on the real home screen: Recon view, then Full Face.
static void packSheets() {
  for (int face = 0; face < 2; ++face)
    for (int half = 0; half < 2; ++half) {
      int columns = half ? 2 : 3;
      HostCanvas sheet = sheetOf(columns, 5);
      for (unsigned pack = 1; pack < limits::Packs; ++pack)
        for (int i = 0; i < columns; ++i) {
          HomeView v = base(pack == 4 && face ? Theme::Light : Theme::Dark);
          v.face = face;
          v.look = packLook(pack, half * 3 + i);
          v.name = PackStyles[pack].name;
          v.backdrop = PackStyles[pack].backdrop;
          v.mood = face && i == 1 ? 3 : 0;
          if (!face && i == 2) {
            v.speaking = true;
            v.message = "New fit. Same little signal goblin.";
          }
          place(sheet, i, int(pack) - 1, v);
        }
      sheet.savePpm(
          (out + "/packs-" + (face ? "face" : "recon") + (half ? "-2" : "") + ".ppm").c_str());
    }
}
// The Full Face close-up (2.1x) of the options that sit on the face, where detail matters most.
static void closeUps() {
  struct Pick {
    int Look::*slot;
    int first, last;
  };
  const Pick picks[] = {
      {&Look::eyes, 6, 9}, {&Look::eyewear, 4, 7}, {&Look::mark, 1, 7}, {&Look::eyewear, 1, 1}};
  const int w = 320, h = 152;
  HostCanvas sheet(4 * w, 4 * h);
  sheet.fillScreen(0x3a4f);
  int cell = 0;
  for (const Pick &pick : picks)
    for (int option = pick.first; option <= pick.last; ++option, ++cell) {
      int x = cell % 4 * w, y = cell / 4 * h;
      Look look = plain(cell % 5);
      look.*pick.slot = option;
      sheet.setClipRect(x + 1, y + 1, w - 2, h - 2);
      art::Zoom<HostCanvas> zoom{sheet, x + w / 2, y + 72, 2.1f};
      Pose pose;
      pose.phase = 3;
      drawBuddy(zoom, look, 0, 0, pose);
      sheet.clearClipRect();
    }
  sheet.savePpm((out + "/closeups.ppm").c_str());
}
// The on-device Avatar Studio: its three pages, in two themes.
static void studioScreens() {
  HostCanvas sheet = sheetOf(3, 2);
  for (int row = 0; row < 2; ++row)
    for (unsigned page = 0; page < 3; ++page) {
      StudioView v;
      v.now = 61000;
      v.theme = row ? Theme::Light : Theme::Dark;
      v.colors = palette(v.theme);
      unsigned pack = row ? 1 : 3;
      v.look = packLook(pack, row ? 0 : 3);
      v.name = row ? "Peaches" : "Nyx the 2nd";
      v.page = page;
      const PackStyle &style = PackStyles[pack];
      const unsigned values[3][4] = {{unsigned(v.look.base), style.eyes, style.mark, pack},
                                     {style.headwear, style.eyewear, style.neckwear, style.back},
                                     {style.held, style.aura, row ? 0u : 4u, 0u}};
      for (int i = 0; i < 4; ++i)
        v.values[i] = values[page][i];
      HostCanvas screen(320, 240);
      screen.fillScreen(v.colors.bg);
      drawStudio(screen, v);
      sheet.blit(screen, 4 + int(page) * 324, 4 + row * 244);
      if (!row && page == 1)
        screen.savePpm((out + "/ui-studio.ppm").c_str());
    }
  sheet.savePpm((out + "/studio.ppm").c_str());
}
// A busy block: every tier at once, more rows than the panel holds.
static void crowd(Scene &s) {
  s.hear(Family::Flock, -47, 600, false, "Flock Falcon", "", 1, Confidence::High);
  s.hear(Family::Axon, -63, 900, true, "Axon Body 3", "", 0, Confidence::High);
  s.hear(Family::Meta, -71, 1200, true, "Ray-Ban Meta", "", -1);
  s.hear(Family::Flipper, -66, 700, true, "", "Flipper Rokeo");
  s.hear(Family::Deauth, -58, 2000, false);
  s.hear(Family::Tracker, -52, 400, true, "", "", 1);
  s.hear(Family::Ring, -77, 1500, false, "", "Ring-9f2a11");
  s.hear(Family::Drone, -84, 3000, false, "1581F5FJD228K00A", "", 0, Confidence::High);
  s.hear(Family::Tile, -88, 90000, true);
  s.ledger(Family::Alpr, -61, 5400, false, "Genetec AutoVu", true);
}
static void reconSheets() {
  using namespace reconview;
  auto styleOf = [](Theme theme) { return Style{palette(theme), theme}; };
  auto fresh = [](Theme theme) {
    HostCanvas screen(320, 240);
    screen.fillScreen(palette(theme).bg);
    return screen;
  };
  auto put = [](HostCanvas &sheet, int column, int row, const HostCanvas &screen) {
    sheet.blit(screen, 4 + column * 324, 4 + row * 244);
  };
  auto save = [](const HostCanvas &screen, const char *file) {
    screen.savePpm((out + "/" + file + ".ppm").c_str());
  };
  auto homeOf = [](Theme theme, const Scene &s) {
    HomeView v = base(theme);
    v.now = s.now;
    v.recon = s.home();
    return v;
  };
  auto driving = [](HomeView &v) {
    v.recording = v.moving = v.fix = true;
    v.sats = 9;
    v.state = "REC";
    v.kit = 2;
    v.wifiSaved = 148;
    v.bleSaved = 37;
    v.look.base = 4;
    v.name = "Ember";
    v.backdrop = 1;
  };

  // 1. The home panel in every state.
  {
    HostCanvas sheet = sheetOf(3, 4);
    Scene empty;
    place(sheet, 0, 0, homeOf(Theme::Dark, empty));
    Scene one;
    one.hear(Family::Tracker, -59, 1500, true, "", "", 1);
    place(sheet, 1, 0, homeOf(Theme::Dark, one));
    Scene two;
    two.hear(Family::Alpr, -49, 800, false, "Motorola Vigilant", "", 1, Confidence::High);
    two.hear(Family::GoogleTag, -72, 2500, true, "Chipolo", "", -1);
    place(sheet, 2, 0, homeOf(Theme::Dark, two));
    Scene many;
    crowd(many);
    HomeView v = homeOf(Theme::Dark, many);
    v.mood = 1;
    place(sheet, 0, 1, v);
    Scene old; // Nothing in range: a stale row from this boot and rows from the ledger.
    old.hear(Family::Flipper, -80, 240000, true, "", "Flipper Rokeo");
    old.hear(Family::Flock, -70, 3900000, false, "Flock Falcon", "", 0, Confidence::High);
    old.ledger(Family::Alpr, -61, 5400, false, "Genetec AutoVu", true);
    old.ledger(Family::Tracker, -66, 200000, true);
    old.ledger(Family::Ring, -80, 0, false);
    place(sheet, 1, 1, homeOf(Theme::Dark, old));
    Scene mixed; // An ignored tag, a tag that is following, and the worst-case row texts.
    ReconDevice *mine = mixed.hear(Family::SamsungTag, -44, 900, true);
    mixed.policy.ignores.add(mine->mac, mine->family);
    mixed.table.applyIgnores(mixed.policy.ignores);
    mixed.follower();
    ReconDevice *far = mixed.hear(Family::Raven, -100, 500, false, "", "", -1);
    far->encounters = 1200;
    mixed.hear(Family::Pwnagotchi, -90, 1800, false, "pwnagotchi-of-doom-9000");
    place(sheet, 2, 1, homeOf(Theme::Dark, mixed));
    v = homeOf(Theme::Dark, many); // Recording: the top two threats and the drive counters.
    driving(v);
    place(sheet, 0, 2, v);
    v = homeOf(Theme::Dark, empty);
    driving(v);
    v.wifiSaved = 0;
    v.bleSaved = 0;
    place(sheet, 1, 2, v);
    v = homeOf(Theme::Dark, one);
    driving(v);
    v.wifiSaved = 99999;
    v.bleSaved = 4321;
    v.speaking = true;
    v.message = "Spotted 'NETGEAR-Guest-5G..'. That's a new one.";
    place(sheet, 2, 2, v);
    v = homeOf(Theme::Dark, empty); // Full Face: the chip, quiet.
    v.face = true;
    place(sheet, 0, 3, v);
    v = homeOf(Theme::Dark, many);
    driving(v);
    v.face = true;
    place(sheet, 1, 3, v);
    v = homeOf(Theme::Bwm, one);
    v.face = true;
    v.speaking = true;
    v.message = "Something just showed up. I wrote it down.";
    place(sheet, 2, 3, v);
    save(sheet, "recon-home");
  }
  // 2. Alert cards: a device, a flood, a follower, Full Face, Light, and the demo.
  {
    HostCanvas sheet = sheetOf(3, 2);
    Scene many;
    crowd(many);
    HomeView v = homeOf(Theme::Dark, many);
    v.mood = 4;
    v.alert = v.speaking = true;
    v.message = "Plate camera spotted!";
    v.card.family = Family::Alpr;
    v.card.confidence = Confidence::High;
    v.card.label = "Motorola Vigilant";
    v.card.rssi = -49;
    place(sheet, 0, 0, v);
    v.message = FloodLine;
    v.card = {};
    v.card.kind = 1;
    v.card.family = Family::Tracker;
    v.card.confidence = Confidence::Medium;
    v.card.rssi = -71;
    place(sheet, 1, 0, v);
    Scene tailed;
    tailed.follower();
    v = homeOf(Theme::Dark, tailed);
    v.mood = 4;
    v.alert = v.speaking = true;
    v.message = "This tag has been with us 29 min / 4.4 km.";
    v.card.kind = 2;
    v.card.family = Family::Tracker;
    v.card.confidence = Confidence::High;
    v.card.rssi = -61;
    v.card.followS = 1797;
    v.card.followMeters = 4400;
    place(sheet, 2, 0, v);
    v.face = true;
    v.message = "We're being followed!";
    place(sheet, 0, 1, v);
    v = homeOf(Theme::Light, many);
    v.mood = 4;
    v.alert = v.speaking = true;
    v.message = "A wild dolphin gadget appeared!";
    v.card.family = Family::Flipper;
    v.card.confidence = Confidence::Medium;
    v.card.rssi = -66;
    place(sheet, 1, 1, v);
    v = homeOf(Theme::Matrix, many);
    v.mood = 4;
    v.alert = v.demo = v.speaking = true;
    v.message = "PIG DETECTED! ACT NATURAL!";
    v.card.family = Family::Axon;
    v.card.confidence = Confidence::Medium;
    v.card.rssi = -45;
    place(sheet, 2, 1, v);
    save(sheet, "recon-alerts");
  }
  // 3. Four themes: the home panel and the LIVE list.
  {
    HostCanvas sheet = sheetOf(4, 2);
    const Theme themes[] = {Theme::Dark, Theme::Light, Theme::Matrix, Theme::Bwm};
    static uint16_t order[96];
    for (int i = 0; i < 4; ++i) {
      Scene s;
      crowd(s);
      place(sheet, i, 0, homeOf(themes[i], s));
      HostCanvas screen = fresh(themes[i]);
      ReconList list;
      reconList(list, s.table, Tab::Live, 0, s.now, s.clock, order, 96);
      ListContext ctx;
      drawReconList(screen, styleOf(themes[i]), Tab::Live, list, ctx);
      put(sheet, i, 1, screen);
    }
    save(sheet, "recon-themes");
  }
  // 4. The Recon page and the device card.
  {
    HostCanvas sheet = sheetOf(3, 4);
    static uint16_t order[96];
    Style dark = styleOf(Theme::Dark);
    Scene s;
    crowd(s);
    ReconDevice *mine = s.hear(Family::SamsungTag, -44, 900, true);
    s.policy.ignores.add(mine->mac, mine->family);
    s.table.applyIgnores(s.policy.ignores);
    ReconDevice *tail = s.follower();
    uint8_t watched[6] = {0x00, 0x1b, 0x63, 0x84, 0x45, 0xe6};
    ReconSighting w;
    w.mac = watched;
    w.match = {Family::Watchlist, Confidence::High, "Watchlist rule: exact address"};
    w.name = "Unmarked-Van-AP";
    w.channel = 11;
    w.rssi = -55;
    ReconSeen seen = s.table.observe(w, s.now - 700);
    s.policy.decide(*seen.row, seen, s.ctx, s.now - 700);
    ReconDevice *van = seen.row;
    ReconDevice *plate =
        s.hear(Family::Alpr, -49, 800, false, "Motorola Vigilant", "", 1, Confidence::High, 760000);
    plate->lat = 39.95258f;
    plate->lon = -75.16522f;
    plate->encounters = 4;
    plate->evidence = "Motorola Solutions OUI; ALPR vendor radio seen as an access point";
    ReconList list;
    ListContext ctx;
    ctx.seenTotal = s.counts().total;
    HostCanvas screen = fresh(Theme::Dark);
    reconList(list, s.table, Tab::Live, 0, s.now, s.clock, order, 96);
    drawReconList(screen, dark, Tab::Live, list, ctx);
    put(sheet, 0, 0, screen);
    save(screen, "ui-recon-list");
    screen = fresh(Theme::Dark);
    reconList(list, s.table, Tab::Seen, 2, s.now, s.clock, order, 96);
    drawReconList(screen, dark, Tab::Seen, list, ctx);
    put(sheet, 1, 0, screen);
    ReconStats stats;
    reconStats(stats, s.table, s.counts(), s.policy.ignores.size(), LedgerState::Ready, 0);
    stats.lifetime[unsigned(Family::Tracker)] = 412;
    stats.lifetime[unsigned(Family::Flock)] = 57;
    stats.lifetime[unsigned(Family::Ring)] = 131;
    stats.lifetimeTotal = 623;
    screen = fresh(Theme::Dark);
    drawReconStats(screen, dark, stats);
    put(sheet, 2, 0, screen);
    Scene quiet; // Empty states.
    quiet.ledger(Family::Alpr, -61, 5400, false, "Genetec AutoVu", true);
    quiet.ledger(Family::Tracker, -66, 200000, true);
    screen = fresh(Theme::Dark);
    reconList(list, quiet.table, Tab::Live, 0, quiet.now, quiet.clock, order, 96);
    ctx.seenTotal = 2;
    drawReconList(screen, dark, Tab::Live, list, ctx);
    put(sheet, 0, 1, screen);
    Scene none;
    screen = fresh(Theme::Dark);
    reconList(list, none.table, Tab::Seen, 0, none.now, none.clock, order, 96);
    ctx = {};
    ctx.card = false;
    drawReconList(screen, dark, Tab::Seen, list, ctx);
    put(sheet, 1, 1, screen);
    stats.page = 2;
    stats.ledger = LedgerState::Failed;
    screen = fresh(Theme::Dark);
    drawReconStats(screen, dark, stats);
    put(sheet, 2, 1, screen);
    screen = fresh(Theme::Dark); // Device cards.
    drawReconDetail(screen, dark, reconRow(*plate, s.now, s.clock), true);
    put(sheet, 0, 2, screen);
    save(screen, "ui-recon-device");
    screen = fresh(Theme::Dark);
    drawReconDetail(screen, dark, reconRow(*tail, s.now, s.clock), true);
    put(sheet, 1, 2, screen);
    screen = fresh(Theme::Dark);
    drawReconDetail(screen, dark, reconRow(*van, s.now, s.clock), true);
    put(sheet, 2, 2, screen);
    screen = fresh(Theme::Light); // An ignored tag, a ledger row, and a row that has gone.
    drawReconDetail(screen, styleOf(Theme::Light), reconRow(*mine, s.now, s.clock), true);
    put(sheet, 0, 3, screen);
    screen = fresh(Theme::Matrix);
    reconList(list, quiet.table, Tab::Seen, 0, quiet.now, quiet.clock, order, 96);
    drawReconDetail(screen, styleOf(Theme::Matrix), list.rows[1], true);
    put(sheet, 1, 3, screen);
    screen = fresh(Theme::Bwm);
    drawReconDetail(screen, styleOf(Theme::Bwm), reconRow(*van, s.now, s.clock), false);
    put(sheet, 2, 3, screen);
    save(sheet, "recon-pages");
  }
  // 5. Detection settings and the type filter.
  {
    HostCanvas sheet = sheetOf(3, 2);
    SettingsView v;
    v.ignored = 3;
    HostCanvas screen = fresh(Theme::Dark);
    drawReconSettings(screen, styleOf(Theme::Dark), v);
    put(sheet, 0, 0, screen);
    save(screen, "ui-recon-settings");
    v.confirmClear = true;
    v.autoSnooze = 0;
    v.quietRoad = false;
    v.repeat = true;
    v.familyQuietMs = 300000;
    v.threshold = Confidence::High;
    v.ignored = 64;
    screen = fresh(Theme::Dark);
    drawReconSettings(screen, styleOf(Theme::Dark), v);
    put(sheet, 1, 0, screen);
    v = {};
    v.autoSnooze = 10;
    screen = fresh(Theme::Light);
    drawReconSettings(screen, styleOf(Theme::Light), v);
    put(sheet, 2, 0, screen);
    uint32_t filter = TypeFilter::Defaults & ~TypeFilter::flag(Family::Ring);
    for (unsigned page = 0; page < TypePages; ++page) {
      Theme theme = page == 2 ? Theme::Matrix : Theme::Dark;
      screen = fresh(theme);
      drawReconTypes(screen, styleOf(theme), filter, page);
      put(sheet, int(page), 1, screen);
    }
    save(sheet, "recon-settings");
  }
  // 6. States added after review: an alert that opens over another page, readings that are no
  //    longer current, the owner's own gear in range, a full ignore list, an unapplied type edit.
  {
    HostCanvas sheet = sheetOf(3, 2);
    static uint16_t order[96];
    Style dark = styleOf(Theme::Dark);
    Scene s;
    ReconDevice *dashcam = s.hear(Family::Camera, -41, 700, false, "Viofo dashcam");
    ReconDevice *doorbell = s.hear(Family::Ring, -67, 1200, false, "Ring LLC");
    s.policy.ignores.add(dashcam->mac, dashcam->family);
    s.policy.ignores.add(doorbell->mac, doorbell->family);
    s.table.applyIgnores(s.policy.ignores);
    s.hear(Family::Flipper, -58, 900, true, "", "Flipper Rokeo", 1);
    ReconDevice *passed = // A plate reader passed 40 s ago: still listed, no longer "now".
        s.hear(Family::Alpr, -47, 40000, false, "Motorola Vigilant", "", 1, Confidence::High);
    s.hear(Family::Tile, -70, 240000, true);
    AlertCardView card;
    card.family = Family::Flipper;
    card.confidence = Confidence::Medium;
    card.rssi = -58;
    ReconList list;
    ListContext ctx;
    ctx.seenTotal = s.counts().total;
    HostCanvas screen = fresh(Theme::Dark);
    reconList(list, s.table, Tab::Live, 0, s.now, s.clock, order, 96);
    drawReconList(screen, dark, Tab::Live, list, ctx);
    drawAlertCard(screen, dark, card); // The alert opened while the list was being read.
    put(sheet, 0, 0, screen);
    SettingsView v;
    v.ignored = 2;
    screen = fresh(Theme::Dark);
    drawReconSettings(screen, dark, v);
    card.kind = 2;
    card.family = Family::Tracker;
    card.followS = 1797;
    card.followMeters = 4400;
    drawAlertCard(screen, dark, card);
    put(sheet, 1, 0, screen);
    screen = fresh(Theme::Dark);
    drawReconDetail(screen, dark, reconRow(*passed, s.now, s.clock), true, true);
    put(sheet, 2, 0, screen);
    place(sheet, 0, 1, homeOf(Theme::Dark, s));
    HomeView drive = homeOf(Theme::Dark, s); // Two rows while driving: not the owner's own.
    driving(drive);
    place(sheet, 1, 1, drive);
    screen = fresh(Theme::Dark);
    drawReconTypes(screen, dark, TypeFilter::Defaults & ~tierMask(Tier::Surveillance), 0, true);
    put(sheet, 2, 1, screen);
    save(sheet, "recon-review");
  }
}
int main(int argc, char **argv) {
  out = argc > 1 ? argv[1] : ".";
  reconSheets();
  studioScreens();
  packSheets();
  closeUps();
  slotSheet("eyes", limits::Eyes, &Look::eyes);
  slotSheet("headwear", limits::Headwear, &Look::headwear);
  slotSheet("eyewear", limits::Eyewear, &Look::eyewear);
  slotSheet("neckwear", limits::Neckwear, &Look::neckwear);
  slotSheet("mark", limits::Marks, &Look::mark);
  slotSheet("back", limits::Backs, &Look::back);
  slotSheet("held", limits::Held, &Look::held);
  slotSheet("aura", limits::Auras, &Look::aura);
  HostCanvas buddies(7 * 150, 5 * 130);
  buddies.fillScreen(0x3a4f);
  for (int a = 0; a < 5; ++a)
    for (int mood = 0; mood < 7; ++mood) {
      Look look;
      look.base = a;
      Pose pose;
      pose.mood = mood;
      pose.phase = 2;
      drawBuddy(buddies, look, 75 + mood * 150, 72 + a * 130, pose);
    }
  buddies.savePpm((out + "/buddies.ppm").c_str());

  HostCanvas home = sheetOf(3, 4);
  HomeView v = base(); // 1. Calm, nothing nearby.
  place(home, 0, 0, v);
  v = base(); // 2. Talking.
  v.speaking = true;
  v.message = "I'd knock a router off a shelf. For science.";
  v.mood = 1;
  place(home, 1, 0, v);
  v = base(); // 3. Hungry and pouting, with clues.
  v.look.base = 1;
  v.name = "Sprout";
  v.mood = 6;
  v.feeling = Feeling::Hungry;
  v.food = 22;
  v.love = 31;
  v.speaking = true;
  v.message = "My tummy is making dial-up noises.";
  {
    Scene s;
    street(s);
    v.recon = s.home();
  }
  place(home, 2, 0, v);
  v = base(); // 4. Recording a drive.
  v.look.base = 4;
  v.name = "Ember";
  v.recording = v.moving = v.fix = true;
  v.sats = 9;
  v.state = "REC";
  v.kit = 2;
  v.wifiSaved = 148;
  v.bleSaved = 37;
  v.feeling = Feeling::Hyped;
  v.backdrop = 1;
  v.level = 12;
  v.discoveryAge = 400;
  place(home, 0, 1, v);
  v.discoveryAge = UINT32_MAX; // 5. Same drive, quoting a network.
  v.speaking = true;
  v.message = "Spotted 'NETGEAR-Guest-5G..'. That's a new one.";
  place(home, 1, 1, v);
  v = base(); // 6. Alert.
  v.look.base = 2;
  v.name = "Orbit";
  v.mood = 4;
  v.alert = v.speaking = true;
  v.message = "A wild dolphin gadget appeared!";
  v.card.family = Family::Flipper;
  v.card.confidence = Confidence::High;
  v.card.rssi = -61;
  v.backdrop = 2;
  place(home, 2, 1, v);
  v = base(); // 7. Napping.
  v.look.base = 3;
  v.name = "Wisp";
  v.mood = 2;
  v.feeling = Feeling::Napping;
  v.backdrop = 3;
  v.food = 48;
  place(home, 0, 2, v);
  v = base(Theme::Light); // 8. Light theme, level up.
  v.levelAge = 900;
  v.level = 5;
  v.xp = 0;
  v.speaking = true;
  v.message = "Ding! I feel smarter. And rounder.";
  v.mood = 3;
  place(home, 1, 2, v);
  v = base(Theme::Matrix); // 9. Matrix theme, no SD.
  v.state = "NO SD";
  v.stateUrgent = true;
  v.look.base = 1;
  v.name = "Sprout";
  place(home, 2, 2, v);
  v = base(); // 10. A long message: three tight rows, and the chin stays clear.
  v.speaking = true;
  v.message = "SD full/write failed. Drive stopped. Check the card, then tap Retry SD.";
  place(home, 0, 3, v);
  v = base(Theme::Light); // 11. Light theme alert while recording: white text on the deep red.
  v.mood = 4;
  v.alert = v.speaking = true;
  v.message = "Tracker nearby. Yours, I hope?";
  v.card.family = Family::SamsungTag;
  v.card.confidence = Confidence::High;
  v.card.rssi = -52;
  v.recording = v.fix = true;
  v.sats = 6;
  v.state = "REC";
  v.wifiSaved = 31;
  v.bleSaved = 9;
  {
    Scene s;
    s.hear(Family::SamsungTag, -52, 500, true, "", "", 1, Confidence::High);
    v.recon = s.home();
  }
  place(home, 1, 3, v);
  v = base(); // 12. A veteran: the XP caption must stay inside its panel.
  v.level = 1000;
  v.xp = 24;
  v.name = "Sixteen chars ok";
  place(home, 2, 3, v);
  home.savePpm((out + "/home.ppm").c_str());

  HostCanvas face = sheetOf(3, 4);
  for (int a = 0; a < 5; ++a) {
    static const char *names[] = {"Mochi", "Sprout", "Orbit", "Wisp", "Ember"};
    v = base(a == 4 ? Theme::Bwm : Theme::Dark);
    v.face = true;
    v.look.base = a;
    v.name = names[a];
    v.backdrop = a % 4;
    v.mood = a == 1 ? 3 : a == 3 ? 6 : a == 2 ? 1 : 0;
    if (a == 3) {
      v.feeling = Feeling::Lonely;
      v.love = 20;
      v.speaking = true;
      v.message = "Being invisible is my job, not my hobby.";
    }
    if (a == 4) {
      v.recording = v.fix = true;
      v.sats = 7;
      v.state = "REC";
      v.wifiSaved = 12;
    }
    place(face, a % 3, a / 3, v);
  }
  v = base();
  v.face = true;
  v.mood = 2;
  v.feeling = Feeling::Napping;
  place(face, 2, 1, v);
  // Custom buddies in Full Face: every hat must be visible, and speech must not hide the mouth.
  for (int hat = 1; hat <= 5; ++hat) {
    v = base();
    v.face = true;
    v.look.custom = true;
    v.look.base = hat % 5;
    v.look.body = hat == 2 ? 0x0000 : rgb565(0xbfe8ff);
    v.look.accent = rgb565(0xff8a5c);
    v.look.headwear = hat;
    v.look.eyewear = hat == 3 ? 1 : 0;
    v.look.neckwear = hat == 5 ? 2 : 0;
    v.name = "Custom";
    v.mood = hat == 4 ? 3 : hat == 5 ? 4 : 0;
    if (hat == 1 || hat == 4) {
      v.speaking = true;
      v.message = hat == 1 ? "Hehe. That's the good spot." : "You were gone 14 hours. I counted.";
    }
    if (hat == 5) {
      v.alert = v.speaking = true;
      v.message = "Possible body-camera signal nearby.";
      v.card.family = Family::Axon;
      v.card.confidence = Confidence::Medium;
      v.card.rssi = -66;
    }
    place(face, (hat - 1) % 3, 2 + (hat - 1) / 3, v);
  }
  v = base(Theme::Light); // No hat: the 2.1x close-up, round glasses, Ink eyes on Orbit's screen.
  v.face = true;
  v.look.custom = true;
  v.look.base = 2;
  v.look.body = rgb565(0xd8c8ff);
  v.look.eyewear = 1;
  v.name = "Custom";
  v.alert = v.speaking = true;
  v.mood = 0;
  v.message = "Something odd nearby. Stay curious. This one wraps onto a second row.";
  v.card.family = Family::Deauth;
  v.card.confidence = Confidence::Low;
  v.card.rssi = -74;
  place(face, 2, 3, v);
  face.savePpm((out + "/face.ppm").c_str());

  // Sound & Voice page: each voice mode, the TEST VOICE read-back, and the light theme.
  {
    HostCanvas audio = sheetOf(2, 2);
    auto page = [&](int column, int row, Theme theme, const AudioView &state) {
      AudioView view = state;
      view.theme = theme;
      view.colors = palette(theme);
      HostCanvas screen(320, 240);
      screen.fillScreen(view.colors.bg);
      homeui::text(screen, "SOUND & VOICE", 12, 12, view.colors.accent); // As ui.cpp heading().
      audioui::button(screen, view, 254, 5, 58, 27, "BACK");
      drawAudio(screen, view);
      audio.blit(screen, 4 + column * 324, 4 + row * 244);
    };
    AudioView a;
    page(0, 0, Theme::Dark, a);
    a.voice = VoiceMode::Alerts;
    a.tested = true;
    a.said = clipCaption(Clip::SpicyPig);
    a.volume = 170;
    page(1, 0, Theme::Dark, a);
    a.voice = VoiceMode::All;
    a.tested = false;
    a.tone = "Chime";
    page(0, 1, Theme::Light, a);
    a.sound = false;
    a.tested = true;
    a.said = nullptr;
    page(1, 1, Theme::Bwm, a);
    audio.savePpm((out + "/audio.ppm").c_str());
  }

  // Documentation screenshots, one screen each (site/assets/ui-*.png).
  auto shot = [&](const char *file, const HomeView &view) {
    HostCanvas screen(320, 240);
    screen.fillScreen(view.colors.bg);
    drawHome(screen, view);
    screen.savePpm((out + "/ui-" + file + ".ppm").c_str());
  };
  auto clues = [](HomeView &view) {
    Scene s;
    street(s);
    view.recon = s.home();
  };
  v = base();
  clues(v);
  shot("recon", v);
  const Theme themes[] = {Theme::Dark, Theme::Light, Theme::Matrix, Theme::Bwm};
  const char *themeFiles[] = {"dark", "light", "matrix", "bwm"};
  for (int i = 0; i < 4; ++i) {
    v = base(themes[i]);
    clues(v);
    v.speaking = true;
    v.message = "I'm not staring. I'm monitoring.";
    v.mood = 1;
    shot(themeFiles[i], v);
  }
  for (int i = 0; i < 4; ++i) {
    v = base();
    v.backdrop = i;
    v.look.base = i == 3 ? 1 : i;
    v.name = i == 0 ? "Mochi" : i == 1 ? "Sprout" : i == 2 ? "Orbit" : "Sprout";
    shot((std::string("world-") + char('0' + i)).c_str(), v);
  }
  v = base();
  v.face = true;
  v.speaking = true;
  v.message = "Tiny copilot, reporting for vibes.";
  shot("face", v);
  v = base();
  v.look.base = 3;
  v.name = "Wisp";
  v.backdrop = 2;
  v.speaking = true;
  v.message = "Boo. Did I scare the Wi-Fi?";
  shot("wisp", v);
  v = base();
  v.look.base = 4;
  v.name = "Ember";
  v.backdrop = 1;
  v.speaking = true;
  v.message = "I'm lit. Literally.";
  shot("ember", v);
  v = base();
  v.mood = 3;
  v.love = 82;
  v.speaking = true;
  v.message = "Hehe. That's the good spot.";
  shot("love", v);
  v = base();
  v.mood = 4;
  v.alert = v.speaking = true;
  v.message = "A little tracker signal just popped up!";
  v.card.family = Family::Tracker;
  v.card.confidence = Confidence::Medium;
  v.card.rssi = -59;
  clues(v);
  shot("alert", v);
  v = base();
  v.mood = 4;
  v.alert = v.demo = v.speaking = true;
  v.message = "PIG DETECTED! ACT NATURAL!";
  v.card.family = Family::Axon;
  v.card.confidence = Confidence::Medium;
  v.card.rssi = -45;
  shot("panic", v);
  HomeView kit = base();
  kit.look.custom = true;
  kit.look.base = 0;
  kit.look.body = rgb565(0xbfe8ff);
  kit.look.accent = rgb565(0xff8a5c);
  kit.look.eye = Ink;
  kit.look.eyes = 1;
  kit.look.headwear = 5;
  kit.look.neckwear = 1;
  kit.name = "Signal Scout";
  kit.kit = 1;
  shot("fieldkit", kit);
  v = kit;
  v.mood = 4;
  v.alert = v.speaking = true;
  v.message = "Smart glasses nearby. Act cool.";
  v.card.family = Family::Meta;
  v.card.confidence = Confidence::High;
  v.card.label = "Ray-Ban Meta";
  v.card.rssi = -57;
  shot("fieldkit-alert", v);
  v = base();
  v.recording = v.fix = true;
  v.sats = 8;
  v.state = "REC";
  v.kit = 1;
  v.wifiSaved = 64;
  v.bleSaved = 21;
  v.speaking = true;
  v.message = "Logging! Show me the good streets.";
  shot("recording", v);
  v.speaking = false;
  v.moving = true;
  v.kit = 2;
  v.feeling = Feeling::Hyped;
  v.wifiSaved = 212;
  v.bleSaved = 58;
  v.level = 12;
  v.backdrop = 1;
  shot("drive", v);
}
