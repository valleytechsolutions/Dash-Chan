#include "detector.h"
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
  std::mt19937 random(7);
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
