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
#include "signature_tests.h"
#include "recon_tests.h"
#include "studio_tests.h"
#include "traffic.h"
#include "travel.h"
#include "upload_csv.h"
#include "voice_tests.h"
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
  studio_tests::run();
  checkGps();
  checkVoice();
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
  // Multiple APs sharing an SSID are ordinary; an open/protected conflict between two makers
  // is a hint, never proof. The same-maker case is in signature_tests.h.
  TrafficHints conflict;
  auto openAp = beacon("Test mesh");
  auto protectedAp = openAp;
  protectedAp[17] = 0x21;
  for (uint32_t i = 0; i < 4; ++i) {
    CHECK(conflict.wifi(openAp.data(), openAp.size(), 100 + i).family == Family::None);
    CHECK(conflict.wifi(protectedAp.data(), protectedAp.size(), 200 + i).family == Family::None);
  }
  protectedAp[34] = 0x10;
  for (uint32_t i = 0; i < 3; ++i) {
    auto hit = conflict.wifi(protectedAp.data(), protectedAp.size(), 300 + i);
    CHECK((hit.family == Family::EvilTwin) == (i == 2));
    if (i == 2)
      CHECK(hit.confidence == Confidence::Medium);
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
  CHECK(f.match.confidence == Confidence::Medium);
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
        deck.pick(Topic((t + 1) % TopicCount), (avatar + 1) % 5, !spicy, 0); // Stale bags.
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
  // Changing buddy or banter mid-bag renumbers the lines: the bag restarts cleanly, and the line
  // just spoken still cannot come straight back.
  for (unsigned seed = 0; seed < 200; ++seed) {
    Banter deck;
    unsigned mochi = Banter::count(Topic::Idle, 0, true), half = mochi / 2;
    std::string previous;
    for (unsigned i = 0; i < half; ++i)
      previous = deck.pick(Topic::Idle, 0, true, random());
    for (unsigned who : {2u, 0u}) // Switch to Orbit with gentle banter, then back.
      for (bool spicy : {false, true}) {
        unsigned n = Banter::count(Topic::Idle, who, spicy);
        std::set<std::string> bag;
        for (unsigned i = 0; i < n; ++i) {
          std::string said = deck.pick(Topic::Idle, who, spicy, random());
          CHECK(said != previous);
          previous = said;
          bag.insert(said);
        }
        CHECK(bag.size() == n);
      }
  }
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
    road.recording = road.fix = true;
    CHECK(chatterTopic(Feeling::Content, road, 1) == Topic::Cruise);
    road.sinceFindMs = DryRoadMs;
    CHECK(chatterTopic(Feeling::Content, road, 1) == Topic::DryRoad);
    road.moving = false;
    road.stoppedMs = RedLightMs;
    CHECK(chatterTopic(Feeling::Content, road, 1) == Topic::RedLight);
    // No fix means motion is unknown: never a red-light or cruising remark, whatever the
    // stale timers say (a tunnel, a garage, or a drive started before the first fix).
    road.fix = false;
    for (uint32_t r = 0; r < 6; ++r)
      for (uint32_t noFix : {0u, NoSkyMs - 1, NoSkyMs, 10 * NoSkyMs}) {
        road.noFixMs = noFix;
        Topic t = chatterTopic(Feeling::Content, road, r);
        CHECK(t == Topic::Idle || (t == Topic::NoSky && noFix >= NoSkyMs));
        CHECK(chatterTopic(Feeling::Hungry, road, r) ==
              (t == Topic::NoSky ? t : Topic::IdleHungry));
      }
    road.noFixMs = NoSkyMs;
    CHECK(chatterTopic(Feeling::Content, road, 1) == Topic::NoSky);
    for (uint32_t r = 0; r < 1000; r += 37) {
      CHECK(chatterGapMs(Feeling::Content, false, r) >= 30000);
      CHECK(chatterGapMs(Feeling::Napping, false, r) >= 120000);
    }
    CHECK(milestone(10) && milestone(100) && milestone(3000) && !milestone(0) && !milestone(11));
    {
      // A milestone that lands at a bad moment waits for the next free one.
      Milestone pets;
      for (uint32_t n = 1; n <= 99; ++n)
        pets.count(n);
      CHECK(pets.due() == 50);
      pets.clear();
      pets.count(100);
      pets.count(101); // The 100th pet was a giggle; the tally is still owed afterwards.
      CHECK(pets.due() == 100);
      CHECK(!petMilestoneFits(Topic::Wake) && !petMilestoneFits(Topic::PetEnough));
      CHECK(petMilestoneFits(Topic::Pet) && petMilestoneFits(Topic::PetGiggle) &&
            petMilestoneFits(Topic::PetMissedYou) && petMilestoneFits(Topic::PetAdore));
      pets.clear();
      CHECK(!pets.due());
      Milestone finds;
      finds.count(10, 25); // Find milestones start at 25.
      CHECK(!finds.due());
      finds.count(25, 25);
      finds.count(26, 25);
      CHECK(finds.due() == 25);
      // Jackpot: once per burst, even when find 12 itself was a snack or a level-up.
      CHECK(!jackpotDue(JackpotFinds - 1, false, JackpotQuietMs + 1));
      CHECK(jackpotDue(JackpotFinds, false, JackpotQuietMs + 1));
      CHECK(jackpotDue(JackpotFinds + 3, false, JackpotQuietMs + 1));
      CHECK(!jackpotDue(JackpotFinds + 3, true, JackpotQuietMs + 1));
      CHECK(!jackpotDue(JackpotFinds, false, JackpotQuietMs));
      // Best drive follows the count as it grows, so a drive that ends with the ignition still
      // counts; "record" compares with the best at the start of the drive.
      uint32_t best = 0;
      DriveBest first;
      first.start(best);
      for (uint32_t n = 1; n <= 40; ++n)
        DriveBest::found(best, n);
      CHECK(best == 40 && !first.record(40)); // A first drive is not a "record".
      DriveBest second;
      second.start(best);
      DriveBest::found(best, 12);
      CHECK(best == 40 && !second.record(12) && !second.record(40));
      DriveBest::found(best, 41);
      CHECK(best == 41 && second.record(41));
    }
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
    // Time apart is measured to boot: three hours of clock-less uptime are not time away.
    CHECK(awayHoursAtBoot(then, then + 10 * 3600, 3 * 3600 * 1000u) == 7);
    CHECK(awayHoursAtBoot(then, then + 10 * 3600, 0) == 10);
    CHECK(!awayHoursAtBoot(then, then + 1800, 3 * 3600 * 1000u)); // Booted before lastSeen.
    CHECK(!awayHoursAtBoot(then, 100, 200 * 1000u) && !awayHoursAtBoot(0, then, 0));
    // The board clock: only after GPS set it, with a healthy backup supply and a sane reading.
    CHECK(boardClockTrusted(true, false, then, 0) && boardClockTrusted(true, false, then, then));
    CHECK(!boardClockTrusted(false, false, then, 0) && !boardClockTrusted(true, true, then, 0));
    CHECK(!boardClockTrusted(true, false, 0, then));
    CHECK(boardClockTrusted(true, false, then + 59u * 86400u, then));
    CHECK(boardClockTrusted(true, false, then - ClockSlackSeconds, then));
    CHECK(!boardClockTrusted(true, false, then - ClockSlackSeconds - 1, then));    // Ran backwards.
    CHECK(!boardClockTrusted(true, false, then + ClockMaxAheadSeconds + 1, then)); // Year 2065.
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
    CHECK(homeui::bubbleChars(true) == int(BubbleChars));
    for (const auto &l : Lines)
      for (bool face : {false, true}) {
        homeui::BubbleBox b;
        CHECK(homeui::layoutBubble(l.text, face, b));
        std::string joined;
        for (int i = 0; i < b.lines; ++i) {
          CHECK(int(strlen(b.rows[i])) <= homeui::bubbleChars(face));
          joined += (i ? " " : "") + std::string(b.rows[i]);
        }
        CHECK(joined == l.text);
        CHECK(!face || b.lines == 1);
      }
    // Bubble placement, for every length the message buffer allows.
    for (unsigned n = 1; n <= 95; ++n)
      for (int gap : {5, 9, 96}) {
        std::string msg;
        for (unsigned i = 0; i < n; ++i)
          msg += i % gap == unsigned(gap) - 1 ? ' ' : 'm';
        for (bool custom : {false, true}) {
          BuddyLook look;
          look.custom = custom;
          look.headwear = 2;
          FaceFit fit = faceFit(look);
          homeui::BubbleBox b;
          CHECK(homeui::layoutBubble(msg.c_str(), true, b));
          // Full Face: top of the stage, tail down, wholly above the mouth (y 13 in buddy space).
          CHECK(b.tailDown && b.y >= 24 && b.x >= 10 && b.x + b.w <= 310);
          CHECK(b.tipY > b.y + b.h && b.tipY < fit.oy + int(13 * fit.k) - 6);
          CHECK(b.tailX - 7 >= b.x + 7 && b.tailX + 7 <= b.x + b.w - 7);
        }
        homeui::BubbleBox b;
        CHECK(homeui::layoutBubble(msg.c_str(), false, b));
        // Recon: under the buddy, inside the stage, and never above the chin, even at 3 rows.
        CHECK(!b.tailDown && b.tipY >= homeui::ReconChinY && b.y + b.h <= 175);
        CHECK(b.x >= 0 && b.x + b.w <= 182);
        CHECK(b.y + b.inset + (b.lines - 1) * b.pitch + 8 <= b.y + b.h);
      }
    homeui::BubbleBox none;
    CHECK(!homeui::layoutBubble("", false, none) && !homeui::layoutBubble("   ", true, none));
  }
  {
    // Family names: every family has a long name and a short one that fits a NEARBY row.
    std::set<std::string> longNames, shortNames;
    for (unsigned f = 0; f < FamilyCount; ++f) {
      const char *brief = familyShort(Family(f));
      CHECK(*familyName(Family(f)) && *brief && strlen(brief) <= FamilyShortChars);
      for (const char *c = brief; *c; ++c)
        CHECK(*c >= 32 && *c <= 126);
      longNames.insert(familyName(Family(f)));
      shortNames.insert(brief);
    }
    CHECK(longNames.size() == FamilyCount && shortNames.size() == FamilyCount);
    CHECK(unsigned(Family::Watchlist) == 20 && unsigned(Family::SamsungTag) == 21 &&
          unsigned(Family::GoogleTag) == 22 && unsigned(Family::Tile) == 23 &&
          unsigned(Family::Ring) == 24 && FamilyCount == 25);
    CHECK(!strcmp(familyName(Family::SamsungTag), "Samsung SmartTag") &&
          !strcmp(familyName(Family::GoogleTag), "Google / Chipolo tag") &&
          !strcmp(familyName(Family::Tile), "Tile tracker") &&
          !strcmp(familyName(Family::Ring), "Ring camera"));
    CHECK(!strcmp(familyShort(Family::Axon), "Body cam") &&
          !strcmp(familyShort(Family::Raven), "Gunshot sensor") &&
          !strcmp(familyShort(Family::SamsungTag), "Samsung tag") &&
          !strcmp(familyShort(Family::GoogleTag), "Google tag") &&
          !strcmp(familyShort(Family::Tile), "Tile") &&
          !strcmp(familyShort(Family::Ring), "Ring cam"));
    // The new families get their own quiet timers like any other.
    Encounters quiet;
    uint8_t tag[6] = {2, 0, 0, 0, 0, 9};
    auto &ring = quiet.observe(tag, true, 0, Family::Ring, 1000, 300000);
    CHECK(quiet.eligible(ring, Confidence::High, 1000, 60000, false));
    quiet.announce(ring, Confidence::High, 1000);
    tag[5] = 10;
    auto &ring2 = quiet.observe(tag, true, 0, Family::Ring, 20000, 300000);
    CHECK(!quiet.eligible(ring2, Confidence::High, 20000, 60000, false));
    auto &tile = quiet.observe(tag, true, 0, Family::Tile, 20000, 300000);
    CHECK(quiet.eligible(tile, Confidence::High, 20000, 60000, false));
  }
  {
    // Home screen rules.
    char label[32];
    for (unsigned level : {1u, 99u, 100u, 1000u, 10000u, 999999999u}) {
      xpLabel(label, sizeof(label), level, 24);
      CHECK(strlen(label) <= 12); // 74 px bar at 6 px per character.
    }
    xpLabel(label, sizeof(label), 4, 19);
    CHECK(!strcmp(label, "LV 4  19/25"));
    CHECK(onDanger(Theme::Light) == 0xffff && onDanger(Theme::Dark) == Ink &&
          onDanger(Theme::Matrix) == Ink && onDanger(Theme::Bwm) == Ink);
    // Full Face keeps the 2.1x close-up unless a custom buddy wears a hat; then the whole hat
    // (top at y -64) and the mouth (down to y 25) must be inside the stage (screen y 24..174).
    BuddyLook look;
    look.headwear = 2; // Ignored: presets wear nothing.
    CHECK(faceFit(look).k == 2.1f && faceFit(look).oy == 96);
    look.custom = true;
    look.headwear = 0;
    CHECK(faceFit(look).k == 2.1f);
    for (int hat = 1; hat <= 5; ++hat) {
      look.headwear = hat;
      FaceFit fit = faceFit(look);
      CHECK(fit.oy - 64 * fit.k >= 24 && fit.oy + 25 * fit.k <= 174);
    }
    // Taps. Buttons first.
    auto tap = [](int x, int y, bool face, bool alert, unsigned clues) {
      return homeTap(x, y, face, alert, clues).action;
    };
    CHECK(tap(80, 220, false, false, 0) == HomeTap::Drive);
    CHECK(tap(200, 220, false, true, 0) == HomeTap::SwitchView);
    CHECK(tap(280, 220, true, false, 0) == HomeTap::Menu &&
          tap(4, 220, true, false, 0) == HomeTap::None);
    // The NEARBY panel: populated rows open their clue; the header, the hint and empty space
    // open the list, or dismiss while an alert is showing.
    CHECK(tap(250, 40, false, false, 3) == HomeTap::ClueList);
    CHECK(tap(250, 30, false, false, 0) == HomeTap::ClueList);
    CHECK(tap(250, 100, false, false, 0) == HomeTap::ClueList);
    CHECK(tap(250, 150, false, false, 2) == HomeTap::ClueList);
    CHECK(tap(250, 174, false, false, 3) == HomeTap::ClueList);
    for (unsigned i = 0; i < 3; ++i)
      for (bool alert : {false, true}) {
        HomeTapTarget t = homeTap(250, 60 + int(i) * 40, false, alert, 9);
        CHECK(t.action == HomeTap::Clue && t.clue == i);
      }
    CHECK(tap(250, 40, false, true, 3) == HomeTap::Dismiss);
    CHECK(tap(250, 100, false, true, 0) == HomeTap::Dismiss);
    CHECK(tap(250, 150, false, true, 2) == HomeTap::Dismiss);
    CHECK(tap(250, 100, true, false, 3) == HomeTap::Pet); // No panel in Full Face.
    CHECK(tap(90, 100, false, false, 3) == HomeTap::Pet &&
          tap(90, 100, false, true, 3) == HomeTap::Dismiss);
    CHECK(tap(160, 190, false, false, 0) == HomeTap::Stats &&
          tap(160, 190, true, true, 0) == HomeTap::Dismiss);
    CHECK(tap(160, 10, false, false, 0) == HomeTap::None);
  }
  {
    // A napping buddy gets one settle cue, then the servos are left alone until it wakes.
    NapSettle nap;
    CHECK(!nap.update(false) && !nap.resting());
    CHECK(nap.update(true) && nap.resting());
    for (int i = 0; i < 1000; ++i)
      CHECK(!nap.update(true));
    CHECK(!nap.update(false) && !nap.resting());
    CHECK(nap.update(true));
  }
  {
    // Buddy art. A recording canvas stands in for the display.
    struct Shape {
      char kind;
      int x, y, w, h, r;
      uint16_t c;
    };
    struct Recorder {
      std::vector<Shape> shapes;
      void fillEllipse(int x, int y, int rx, int ry, uint16_t c) {
        shapes.push_back({'e', x, y, rx, ry, 0, c});
      }
      void fillCircle(int x, int y, int r, uint16_t c) {
        shapes.push_back({'c', x, y, r, r, r, c});
      }
      void fillRect(int x, int y, int w, int h, uint16_t c) {
        shapes.push_back({'r', x, y, w, h, 0, c});
      }
      void fillRoundRect(int x, int y, int w, int h, int r, uint16_t c) {
        shapes.push_back({'r', x, y, w, h, r, c});
      }
      void fillTriangle(int, int, int, int, int, int, uint16_t) {}
      void drawLine(int, int, int, int, uint16_t) {}
    };
    // Zoom: the display driver does not clamp corner radii, so no box may reach it with a radius
    // over half its size, and thin details must keep at least one pixel at any scale.
    for (float k : {0.62f, 1.0f, 1.6f, 2.1f}) {
      Recorder canvas;
      art::Zoom<Recorder> zoom{canvas, 100, 100, k};
      for (int avatar = 0; avatar < 5; ++avatar)
        for (int mood = 0; mood < 7; ++mood)
          for (bool custom : {false, true}) {
            Look look;
            look.base = avatar;
            look.custom = custom;
            look.eyes = look.headwear = mood % 6;
            look.eyewear = look.neckwear = mood % 4;
            Pose pose;
            pose.mood = pose.phase = mood;
            pose.kit = mood % 3;
            drawBuddy(zoom, look, 0, 0, pose);
          }
      CHECK(canvas.shapes.size() > 1000);
      for (const auto &s : canvas.shapes)
        if (s.kind == 'r')
          CHECK(s.w >= 1 && s.h >= 1 && s.r >= 0 && 2 * s.r <= s.w && 2 * s.r <= s.h);
    }
    {
      Recorder canvas;
      art::Zoom<Recorder> zoom{canvas, 0, 0, 0.62f};
      zoom.fillRoundRect(-2, 17, 5, 3, 1, Mint); // Orbit's LED mouth dash in the buddy picker.
      zoom.fillRect(-8, 1, 16, 1, Ink);          // A scanline that used to round to nothing.
      CHECK(canvas.shapes[0].h == 1 && canvas.shapes[0].r == 0 && canvas.shapes[1].h == 1);
    }
    // Faces stay readable: a dark eye colour on a dark surface is swapped for a light one.
    auto faceColors = [](int avatar, uint16_t body, uint16_t eyeColor, bool custom, int mood) {
      Recorder canvas;
      art::Pen<Recorder> pen{canvas, 0, 0, 0, Ink};
      Tones t = buddyTones(avatar, custom, body, Blush);
      buddyDetails(pen, avatar, mood, 0, false, custom, 0, eyeColor, t.body, t.blush, t.line,
                   t.shade, t.light, t.trim, 0);
      std::set<uint16_t> eyes;
      for (const auto &s : canvas.shapes) // The classic eye is an 8 x 12 oval at x +-21, y -4.
        if (s.kind == 'e' && (s.x == 21 || s.x == -21) && s.y == -4 && s.w == 8)
          eyes.insert(s.c);
      return eyes;
    };
    using Colors = std::set<uint16_t>;
    CHECK(faceColors(2, Lilac, Ink, true, 0) == Colors{Mint});       // Orbit's dark screen.
    CHECK(faceColors(0, 0x0000, Ink, true, 0) == Colors{0xefff});    // A black custom body.
    CHECK(faceColors(0, 0x0000, 0x001f, true, 0) == Colors{0xefff}); // Deep blue on black.
    CHECK(faceColors(0, Paper, Ink, true, 0) == Colors{Ink});        // Normal: untouched.
    CHECK(faceColors(0, Paper, 0x8200, true, 0) == Colors{0x8200});  // Any mid colour is kept.
    CHECK(faceColors(0, 0x0000, 0xfd20, true, 0) == Colors{0xfd20}); // Bright on dark is kept.
    CHECK(faceColors(2, Lilac, 0xfd20, true, 0) == Colors{0xfd20});
    CHECK(faceColors(0, Paper, 0xffff, true, 0) == Colors{Ink}); // White on cream.
    CHECK(faceColors(0, Paper, Ink, false, 0) == Colors{Ink});   // Presets are unchanged.
    CHECK(art::dark(Ink) && art::dark(0x10c7) && !art::dark(Mint) && art::pale(Paper) &&
          !art::pale(Mint) && !art::pale(Ink));
  }
  signatureTests();
  for (int i = 0; i < 50000; ++i) {
    size_t n = random() % 768;
    std::vector<uint8_t> q(n);
    for (auto &v : q)
      v = random();
    classifyBle(q.data(), q.size(), mac, true);
    classifyWifi(q.data(), q.size());
  }
  reconTests();
  std::cout << "Core regression checks and 50,000 malformed-packet cases passed\n";
}
