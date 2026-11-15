#include "detector.h"
#include "encounters.h"
#include "gps_support.h"
#include "home_view.h"
#include "idle_motion.h"
#include "nutrition.h"
#include "personality.h"
#include "petting.h"
#include "profile.h"
#include "progression.h"
#include "traffic.h"
#include "travel.h"
#include "upload_csv.h"
#include "watchlist.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <vector>
using namespace dash;
#define CHECK(expr)                                                                                \
  do {                                                                                             \
    if (!(expr)) {                                                                                 \
      std::cerr << "CHECK failed at " << __LINE__ << ": " << #expr << '\n';                        \
      std::exit(1);                                                                                \
    }                                                                                              \
  } while (0)
static std::vector<uint8_t> beacon(const char *ssid) {
  std::vector<uint8_t> p(36, 0);
  p[0] = 0x80;
  p[16] = 0x10;
  p[17] = 0x20;
  p[18] = 0x30;
  p[19] = 1;
  p[20] = 2;
  p[21] = 3;
  p.push_back(0);
  p.push_back(strlen(ssid));
  p.insert(p.end(), ssid, ssid + strlen(ssid));
  p.insert(p.end(), {3, 1, 6});
  return p;
}
static void ad(std::vector<uint8_t> &p, uint8_t type, std::initializer_list<uint8_t> d) {
  p.push_back(d.size() + 1);
  p.push_back(type);
  p.insert(p.end(), d);
}
static std::string sentence(const std::string &body) {
  uint8_t sum = 0;
  for (char c : body)
    sum ^= c;
  char suffix[8];
  snprintf(suffix, sizeof(suffix), "*%02X\r\n", sum);
  return "$" + body + suffix;
}
static bool feed(NmeaFrame &frame, const std::string &input) {
  bool decoded = false;
  for (char c : input)
    decoded |= frame.feed(c);
  return decoded;
}
static void checkGps() {
  GpsConfig cfg;
  CHECK(validGpsConfig(cfg) && gpsRxPin(cfg) == 18);
  cfg.port = 1;
  CHECK(gpsRxPin(cfg) == 1);
  cfg.port = 3;
  CHECK(gpsRxPin(cfg) == 8);
  cfg.port = 2;
  for (auto pin : GpsInputPins) {
    cfg.rx = pin;
    CHECK(validGpsConfig(cfg) && gpsRxPin(cfg) == pin);
  }
  for (int pin : {0, 4, 6, 7, 11, 12, 19, 20, 35, 36, 37, 255}) {
    cfg.rx = pin;
    CHECK(!validGpsConfig(cfg));
  }
  cfg.rx = 18;
  for (unsigned i = 0; i < GpsPresetCount; ++i) {
    cfg.profile = i;
    cfg.baud = GpsPresets[i].baud;
    CHECK(validGpsConfig(cfg));
  }
  cfg.profile = GpsPresetCount;
  CHECK(!validGpsConfig(cfg));
  cfg.profile = 0;
  cfg.baud = 12345;
  CHECK(!validGpsConfig(cfg));
  cfg.baud = 9600;
  cfg.port = 4;
  CHECK(!validGpsConfig(cfg));
  for (const char *talker : {"GP", "GN", "GA", "GL", "GB", "BD", "GQ", "GI"}) {
    NmeaFrame f;
    const std::string body = "RMC,123519.00,V,,,,,,,171026,,,N"; // No satellite fix required.
    CHECK(feed(f, sentence(std::string(talker) + body)));
    auto expected = sentence("GP" + body);
    expected.erase(expected.size() - 2, 1); // Normalized output ends with LF.
    CHECK(std::string(f.line()) == expected);
    CHECK(f.good == 1 && f.bad == 0);
  }
  NmeaFrame f;
  CHECK(!feed(f, sentence("GNGSV,1,1,00"))); // Valid but not fix-bearing.
  CHECK(!feed(f, sentence("XXGGA,1,2,3")));
  CHECK(!feed(f, sentence("PTEST,1,2,3")));
  CHECK(f.good == 3 && f.bad == 0);
  auto bad = sentence("GNRMC,123519.00,V,,,,,,,171026,,,N");
  bad[8] ^= 1;
  CHECK(!feed(f, bad) && f.bad == 1);
  CHECK(!feed(f, "$GNRMC,0*XX\n") && f.bad == 2);
  CHECK(!feed(f, "$GNGGA,0\n") && f.bad == 3);
  CHECK(!feed(f, "$" + std::string(180, 'A') + "\n") && f.bad == 4);
  CHECK(!feed(f, std::string("$GNRMC,") + char(0) + "\n") && f.bad == 5);
  CHECK(feed(f, "noise$partial" + sentence("GNGGA,1,2,3"))); // Re-sync at next '$'.
  CHECK(f.good == 4);
  GpsBaudSearch search;
  CHECK(!search.sentence() && search.tick(10000) == 0);
  search.start(0xfffffff0);
  CHECK(search.baud() == 9600 && search.tick(3483) == 0);
  CHECK(!search.sentence());
  CHECK(search.tick(3484) == 1 && search.baud() == 115200);
  CHECK(!search.sentence()); // Hits from a previous baud cannot count.
  CHECK(search.sentence() && !search.active());
  CHECK(!search.sentence() && search.tick(20000) == 0);
  search.start(0);
  for (unsigned i = 1; i < GpsRateCount; ++i)
    CHECK(search.tick(3500 * i) == 1);
  CHECK(search.tick(3500 * GpsRateCount) == -1 && !search.active());
  search.start(0);
  search.cancel();
  CHECK(search.tick(3500) == 0 && !search.sentence());
}
int main() {
  checkGps();
  PetTouch head;
  CHECK(!head.update(0, false));
  CHECK(!head.update(600, false));
  CHECK(!head.update(700, true));
  CHECK(!head.update(710, false));
  CHECK(!head.update(800, true));
  CHECK(!head.update(900, false));
  CHECK(!head.update(1500, false));
  CHECK(!head.update(1600, true));
  CHECK(!head.update(1849, true));
  CHECK(head.update(1850, true));
  CHECK(!head.update(99999, true));
  CHECK(!head.update(100000, false));
  CHECK(!head.update(100600, false));
  CHECK(!head.update(100700, true));
  CHECK(head.update(100950, true));
  AvatarProfile profile;
  CHECK(parseAvatar("DCAV2 3 000000 C5BFFF 0 EDFFFF 0 0 0 Wisp", profile));
  CHECK(profile.base == 3 && profile.body == 0);
  CHECK(parseAvatar("DCAV2 4 FF4D24 FFE040 1 201029 0 0 0 Ember", profile));
  CHECK(profile.base == 4);
  CHECK(!parseAvatar("DCAV2 5 FFFFFF FFFFFF 0 000000 0 0 0 Nope", profile));
  CHECK(parseAvatar("DCAV2 2 112233 FFAACC 4 99FFDD 5 3 2 Field Goblin", profile));
  CHECK(profile.base == 2 && profile.eyes == 4 && profile.eye == 0x99ffdd &&
        profile.headwear == 5 && profile.eyewear == 3 && profile.neckwear == 2);
  CHECK(!parseAvatar("DCAV2 0 FFFFFF FFFFFF 6 112233 0 0 0 Nope", profile));
  CHECK(!parseAvatar("DCAV2 0 FFFFFF FFFFFF 0 112233 6 0 0 Nope", profile));
  CHECK(!parseAvatar("DCAV2 0 FFFFFF FFFFFF 0 112233 0 4 0 Nope", profile));
  CHECK(!parseAvatar("DCAV2 0 FFFFFF FFFFFF 0 112233 0 0 4 Nope", profile));
  CHECK(!parseAvatar("DCAV2 0 FFFFFF FFFFFF 0 112233 0 0 0 Bad;name", profile));
  CHECK(!parseAvatar("DCAV3 0 FFFFFF FFFFFF 0 112233 0 0 0 Nope", profile));
  CHECK(parseAvatar("DCAV1 2 FFFFFF FFAABB 2 Old Buddy", profile));
  CHECK(profile.eyes == 0 && profile.neckwear == 1 && profile.headwear == 0 &&
        profile.eye == 0x84f7b5);
  CHECK(parseAvatar("DCAV1 0 FFFFFF FFAABB 3 Old Crown", profile));
  CHECK(profile.headwear == 2 && profile.neckwear == 0);
  TravelState travel;
  CHECK(!travel.update(100, true, 1.5));
  CHECK(!travel.update(1000, true, 8));
  CHECK(!travel.update(3999, true, 8));
  CHECK(travel.update(4000, true, 8));
  CHECK(travel.update(5000, true, 3)); // Retain travel inside the hysteresis band.
  CHECK(travel.update(6000, true, 1));
  CHECK(travel.update(10999, true, 1));
  CHECK(!travel.update(11000, true, 1));
  CHECK(!travel.update(12000, true, 30));
  CHECK(travel.update(15000, true, 30));
  CHECK(!travel.update(15001, false, 30)); // GPS loss clears animation immediately.
  CHECK(!travel.update(16000, true, NAN));
  CHECK(!travel.update(17000, true, 301));
  CHECK(!travel.update(0xfffffff0, true, 20));
  CHECK(travel.update(2984, true, 20));
  CHECK(fieldScene(0, false, false) == 0 && fieldScene(0, true, false) == 1);
  CHECK(fieldScene(0, false, true) == 2 && fieldScene(1, false, false) == 1);
  CHECK(fieldScene(2, true, true) == 0);
  CHECK(parseAvatar("DCAV1 0 FFFFD0 FF88B6 1 My Mochi", profile));
  CHECK(profile.base == 0 && profile.accessory == 1 && !strcmp(profile.name, "My Mochi"));
  CHECK(!parseAvatar("DCAV1 3 FFFFFF FF8800 0 Nope", profile));
  CHECK(!parseAvatar("DCAV1 0 FFFFFF FF8800 4 Nope", profile));
  CHECK(!parseAvatar("DCAV1 0 FFFFFF FF8800 1 Name; reboot", profile));
  CHECK(!parseAvatar("DCAV1 0 FFFFFF FF8800 1 This-name-is-much-too-long", profile));
  CHECK(!parseAvatar("DCAV1 0 FFFFFF FF8800 1    ", profile));
  Encounters encounters;
  uint8_t known[] = {0, 0x25, 0xdf, 0, 0, 1};
  auto &encounter = encounters.observe(known, true, 0, Family::Axon, 0, 300000);
  CHECK(encounters.eligible(encounter, Confidence::Medium, 0, 60000, false));
  encounters.announce(encounter, Confidence::Medium, 0);
  for (uint32_t t = 1000; t <= 600000; t += 1000) {
    auto &same = encounters.observe(known, true, 0, Family::Axon, t, 300000);
    CHECK(!encounters.eligible(same, Confidence::Medium, t, 60000, false));
  }
  CHECK(encounters.eligible(encounter, Confidence::High, 600000, 60000, false));
  CHECK(encounters.eligible(encounter, Confidence::Medium, 600000, 60000, true));
  auto &returned = encounters.observe(known, true, 0, Family::Axon, 900001, 300000);
  CHECK(encounters.eligible(returned, Confidence::Medium, 900001, 60000, false));
  encounters.announce(returned, Confidence::Medium, 900001);
  known[5] = 2;
  auto &rotating = encounters.observe(known, true, 1, Family::Axon, 910001, 300000);
  CHECK(!encounters.eligible(rotating, Confidence::Medium, 910001, 60000, false));
  CHECK(encounters.eligible(rotating, Confidence::Medium, 960001, 60000, false));
  Watchlist list;
  CHECK(list.add("MAC,0025df000002"));
  CHECK(list.match(known, false, "").family == Family::Watchlist);
  CHECK(!list.add("OUI,nothex"));
  CHECK(!list.add("NAME,"));
  list.clear();
  CHECK(list.add("OUI,0025DF"));
  CHECK(list.match(known, true, "").family == Family::Watchlist);
  CHECK(list.match(known, false, "").family == Family::None);
  list.clear();
  CHECK(list.add("NAME,Mochi"));
  CHECK(list.match(known, false, "MY MOCHI").family == Family::Watchlist);
  list.clear();
  CHECK(list.add("COMPANY,034D"));
  uint8_t company[] = {3, 0xff, 0x4d, 3};
  CHECK(list.match(known, false, "", company, sizeof(company)).family == Family::Watchlist);
  list.clear();
  CHECK(list.add("SERVICE,FC81"));
  uint8_t service[] = {3, 3, 0x81, 0xfc};
  CHECK(list.match(known, false, "", service, sizeof(service)).family == Family::Watchlist);
  uint8_t badAd[] = {5, 3, 0x81};
  CHECK(list.match(known, false, "", badAd, sizeof(badAd)).family == Family::None);
  uint8_t baseService[] = {17,   7, 0xfb, 0x34, 0x9b, 0x5f, 0x80, 0, 0,
                           0x80, 0, 0x10, 0,    0,    0x81, 0xfc, 0, 0};
  CHECK(list.match(known, false, "", baseService, sizeof(baseService)).family == Family::Watchlist);
  baseService[2] ^= 1;
  CHECK(list.match(known, false, "", baseService, sizeof(baseService)).family == Family::None);
  std::vector<uint8_t> probe(26, 0);
  probe[0] = 0x40;
  memcpy(probe.data() + 10, known, 6);
  auto observed = classifyWifi(probe.data(), probe.size());
  CHECK(observed.valid && !observed.loggable && observed.match.family == Family::Axon);
  std::vector<uint8_t> dataHeader(24, 0);
  dataHeader[0] = 8;
  memcpy(dataHeader.data() + 4, known, 6);
  observed = classifyWifi(dataHeader.data(), dataHeader.size());
  CHECK(observed.valid && !observed.loggable && observed.match.family == Family::Axon &&
        !memcmp(observed.mac, known, 6));
  // Wildcard probe fingerprint needs the research OUI AND the complete IE sequence.
  std::vector<uint8_t> flockProbe(24, 0);
  flockProbe[0] = 0x40;
  uint8_t flockMac[] = {0x70, 0xc9, 0x4e, 1, 2, 3};
  memcpy(flockProbe.data() + 10, flockMac, 6);
  flockProbe.insert(flockProbe.end(),
                    {0, 0, 2,  0, 12,  0, 127, 0, 221, 7,    0x50, 0x6f, 0x9a, 0x16, 3,
                     1, 3, 45, 0, 191, 0, 221, 7, 0,   0x50, 0xf2, 8,    0,    0,    0});
  observed = classifyWifi(flockProbe.data(), flockProbe.size());
  CHECK(observed.valid && !observed.loggable && observed.match.family == Family::Flock &&
        observed.match.confidence == Confidence::Medium);
  flockProbe.back() = 1;
  CHECK(classifyWifi(flockProbe.data(), flockProbe.size()).match.confidence < Confidence::Medium);
  flockProbe.pop_back();
  CHECK(!classifyWifi(flockProbe.data(), flockProbe.size()).valid);
  // Multiple APs sharing an SSID are ordinary; an open/protected conflict is only a weak hint.
  TrafficHints conflict;
  auto openAp = beacon("Test mesh");
  auto protectedAp = openAp;
  protectedAp[21] = 4;
  for (uint32_t i = 0; i < 4; ++i) {
    CHECK(conflict.wifi(openAp.data(), openAp.size(), 100 + i).family == Family::None);
    CHECK(conflict.wifi(protectedAp.data(), protectedAp.size(), 200 + i).family == Family::None);
  }
  protectedAp[34] = 0x10;
  for (uint32_t i = 0; i < 3; ++i) {
    auto hit = conflict.wifi(protectedAp.data(), protectedAp.size(), 300 + i);
    CHECK((hit.family == Family::EvilTwin) == (i == 2));
    if (i == 2)
      CHECK(hit.confidence == Confidence::Low);
  }
  CHECK(conflict.wifi(protectedAp.data(), protectedAp.size(), 31000).family == Family::None);
  TrafficHints traffic;
  std::vector<uint8_t> disconnect(26, 0);
  disconnect[0] = 0xc0;
  memcpy(disconnect.data() + 10, known, 6);
  for (unsigned i = 1; i <= 12; ++i) {
    disconnect[22] = i;
    auto hit = traffic.wifi(disconnect.data(), disconnect.size(), i * 30);
    CHECK((hit.family == Family::Deauth) == (i == 12));
  }
  TrafficHints pairing;
  uint8_t popup[] = {5, 0xff, 0x4c, 0, 7, 0};
  for (unsigned i = 0; i < 23; ++i) {
    known[5] = i;
    CHECK(pairing.ble(popup, sizeof(popup), known, 100 + i).family == Family::None);
  }
  known[5] = 23;
  CHECK(pairing.ble(popup, sizeof(popup), known, 124).family == Family::BleSpam);
  CHECK(pairing.ble(popup, sizeof(popup), known, 6000).family == Family::None);
  uint8_t mac[] = {0x10, 0x20, 0x30, 1, 2, 3};
  auto p = beacon("Cafe, \"Mochi\"");
  auto w = classifyWifi(p.data(), p.size());
  CHECK(w.valid && w.channel == 6 && !strcmp(w.auth, "[OPEN][ESS]"));
  p.push_back(48);
  p.push_back(2);
  p.push_back(1);
  CHECK(!classifyWifi(p.data(), p.size()).valid);
  p = beacon("secure");
  p[34] = 0x10;
  p.insert(p.end(),
           {48, 20, 1, 0, 0, 0x0f, 0xac, 4, 1, 0, 0, 0x0f, 0xac, 4, 1, 0, 0, 0x0f, 0xac, 2, 0, 0});
  w = classifyWifi(p.data(), p.size());
  CHECK(w.valid && strstr(w.auth, "WPA2-PSK"));
  for (size_t i = 0; i < 36; ++i)
    CHECK(!classifyWifi(p.data(), i).valid);
  p = beacon("normal");
  p.insert(p.end(), {48, 2, 1, 0});
  w = classifyWifi(p.data(), p.size());
  CHECK(!w.valid);
  std::vector<uint8_t> b;
  ad(b, 0xff, {0x4c, 0, 2, 0x15, 0, 0, 0});
  auto f = classifyBle(b.data(), b.size(), mac, true);
  CHECK(f.valid && f.match.confidence < Confidence::Medium);
  b.clear();
  ad(b, 0xff, {0x4d, 3});
  ad(b, 3, {0x6b, 0xfe});
  f = classifyBle(b.data(), b.size(), mac, true);
  CHECK(f.valid && f.match.family == Family::Axon && f.match.confidence >= Confidence::Medium);
  b.clear();
  ad(b, 3, {0x81, 0x30});
  f = classifyBle(b.data(), b.size(), mac, true);
  CHECK(f.valid && f.match.family == Family::Flipper);
  b.clear();
  ad(b, 3, {0x5f, 0xfd});
  f = classifyBle(b.data(), b.size(), mac, true);
  CHECK(f.valid && f.match.family == Family::Meta);
  CHECK(f.match.confidence == Confidence::Low);
  ad(b, 0xff, {0x53, 0x0d});
  f = classifyBle(b.data(), b.size(), mac, true);
  CHECK(f.valid && f.match.family == Family::Meta && f.match.confidence == Confidence::Medium);
  b.clear();
  ad(b, 3, {0xaa, 0xfe});
  f = classifyBle(b.data(), b.size(), mac, true);
  CHECK(f.valid && f.match.confidence < Confidence::Medium);
  b.clear();
  ad(b, 0x16, {0xaa, 0xfe, 0x40, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
  f = classifyBle(b.data(), b.size(), mac, true);
  CHECK(f.valid);
  b.clear();
  ad(b, 7,
     {0x4b, 0x91, 0x31, 0xc3, 0xc9, 0xc5, 0xcc, 0x8f, 0x9e, 0x45, 0xb5, 0x1f, 1, 0xc2, 0xaf, 0x4f});
  ad(b, 9, {'B', 'i', 's', 'c', 'u', 'i', 't'});
  f = classifyBle(b.data(), b.size(), mac, true);
  CHECK(f.valid && f.match.family == Family::Biscuit && f.match.confidence == Confidence::Medium);
  b.clear();
  ad(b, 0xff, {0x4c, 0, 0x12, 0x19, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
               0,    0, 0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
  f = classifyBle(b.data(), b.size(), mac, true);
  CHECK(f.valid && f.match.family == Family::Tracker);
  b = {5, 9, 'a'};
  CHECK(!classifyBle(b.data(), b.size(), mac, true).valid);
  uint8_t axonMac[] = {0, 0x25, 0xdf, 1, 2, 3};
  b = {2, 1, 6};
  CHECK(classifyBle(b.data(), b.size(), axonMac, false).match.confidence < Confidence::Medium);
  CHECK(classifyBle(b.data(), b.size(), axonMac, true).match.family == Family::Axon);
  Fix fix;
  fix.valid = true;
  fix.sats = 7;
  fix.hdop = 1.2;
  fix.lat = 40;
  fix.lon = -74;
  fix.alt = 25;
  strcpy(fix.utc, "2026-10-17 01:02:03");
  CHECK(usableFix(fix));
  auto line = wigleRow(mac, "Cafe, \"Mochi\"", "[OPEN][ESS]", 6, -57, false, fix);
  CHECK(line.find("\"Cafe, \"\"Mochi\"\"\"") != std::string::npos);
  std::ofstream file("build/test-wigle.csv");
  file << wigleHeader() << line << wigleRow(mac, "Flipper", "[BLE]", 0, -42, true, fix);
  file.close();
  fix.age = 2001;
  CHECK(!usableFix(fix) && wigleRow(mac, "x", "[BLE]", 0, -50, true, fix).empty());
  fix.age = 0;
  fix.hdop = 9;
  CHECK(!usableFix(fix));
  fix.hdop = 1;
  fix.lat = NAN;
  CHECK(!usableFix(fix));
  fix.lat = 0;
  fix.lon = 0;
  CHECK(usableFix(fix));
  fix.sats = 3;
  CHECK(!usableFix(fix));
  Session s;
  CHECK(!s.accepts(0));
  s.start();
  auto first = s.generation;
  CHECK(s.accepts(first) && !s.accepts(0));
  s.stop();
  CHECK(!s.accepts(first));
  s.start();
  CHECK(!s.accepts(first));
  Cooldown c;
  CHECK(c.ready(0, 60000));
  c.mark(0);
  CHECK(!c.ready(59999, 60000));
  CHECK(c.ready(60000, 60000));
  c.mark(0xfffffff0);
  CHECK(!c.ready(5, 30));
  CHECK(c.ready(15, 30));
  WardriveFood food;
  CHECK(food.discoveries() == 0 && food.progress() == 0);
  for (uint8_t i = 0; i < 5; ++i) {
    uint8_t network[] = {0x02, 0x10, 0x20, 0x30, 0x40, i};
    CHECK(food.discover(network));
    for (unsigned repeat = 0; repeat < 100; ++repeat)
      CHECK(!food.discover(network));
    CHECK(food.discoveries() == unsigned(i + 1));
    CHECK(food.progress() == unsigned(i + 1) % 5);
  }
  // The contact cache expiring has no effect on this per-session memory.
  uint8_t firstNetwork[] = {0x02, 0x10, 0x20, 0x30, 0x40, 0};
  CHECK(!food.discover(firstNetwork));
  food.reset();
  CHECK(food.discoveries() == 0 && food.discover(firstNetwork));
  std::mt19937 random(7);
  for (unsigned i = 0; i < 32; ++i) {
    auto cue = panicMotion(i);
    CHECK(cue.yaw >= -320 && cue.yaw <= 320);
    CHECK(cue.pitch >= 130 && cue.pitch <= 380);
    CHECK(cue.speed <= 600 && cue.holdMs >= 300);
  }
  // Every relaxed gesture stays in its narrower travel envelope, returns to
  // center, and leaves an actual rest. A reset cancels an old gesture.
  for (unsigned avatar = 0; avatar < 5; ++avatar) {
    for (unsigned sample = 0; sample < 256; ++sample) {
      IdleMotion idle;
      uint32_t now = 0xfffffff0;
      uint32_t seed = random();
      idle.reset(now, 100);
      MotionCue cue;
      CHECK(!idle.tick(now + 99, seed, sample & 1, avatar, false, cue));
      now += 100;
      unsigned steps = 0;
      while (idle.tick(now, seed, sample & 1, avatar, false, cue)) {
        CHECK(++steps <= 3);
        CHECK(cue.yaw >= -200 && cue.yaw <= 200);
        CHECK(cue.pitch >= 145 && cue.pitch <= 310);
        CHECK(cue.speed >= 75 && cue.speed <= 180);
        CHECK(cue.holdMs >= 1000 && cue.holdMs <= 2500);
        CHECK(!idle.tick(now, seed, sample & 1, avatar, false, cue));
        now += cue.holdMs;
      }
      CHECK(steps >= 2 && cue.yaw == 0 && cue.pitch >= 190 && cue.pitch <= 200);
      idle.reset(now, 4500);
      CHECK(!idle.tick(now + 4499, seed, false, avatar, false, cue));
      CHECK(idle.tick(now + 4500, seed, false, avatar, false, cue));
    }
  }
  CHECK(petLevel(0) == 1 && petLevel(24) == 1 && petLevel(25) == 2);
  CHECK(levelProgress(24) == 24 && levelProgress(25) == 0);
  uint32_t xp = UINT32_MAX;
  earnDiscoveryXp(xp);
  CHECK(xp == UINT32_MAX);
  xp = 24;
  earnDiscoveryXp(xp);
  CHECK(xp == 25 && petLevel(xp) == 2);
  std::vector<std::string> csvFields;
  std::string upgrade;
  CHECK(upgradeCsvRow(
      "02:00:00:00:00:01,\"Cafe, \"\"Mochi\"\"\",[WPA2],2026-10-17 12:00:00,6,-40,40,-75,12,5,WIFI",
      upgrade));
  CHECK(parseCsvLine(upgrade.substr(0, upgrade.size() - 1), csvFields));
  CHECK(csvFields.size() == 14 && csvFields[1] == "Cafe, \"Mochi\"" && csvFields[5] == "2437" &&
        csvFields[11].empty() && csvFields[12] == "0");
  CHECK(
      !upgradeCsvRow("02:00:00:00:00:01,b,c,2026-10-17 12:00:00,6,-40,NaN,-75,12,5,WIFI", upgrade));
  CHECK(
      !upgradeCsvRow("02:00:00:00:00:01,b,c,2026-10-17 12:00:00,6,-40,91,-75,12,5,WIFI", upgrade));
  CHECK(
      !upgradeCsvRow("02:00:00:00:00:01,b,c,2026-10-17 12:00:00,99,-40,40,-75,12,5,WIFI", upgrade));
  CHECK(!parseCsvLine("\"broken", csvFields));
  CHECK(!parseCsvLine("\"closed\"junk", csvFields));
  // Personality: every line fits the speech strip, and no pool repeats itself.
  for (const auto &l : Lines) {
    CHECK(*l.text && strlen(l.text) <= BubbleChars);
    for (const char *c = l.text; *c; ++c)
      CHECK(*c >= 32 && *c <= 126 && *c != '%');
  }
  for (unsigned t = 0; t < TopicCount; ++t)
    for (unsigned avatar = 0; avatar < 5; ++avatar)
      for (bool spicy : {false, true}) {
        unsigned n = Banter::count(Topic(t), avatar, spicy);
        CHECK(n >= 3 && n <= 32);
        Banter deck;
        std::string previous;
        for (unsigned round = 0; round < 4; ++round) {
          std::set<std::string> bag;
          for (unsigned i = 0; i < n; ++i) {
            std::string said = deck.pick(Topic(t), avatar, spicy, random());
            CHECK(!said.empty() && said != previous);
            previous = said;
            bag.insert(said);
          }
          CHECK(bag.size() == n); // Every line is heard once before any line comes back.
        }
      }
  CHECK(Banter::count(Topic::Idle, 0, true) > Banter::count(Topic::Idle, 0, false));
  CHECK(!*Banter().pick(Topic::Count, 0, true, 1));
  {
    Vitals v;
    CHECK(feelingFor(v) == Feeling::Content);
    v.quietMs = BoredAfterMs;
    CHECK(feelingFor(v) == Feeling::Bored);
    v.quietMs = DrowsyAfterMs;
    CHECK(feelingFor(v) == Feeling::Drowsy);
    v.bond = LonelyBelow - 1;
    CHECK(feelingFor(v) == Feeling::Lonely);
    v.energy = HungryBelow - 1;
    CHECK(feelingFor(v) == Feeling::Hungry);
    v.quietMs = NapAfterMs;
    CHECK(feelingFor(v) == Feeling::Napping);
    v.onTheRoad = true; // No naps or sulking mid-drive, but hunger still shows.
    CHECK(feelingFor(v) == Feeling::Hungry);
    v.energy = 80;
    CHECK(feelingFor(v) == Feeling::Content);
    v.recentFinds = HypedFinds;
    CHECK(feelingFor(v) == Feeling::Hyped);
    uint8_t fuel = 50, bond = 50;
    for (uint32_t minute = 1; minute <= 10; ++minute)
      decayNeeds(fuel, bond, Feeling::Content, false, minute);
    CHECK(fuel == 40 && bond == 45);
    for (uint32_t minute = 1; minute <= 10; ++minute)
      decayNeeds(fuel, bond, Feeling::Content, true, minute);
    CHECK(fuel == 30 && bond == 45); // Driving together never costs affection.
    for (uint32_t minute = 1; minute <= 10; ++minute)
      decayNeeds(fuel, bond, Feeling::Napping, false, minute);
    CHECK(fuel == 28 && bond == 44); // Asleep: a slow trickle only.
    fuel = bond = 10;
    decayNeeds(fuel, bond, Feeling::Content, false, 2);
    CHECK(fuel == 10 && bond == 10);
    PetCombo combo;
    CHECK(combo.pet(1000) == 1 && combo.pet(2000) == 2 && combo.pet(3000) == 3);
    CHECK(combo.pet(3000 + PetComboWindowMs) == 1);
    PetCombo wrap;
    CHECK(wrap.pet(0xfffffff0u) == 1 && wrap.pet(0x10) == 2);
    CHECK(petBondGain(1) == 4 && petBondGain(GiggleAtStreak) == 2 &&
          petBondGain(EnoughAtStreak) == 0);
    CHECK(petTopic(1, Feeling::Napping, false, 70, 0) == Topic::Wake);
    CHECK(petTopic(EnoughAtStreak, Feeling::Content, false, 70, 0) == Topic::PetEnough);
    CHECK(petTopic(GiggleAtStreak, Feeling::Content, false, 70, 0) == Topic::PetGiggle);
    CHECK(petTopic(1, Feeling::Lonely, false, 20, 0) == Topic::PetMissedYou);
    CHECK(petTopic(1, Feeling::Content, true, 70, 1) == Topic::PetDriving);
    CHECK(petTopic(1, Feeling::Hungry, false, 70, 1) == Topic::PetHungry);
    CHECK(petTopic(1, Feeling::Content, false, AdoreAtBond, 0) == Topic::PetAdore);
    CHECK(petTopic(1, Feeling::Content, false, AdoreAtBond - 1, 0) == Topic::Pet);
    RoadView road;
    CHECK(chatterTopic(Feeling::Napping, road, 1) == Topic::NapTalk);
    CHECK(chatterTopic(Feeling::Hungry, road, 1) == Topic::IdleHungry);
    CHECK(chatterTopic(Feeling::Bored, road, 1) == Topic::IdleBored);
    road.moving = true;
    CHECK(chatterTopic(Feeling::Content, road, 0) == Topic::NotLogging);
    road.recording = true;
    CHECK(chatterTopic(Feeling::Content, road, 1) == Topic::Cruise);
    road.sinceFindMs = DryRoadMs;
    CHECK(chatterTopic(Feeling::Content, road, 1) == Topic::DryRoad);
    road.moving = false;
    road.stoppedMs = RedLightMs;
    road.fix = true;
    CHECK(chatterTopic(Feeling::Content, road, 1) == Topic::RedLight);
    road.fix = false;
    road.noFixMs = NoSkyMs;
    CHECK(chatterTopic(Feeling::Content, road, 1) == Topic::NoSky);
    for (uint32_t r = 0; r < 1000; r += 37) {
      CHECK(chatterGapMs(Feeling::Content, false, r) >= 30000);
      CHECK(chatterGapMs(Feeling::Napping, false, r) >= 120000);
    }
    CHECK(milestone(10) && milestone(100) && milestone(3000) && !milestone(0) && !milestone(11));
    char line[64];
    for (uint32_t r = 0; r < 6; ++r) {
      petMilestoneLine(line, sizeof(line), 4000000000u, r);
      CHECK(strlen(line) <= BubbleChars);
      findMilestoneLine(line, sizeof(line), 4000000000u, r);
      CHECK(strlen(line) <= BubbleChars);
      CHECK(ssidLine(line, sizeof(line), "An absurdly long network name % with\x01junk", r));
      CHECK(strlen(line) <= BubbleChars && !strchr(line, '%') && !strchr(line, 1));
      CHECK(ssidLine(line, sizeof(line), "Cafe", r) && strstr(line, "'Cafe'"));
    }
    CHECK(!ssidLine(line, sizeof(line), "", 0) && !ssidLine(line, sizeof(line), nullptr, 0));
    CHECK(!ssidLine(line, sizeof(line), "  \x02 ", 0));
    CHECK(!driveSummaryLine(line, sizeof(line), 0, false));
    for (uint32_t finds : {1u, 24u, 149u, 4000000000u})
      for (bool record : {false, true})
        CHECK(driveSummaryLine(line, sizeof(line), finds, record) && strlen(line) <= BubbleChars);
    CHECK(driveSummaryLine(line, sizeof(line), 42, true) && strstr(line, "Record"));
    // Time apart: parsing, clamping, floors and the welcome-back line.
    CHECK(utcSeconds("2000-01-01 00:00:00") == 0); // Before the supported range.
    CHECK(utcSeconds("2024-01-01 00:00:00") == 8766u * 86400u);
    CHECK(utcSeconds("2024-03-01 00:00:00") - utcSeconds("2024-02-28 00:00:00") == 2 * 86400u);
    CHECK(utcSeconds("2025-03-01 00:00:00") - utcSeconds("2025-02-28 00:00:00") == 86400u);
    CHECK(utcSeconds("2026-11-15 12:30:05") - utcSeconds("2026-11-14 12:30:00") == 86405u);
    CHECK(!utcSeconds("") && !utcSeconds(nullptr) && !utcSeconds("2026-13-01 00:00:00") &&
          !utcSeconds("garbage") && !utcSeconds("1999-01-01 00:00:00"));
    uint32_t then = utcSeconds("2026-11-01 08:00:00");
    CHECK(awayHours(then, then + 3 * 3600 + 59) == 3 && !awayHours(0, then) && !awayHours(then, 0));
    CHECK(!awayHours(then, then - 500) && awayHours(then, then + 400u * 86400u) == MaxAwayHours);
    fuel = 80;
    bond = 70;
    applyAway(fuel, bond, 10);
    CHECK(fuel == 60 && bond == 60);
    applyAway(fuel, bond, MaxAwayHours);
    CHECK(fuel == 15 && bond == 20);
    fuel = 12;
    bond = 14;
    applyAway(fuel, bond, 50); // Already below the floor: absence never raises or lowers it.
    CHECK(fuel == 12 && bond == 14);
    CHECK(!awayLine(line, sizeof(line), MissedYouAfterHours - 1, 0));
    for (uint32_t r = 0; r < 3; ++r)
      for (uint32_t hours : {MissedYouAfterHours, 47u, 48u, MaxAwayHours})
        CHECK(awayLine(line, sizeof(line), hours, r) && strlen(line) <= BubbleChars);
    CHECK(strlen(FirstHello) <= BubbleChars);
    CHECK(!strcmp(rankName(1), "Hatchling") && !strcmp(rankName(4), "Rookie") &&
          !strcmp(rankName(5), "Scout") && !strcmp(rankName(1000), "Legend"));
    // Speech bubbles: any line the buddy can say must wrap into the bubble without loss.
    for (const auto &l : Lines)
      for (int width : {26, 47}) {
        char rows[3][64];
        int n = homeui::wrapLines(l.text, width, rows, 3);
        std::string joined;
        for (int i = 0; i < n; ++i) {
          CHECK(int(strlen(rows[i])) <= width);
          joined += (i ? " " : "") + std::string(rows[i]);
        }
        CHECK(joined == l.text);
      }
  }
  for (int i = 0; i < 50000; ++i) {
    size_t n = random() % 768;
    std::vector<uint8_t> q(n);
    for (auto &v : q)
      v = random();
    classifyBle(q.data(), q.size(), mac, true);
    classifyWifi(q.data(), q.size());
  }
  std::cout << "Core regression checks and 50,000 malformed-packet cases passed\n";
}
