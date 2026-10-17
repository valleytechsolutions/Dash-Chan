#include "detector.h"
#include "encounters.h"
#include "idle_motion.h"
#include "nutrition.h"
#include "petting.h"
#include "profile.h"
#include "traffic.h"
#include "travel.h"
#include "watchlist.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
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
int main() {
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
  for (unsigned avatar = 0; avatar < 3; ++avatar) {
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
