// Classification, grading and rate-rule checks. Fixtures are synthetic: they pin the rules, not
// real-world recall. Called from tests/core_test.cpp.
#pragma once
#include "detector.h"
#include "traffic.h"
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace sigtest {
using namespace dash;
using Bytes = std::vector<uint8_t>;
#define SIG_CHECK(expr)                                                                            \
  do {                                                                                             \
    if (!(expr)) {                                                                                 \
      std::cerr << "signature_tests.h:" << __LINE__ << " failed: " << #expr << '\n';               \
      std::exit(1);                                                                                \
    }                                                                                              \
  } while (0)

// An address in no vendor table, public and globally administered.
static const uint8_t Plain[6] = {0x10, 0x20, 0x30, 1, 2, 3};

inline void ad(Bytes &p, uint8_t type, std::initializer_list<uint8_t> d) {
  p.push_back(uint8_t(d.size() + 1));
  p.push_back(type);
  p.insert(p.end(), d);
}
inline void adName(Bytes &p, const char *name) {
  p.push_back(uint8_t(strlen(name) + 1));
  p.push_back(9);
  p.insert(p.end(), name, name + strlen(name));
}
inline BleFrame ble(const Bytes &p, const uint8_t *mac = Plain, bool publicAddress = true) {
  return classifyBle(p.data(), p.size(), mac, publicAddress);
}
inline BleFrame bleNamed(const char *name) {
  Bytes p;
  adName(p, name);
  return ble(p);
}
inline BleFrame bleService(uint16_t uuid, bool serviceData = false) {
  Bytes p;
  if (serviceData)
    ad(p, 0x16, {uint8_t(uuid), uint8_t(uuid >> 8), 1, 2});
  else
    ad(p, 3, {uint8_t(uuid), uint8_t(uuid >> 8)});
  return ble(p);
}
inline BleFrame bleCompany(uint16_t id) {
  Bytes p;
  ad(p, 0xff, {uint8_t(id), uint8_t(id >> 8), 0});
  return ble(p);
}
inline bool is(const Match &m, Family f, Confidence c) {
  return m.family == f && m.confidence == c;
}
inline bool labelled(const char *label, const char *expected) {
  return !strcmp(label, expected);
}
// A beacon whose transmitter and BSSID are both `mac`.
inline Bytes beacon(const uint8_t *mac, const char *ssid, bool privacy = false) {
  Bytes p(36, 0);
  p[0] = 0x80;
  memset(p.data() + 4, 0xff, 6);
  memcpy(p.data() + 10, mac, 6);
  memcpy(p.data() + 16, mac, 6);
  if (privacy)
    p[34] = 0x10;
  p.push_back(0);
  p.push_back(uint8_t(strlen(ssid)));
  p.insert(p.end(), ssid, ssid + strlen(ssid));
  p.insert(p.end(), {3, 1, 6});
  return p;
}
inline WifiFrame ssid(const char *name) {
  auto p = beacon(Plain, name);
  return classifyWifi(p.data(), p.size());
}
// A wildcard probe request: the classifier sees only the transmitter address.
inline WifiFrame probeFrom(uint8_t a, uint8_t b, uint8_t c) {
  Bytes p(26, 0);
  p[0] = 0x40;
  p[10] = a;
  p[11] = b;
  p[12] = c;
  p[15] = 7;
  return classifyWifi(p.data(), p.size());
}
inline Bytes disconnect(const uint8_t *from, bool broadcast, uint16_t sequence) {
  Bytes p(26, 0);
  p[0] = 0xc0;
  if (broadcast)
    memset(p.data() + 4, 0xff, 6);
  else
    p[4] = 0x44;
  memcpy(p.data() + 10, from, 6);
  p[22] = uint8_t(sequence);
  p[23] = uint8_t(sequence >> 8);
  p[24] = 7;
  return p;
}

inline void trackers() {
  // Tile, Inc. holds all three UUIDs; they arrive in the list or as service data.
  for (uint16_t uuid : {0xfeed, 0xfeec, 0xfd84})
    for (bool data : {false, true}) {
      auto f = bleService(uuid, data);
      SIG_CHECK(f.valid && is(f.match, Family::Tile, Confidence::High) &&
                labelled(f.label, "Tile"));
    }
  auto f = bleService(0xfd5a, true);
  SIG_CHECK(is(f.match, Family::SamsungTag, Confidence::High) && labelled(f.label, "SmartTag"));
  // Find Hub: frame type 0x40 / 0x41 and an ephemeral ID of a valid length (20 or 32 bytes,
  // optionally one hashed-flags byte).
  for (size_t body : {21, 22, 33, 34})
    for (uint8_t frame : {0x40, 0x41}) {
      Bytes p = {uint8_t(body + 3), 0x16, 0xaa, 0xfe, frame};
      p.resize(p.size() + body - 1, 0x5a);
      f = ble(p);
      SIG_CHECK(f.valid && is(f.match, Family::GoogleTag, Confidence::High) &&
                labelled(f.label, "Find Hub"));
    }
  Bytes p;
  ad(p, 0x16, {0xaa, 0xfe, 0x40, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10}); // Wrong length.
  SIG_CHECK(ble(p).valid && ble(p).match.family == Family::None);
  p.clear();
  ad(p, 0x16, {0xaa, 0xfe, 0x10, 0xeb, 1, 'x'}); // Eddystone-URL: a shop beacon, not a tracker.
  SIG_CHECK(ble(p).match.family == Family::None);
  // Removed false positive: FEAA in the service list is every Eddystone beacon.
  SIG_CHECK(bleService(0xfeaa).match.family == Family::None && !*bleService(0xfeaa).label);
  f = bleService(0xfcb2, true);
  SIG_CHECK(is(f.match, Family::GoogleTag, Confidence::Medium) &&
            labelled(f.label, "DULT tracker"));
  for (uint16_t uuid : {0xfe33, 0xfe65}) {
    f = bleService(uuid);
    SIG_CHECK(is(f.match, Family::GoogleTag, Confidence::Medium) && labelled(f.label, "Chipolo"));
  }
  f = bleCompany(0x08c3);
  SIG_CHECK(is(f.match, Family::GoogleTag, Confidence::Medium) && labelled(f.label, "Chipolo"));
  // Apple Find My stays the plain tracker family.
  p.clear();
  p = {30, 0xff, 0x4c, 0, 0x12, 0x19};
  p.resize(31, 0);
  f = ble(p);
  SIG_CHECK(is(f.match, Family::Tracker, Confidence::Medium) && labelled(f.label, "Find My"));
  SIG_CHECK(bleNamed("AirTag").match.family == Family::Tracker);
  SIG_CHECK(is(bleNamed("Galaxy SmartTag2").match, Family::SamsungTag, Confidence::Low));
}

inline void beacons() {
  // The 25-byte Apple block: 4C 00 02 15, UUID, big-endian major and minor, power.
  Bytes p = {26, 0xff, 0x4c, 0,  2,  0x15, 0xa1, 0xb2, 0xc3, 4,    5,    6,    7,   8,
             9,  10,   11,   12, 13, 14,   15,   16,   0x01, 0x02, 0x03, 0x04, 0xc5};
  auto f = ble(p);
  SIG_CHECK(f.valid && is(f.match, Family::Beacon, Confidence::High));
  SIG_CHECK(labelled(f.label, "A1B2C3 258.772"));
  p[22] = p[23] = p[24] = p[25] = 0xff; // The longest label still fits.
  f = ble(p);
  SIG_CHECK(labelled(f.label, "A1B2C3 65535.65535") && strlen(f.label) < LabelChars);
  // One byte too long is not the fixed format; one byte short is not a beacon at all.
  p[0] = 27;
  p.push_back(0);
  f = ble(p);
  SIG_CHECK(is(f.match, Family::Beacon, Confidence::Low));
  p.pop_back();
  p.pop_back();
  p[0] = 25;
  SIG_CHECK(ble(p).valid && ble(p).match.family == Family::None && !*ble(p).label);
}

inline void bodyCamerasAndGlasses() {
  // Axon / TASER's registered identifiers stand on their own.
  auto f = bleCompany(0x034d);
  SIG_CHECK(is(f.match, Family::Axon, Confidence::High) && labelled(f.label, "Axon / TASER"));
  for (uint16_t uuid : {0xfc81, 0xfe6b, 0xfe6c})
    SIG_CHECK(is(bleService(uuid).match, Family::Axon, Confidence::High));
  // BWCDEVICE inside manufacturer data, and reversed inside a 128-bit service-data UUID.
  Bytes p;
  ad(p, 0xff, {0x99, 0x09, 'B', 'W', 'C', 'D', 'E', 'V', 'I', 'C', 'E'});
  f = ble(p);
  SIG_CHECK(is(f.match, Family::Axon, Confidence::High) && labelled(f.label, "Axon body cam"));
  p.clear();
  ad(p, 0x21, {1, 2, 3, 'E', 'C', 'I', 'V', 'E', 'D', 'C', 'W', 'B', 4, 5, 6, 7, 0});
  SIG_CHECK(is(ble(p).match, Family::Axon, Confidence::High));
  // The marker typed into a device name is just a name.
  SIG_CHECK(bleNamed("BWCDEVICE").match.family == Family::None);
  // Public addresses in a registered block; a random address with the same bytes is only a hint.
  const uint8_t axon[6] = {0, 0x25, 0xdf, 1, 2, 3}, watchguard[6] = {0, 0x1d, 0x96, 1, 2, 3};
  p = {2, 1, 6};
  f = ble(p, axon);
  SIG_CHECK(is(f.match, Family::Axon, Confidence::High) && *f.label);
  SIG_CHECK(ble(p, axon, false).match.confidence == Confidence::Low);
  f = ble(p, watchguard);
  SIG_CHECK(is(f.match, Family::Axon, Confidence::Medium) && labelled(f.label, "WatchGuard"));

  // Glasses. FD5F is specific to Ray-Ban Meta; a bare Meta identifier is also a headset.
  f = bleService(0xfd5f);
  SIG_CHECK(is(f.match, Family::Meta, Confidence::Medium) && labelled(f.label, "Ray-Ban Meta"));
  p.clear();
  ad(p, 3, {0x5f, 0xfd});
  ad(p, 0xff, {0xab, 0x01, 0});
  f = ble(p);
  SIG_CHECK(is(f.match, Family::Meta, Confidence::Medium) && labelled(f.label, "Ray-Ban Meta"));
  f = bleCompany(0x01ab);
  SIG_CHECK(is(f.match, Family::Meta, Confidence::Low) && labelled(f.label, "Meta"));
  SIG_CHECK(is(bleService(0xfeb7).match, Family::Meta, Confidence::Low));
  SIG_CHECK(is(bleService(0xfeb8).match, Family::Meta, Confidence::Low));
  f = bleCompany(0x0d53);
  SIG_CHECK(is(f.match, Family::Meta, Confidence::Medium) && labelled(f.label, "Luxottica"));
  f = bleCompany(0x03c2);
  SIG_CHECK(is(f.match, Family::Meta, Confidence::Medium) && labelled(f.label, "Snap Spectacles"));
  SIG_CHECK(is(bleService(0xfe45).match, Family::Meta, Confidence::Medium));
  f = bleCompany(0x060c);
  SIG_CHECK(is(f.match, Family::Meta, Confidence::Medium) && labelled(f.label, "Vuzix"));
  SIG_CHECK(is(bleNamed("Ray-Ban Meta 01").match, Family::Meta, Confidence::Low));
}

inline void gadgets() {
  // A Flipper Zero advertises exactly one of three services. 0x3080 is not one of them.
  for (uint16_t uuid : {0x3081, 0x3082, 0x3083})
    SIG_CHECK(is(bleService(uuid).match, Family::Flipper, Confidence::High));
  SIG_CHECK(bleService(0x3080).match.family == Family::None);
  SIG_CHECK(bleService(0x3084).match.family == Family::None);
  SIG_CHECK(is(bleNamed("Flipper Abc").match, Family::Flipper, Confidence::Medium));
  SIG_CHECK(is(bleNamed("Superflipper").match, Family::Flipper, Confidence::Low));
  // Raven: the five round services, graded up when all are present. Nothing in between.
  SIG_CHECK(is(bleService(0x3100).match, Family::Raven, Confidence::Medium));
  SIG_CHECK(bleService(0x3101).match.family == Family::None);
  Bytes p;
  ad(p, 3, {0, 0x31, 0, 0x32, 0, 0x33, 0, 0x34, 0, 0x35});
  SIG_CHECK(is(ble(p).match, Family::Raven, Confidence::High));
  // Serial modules: the exact default names known skimmers keep, then weaker forms.
  for (const char *name : {"HC-03", "HC-05", "HC-06", "RN42", "BT04-A", "hc-05"})
    SIG_CHECK(is(bleNamed(name).match, Family::Skimmer, Confidence::High));
  SIG_CHECK(is(bleNamed("HC-05-pump7").match, Family::Skimmer, Confidence::Low));
  SIG_CHECK(is(bleNamed("HC-08").match, Family::Skimmer, Confidence::Low));
  SIG_CHECK(is(bleService(0x1101).match, Family::Skimmer, Confidence::Medium));
  // Xuntong is a module maker: weak alone, stronger with a battery name.
  SIG_CHECK(is(bleCompany(0x09c8).match, Family::Flock, Confidence::Low));
  p.clear();
  ad(p, 0xff, {0xc8, 0x09, 0});
  adName(p, "Penguin-1234567890");
  auto f = ble(p);
  SIG_CHECK(is(f.match, Family::Flock, Confidence::High) && labelled(f.label, "Flock battery"));
  p.clear();
  ad(p, 0xff, {0xc8, 0x09, 0});
  adName(p, "1234567890");
  SIG_CHECK(is(ble(p).match, Family::Flock, Confidence::Medium));
  // The Flock accessory's 128-bit service.
  p.clear();
  ad(p, 7,
     {0x6f, 0x2e, 0x17, 0xdf, 0x14, 0x18, 0xe5, 0x9f, 0xa8, 0x46, 0x32, 0x95, 0x38, 0xbb, 0xcc,
      0xe8});
  SIG_CHECK(is(ble(p).match, Family::Flock, Confidence::High));
  SIG_CHECK(is(bleNamed("Nyanbox").match, Family::Nyanbox, Confidence::Low));
  SIG_CHECK(is(bleNamed("HaleHound").match, Family::Halehound, Confidence::Low));
}

inline void names() {
  // Documented Flock forms are strong; a bare serial or a word inside a sentence is a guess.
  for (const char *name : {"Flock", "Flock_Setup", "flock-1A2B", "FS Ext Battery", "FS-BEC46A",
                           "Penguin-1234567890", "Pigvision"})
    SIG_CHECK(is(bleNamed(name).match, Family::Flock, Confidence::High));
  for (const char *name : {"1234567890", "Happy Penguin", "My Flock Phone", "Flockhart"})
    SIG_CHECK(is(bleNamed(name).match, Family::Flock, Confidence::Low));
  SIG_CHECK(bleNamed("FS-Living Room").match.family == Family::None);
  // The stronger of name and payload wins: a documented name beats a Medium service clue, and a
  // guessed name does not hide one.
  Bytes mixed;
  ad(mixed, 3, {0x33, 0xfe});
  adName(mixed, "Flock-1A2B");
  SIG_CHECK(is(ble(mixed).match, Family::Flock, Confidence::High) && !*ble(mixed).label);
  mixed.clear();
  ad(mixed, 3, {0x33, 0xfe});
  adName(mixed, "My Flock Phone");
  SIG_CHECK(is(ble(mixed).match, Family::GoogleTag, Confidence::Medium));
  SIG_CHECK(bleNamed("Flock Noir").match.family == Family::None); // A detector, not a camera.
  // Axon needs a word boundary. Saxon, Klaxon and taxonomy are not body cameras.
  auto f = bleNamed("Axon Body 3");
  SIG_CHECK(is(f.match, Family::Axon, Confidence::High) && labelled(f.label, "Axon"));
  f = bleNamed("BodyWorn-0042");
  SIG_CHECK(is(f.match, Family::Axon, Confidence::High) && labelled(f.label, "BodyWorn"));
  SIG_CHECK(is(bleNamed("My Axon").match, Family::Axon, Confidence::Low));
  for (const char *name : {"Saxon", "Klaxon", "Taxonomy", "Axonometric", "SAXON-5G"}) {
    SIG_CHECK(bleNamed(name).match.family == Family::None);
    SIG_CHECK(ssid(name).match.family == Family::None);
  }

  // SSIDs. The Axon pairing prefixes carry the model.
  auto w = ssid("AB3-0123456");
  SIG_CHECK(w.valid && is(w.match, Family::Axon, Confidence::High) &&
            labelled(w.label, "Axon Body 3"));
  SIG_CHECK(labelled(ssid("AB2-77").label, "Axon Body 2"));
  SIG_CHECK(labelled(ssid("ab4-77").label, "Axon Body 4"));
  SIG_CHECK(is(ssid("AXON-FIELD").match, Family::Axon, Confidence::High));
  SIG_CHECK(
      is(ssid("AB3 lounge").match, Family::Axon, Confidence::Low)); // Not the documented form.
  for (const char *name : {"Flock", "flock-88AA", "FLOCK-1", "Flock Camera net."})
    SIG_CHECK(is(ssid(name).match, Family::Flock, Confidence::High));
  SIG_CHECK(ssid("Flockhart Family WiFi").match.family == Family::None);
  SIG_CHECK(is(ssid("Flock guests").match, Family::Flock, Confidence::Low));
  w = ssid("Pineapple_1A2B");
  SIG_CHECK(is(w.match, Family::Pineapple, Confidence::Medium) &&
            labelled(w.label, "Pineapple AP"));
  w = ssid("pwned");
  SIG_CHECK(is(w.match, Family::Pineapple, Confidence::Medium) && labelled(w.label, "Deauther AP"));
  SIG_CHECK(is(ssid("Pineapple Express").match, Family::Pineapple, Confidence::Low));
  // A name clue alone still lets an ordinary AP into WiGLE.
  SIG_CHECK(ssid("pwned").loggable);
}

inline void addresses() {
  struct Row {
    uint8_t oui[3];
    Family family;
    Confidence confidence;
    const char *label;
  };
  // One block per vendor group; grades follow the registry audit in docs/DETECTIONS.md.
  const Row rows[] = {
      {{0x00, 0x25, 0xdf}, Family::Axon, Confidence::High, nullptr},
      {{0x00, 0x23, 0xbd}, Family::Axon, Confidence::Medium, "DigitalAlly"},
      {{0xb4, 0x1e, 0x52}, Family::Flock, Confidence::High, "Flock Safety"},
      {{0x70, 0xc9, 0x4e}, Family::Flock, Confidence::Low, ""}, // Shared module maker.
      {{0x00, 0x04, 0x7d}, Family::Alpr, Confidence::High, "Motorola Solutions"},
      {{0xb8, 0xe2, 0x8c}, Family::Alpr, Confidence::High, "Motorola Solutions"},
      {{0x10, 0x74, 0x6f}, Family::Alpr, Confidence::Low, "Motorola Solutions"}, // Not audited.
      {{0x00, 0xbf, 0x15}, Family::Alpr, Confidence::High, "Genetec Inc."},
      {{0xc0, 0x56, 0xe3}, Family::Camera, Confidence::High, "Hikvision"},
      {{0x00, 0xbc, 0x99}, Family::Camera, Confidence::Medium, "Hikvision"}, // Not audited.
      {{0xbc, 0x29, 0x78}, Family::Camera, Confidence::Medium, "Prama Hikvision India"},
      {{0x2c, 0xaa, 0x8e}, Family::Camera, Confidence::High, "Wyze Labs Inc"},
      {{0xfc, 0x9c, 0x98}, Family::Camera, Confidence::High, "Arlo Technology"},
      {{0xe0, 0xa7, 0x00}, Family::Camera, Confidence::High, "Verkada Inc"},
      {{0x00, 0x40, 0x8c}, Family::Camera, Confidence::High, "Axis Communications AB"},
      {{0xd4, 0x2d, 0xc5}, Family::Camera, Confidence::High, "i-PRO"},
      {{0x3c, 0xa0, 0x70}, Family::Camera, Confidence::Medium, "Blink by Amazon"},
      {{0x34, 0xd2, 0x70}, Family::Camera, Confidence::Low, "Amazon Technologies"},
      {{0x00, 0xe0, 0x4c}, Family::Camera, Confidence::Low, "Realtek"},
      {{0x18, 0x7f, 0x88}, Family::Ring, Confidence::High, "Ring LLC"},
      {{0x50, 0xe4, 0x67}, Family::Ring, Confidence::High, "Ring LLC"},
      {{0xfc, 0x65, 0xde}, Family::Ring, Confidence::Low, "Amazon Technologies"},
      {{0x60, 0x60, 0x1f}, Family::Drone, Confidence::Medium, "DJI"},
      {{0x9c, 0x5a, 0x8a}, Family::Drone, Confidence::Low, "DJI-BW"},
      {{0x38, 0x1d, 0x14}, Family::Drone, Confidence::High, "Skydio"},
      {{0x90, 0x03, 0xb7}, Family::Drone, Confidence::Medium, "Parrot"},
      {{0x00, 0x12, 0x1c}, Family::Drone, Confidence::Low, "Parrot"},
      {{0xd4, 0x11, 0xd6}, Family::Raven, Confidence::High, "ShotSpotter"},
      {{0x98, 0x59, 0x49}, Family::Meta, Confidence::Medium, "Luxottica Group S.P.A."},
      {{0x38, 0x47, 0x12}, Family::Meta, Confidence::Low, "Luxottica Tristar"},
      {{0x98, 0xd3, 0x31}, Family::Skimmer, Confidence::Low, "Skim-HC05"},
  };
  for (const auto &row : rows) {
    auto w = probeFrom(row.oui[0], row.oui[1], row.oui[2]);
    SIG_CHECK(w.valid && !w.loggable && is(w.match, row.family, row.confidence));
    if (row.label)
      SIG_CHECK(labelled(w.label, row.label));
    SIG_CHECK(strlen(w.label) < LabelChars);
  }
  // Removed false positives: an Espressif block is every ESP32, and Hak5 holds no IEEE block.
  SIG_CHECK(probeFrom(0xbc, 0xdd, 0xc2).match.family == Family::None);
  SIG_CHECK(probeFrom(0x00, 0x13, 0x37).match.family == Family::None);
  const uint8_t leet[6] = {0x00, 0x13, 0x37, 1, 2, 3};
  auto p = beacon(leet, "Open WiFi");
  SIG_CHECK(classifyWifi(p.data(), p.size()).match.family == Family::None);
  // Locally administered patterns identify no vendor and never rise above Low.
  SIG_CHECK(is(probeFrom(0x02, 0xc0, 0xca).match, Family::Pineapple, Confidence::Low));
  SIG_CHECK(is(probeFrom(0x02, 0x13, 0x37).match, Family::Pineapple, Confidence::Low));
  SIG_CHECK(is(probeFrom(0x82, 0x6b, 0xf2).match, Family::Flock, Confidence::Low));
  // A frame addressed to a vendor block is a clue about the receiver, labelled the same way.
  Bytes header(24, 0);
  header[0] = 8;
  header[4] = 0x18;
  header[5] = 0x7f;
  header[6] = 0x88;
  auto w = classifyWifi(header.data(), header.size());
  SIG_CHECK(is(w.match, Family::Ring, Confidence::High) && labelled(w.label, "Ring LLC") &&
            w.mac[0] == 0x18 && !w.loggable);

  // The lookup is a binary search, and a shared chip maker must never outrank Low.
  const size_t count = sizeof(dragon::vendorPrefixes) / sizeof(*dragon::vendorPrefixes);
  for (size_t i = 0; i < count; ++i) {
    const auto &row = dragon::vendorPrefixes[i];
    SIG_CHECK(!i || dragon::vendorPrefixes[i - 1].prefix < row.prefix);
    SIG_CHECK(!(row.prefix & 0x030000)); // Never multicast or locally administered.
    if (strstr(row.evidence, "hared"))
      SIG_CHECK(row.confidence == dragon::Confidence::Low);
    char label[LabelChars];
    vendorLabel(label, row.evidence);
    SIG_CHECK(strlen(label) < LabelChars);
    SIG_CHECK(starts(row.evidence, "Shared") == !*label);
  }
}

inline void wifiGadgetsAndDrones() {
  // A Pwnagotchi names itself inside its beacon. It is not an access point for WiGLE.
  auto p = beacon(Plain, "x");
  const std::string peer = "{\"name\":\"pwny-express\",\"pwnd_tot\":7,\"version\":\"1.5.5\"}";
  p.push_back(222);
  p.push_back(uint8_t(peer.size()));
  p.insert(p.end(), peer.begin(), peer.end());
  auto w = classifyWifi(p.data(), p.size());
  SIG_CHECK(w.valid && is(w.match, Family::Pwnagotchi, Confidence::High));
  SIG_CHECK(labelled(w.label, "pwny-express") && !w.loggable && labelled(w.name, "x"));
  // Its throwaway SSID is not a network anyone could impersonate.
  TrafficHints hints;
  const uint8_t other[6] = {0x44, 0x55, 0x66, 1, 2, 3};
  auto open = beacon(other, "x");
  auto guarded = p;
  guarded[34] = 0x10;
  for (uint32_t i = 0; i < 4; ++i) {
    SIG_CHECK(hints.wifi(open.data(), open.size(), 100 + i).family == Family::None);
    SIG_CHECK(hints.wifi(guarded.data(), guarded.size(), 200 + i).family == Family::None);
  }
  // Without the handshake counter it is only a beacon with JSON in it.
  auto q = beacon(Plain, "x");
  const std::string json = "{\"name\":\"pwny-express\"}";
  q.push_back(222);
  q.push_back(uint8_t(json.size()));
  q.insert(q.end(), json.begin(), json.end());
  w = classifyWifi(q.data(), q.size());
  SIG_CHECK(w.valid && w.match.family == Family::None && w.loggable && !*w.label);
  // A unit name longer than the label is cut, never overrun.
  auto r = beacon(Plain, "x");
  const std::string longPeer = "{\"pwnd_tot\":1,\"name\":\"abcdefghijklmnopqrstuvwxyz0123\"}";
  r.push_back(222);
  r.push_back(uint8_t(longPeer.size()));
  r.insert(r.end(), longPeer.begin(), longPeer.end());
  w = classifyWifi(r.data(), r.size());
  SIG_CHECK(labelled(w.label, "abcdefghijklmnopqrstuvw") && strlen(w.label) == LabelChars - 1);

  // Remote ID over BLE: service data FFFA, application code 0x0D, a counter, one 25-byte
  // message. This one is a Basic ID carrying a serial number.
  Bytes b = {30, 0x16, 0xfa, 0xff, 0x0d, 1, 0x02, 0x12};
  const char serial[] = "1581F4XDASH000000042";
  b.insert(b.end(), serial, serial + 20);
  b.insert(b.end(), {0, 0, 0});
  auto f = ble(b);
  SIG_CHECK(f.valid && is(f.match, Family::Drone, Confidence::High));
  SIG_CHECK(labelled(f.label, serial));
  // The same message in a Wi-Fi beacon's vendor element (FA:0B:BC, type 0x0D, counter, pack).
  auto d = beacon(Plain, "RID");
  d.insert(d.end(), {221, 33, 0xfa, 0x0b, 0xbc, 0x0d, 1, 0xf2, 25, 1});
  d.insert(d.end(), b.begin() + 6, b.end());
  w = classifyWifi(d.data(), d.size());
  SIG_CHECK(w.valid && is(w.match, Family::Drone, Confidence::High) && labelled(w.label, serial));
  // The service with nothing decodable behind it is only a weak hint, with no label.
  f = bleService(0xfffa);
  SIG_CHECK(is(f.match, Family::Drone, Confidence::Low) && !*f.label);
}

inline void mapping() {
  using D = dragon::Family;
  const struct {
    D from;
    Family to;
  } pairs[] = {
      {D::Wifi, Family::None},
      {D::Ble, Family::None},
      {D::Fox, Family::None},
      {D::Flock, Family::Flock},
      {D::Axon, Family::Axon},
      {D::Meta, Family::Meta},
      {D::Pineapple, Family::Pineapple},
      {D::Pwnagotchi, Family::Pwnagotchi},
      {D::Nyanbox, Family::Nyanbox},
      {D::Halehound, Family::Halehound},
      {D::Tracker, Family::Tracker},
      {D::Skimmer, Family::Skimmer},
      {D::Flipper, Family::Flipper},
      {D::Biscuit, Family::Biscuit},
      {D::SmartTag, Family::SamsungTag},
      {D::Raven, Family::Raven},
      {D::GoogleTag, Family::GoogleTag},
      {D::Tile, Family::Tile},
      {D::Ring, Family::Ring},
      {D::Camera, Family::Camera},
      {D::Alpr, Family::Alpr},
      {D::Drone, Family::Drone},
      {D::IBeacon, Family::Beacon},
      {D::Deauth, Family::Deauth},
      {D::EvilTwin, Family::EvilTwin},
      {D::BleSpam, Family::BleSpam},
  };
  SIG_CHECK(sizeof(pairs) / sizeof(*pairs) == size_t(D::Count)); // Every Dragon family is listed.
  for (const auto &pair : pairs) {
    auto m = translate({pair.from, dragon::Confidence::Strong, "test"});
    SIG_CHECK(m.family == pair.to);
    SIG_CHECK(m.confidence == (pair.to == Family::None ? Confidence::None : Confidence::High));
  }
  SIG_CHECK(translate({D::Tile, dragon::Confidence::Low, "test"}).confidence == Confidence::Low);
  SIG_CHECK(translate({D::Tile, dragon::Confidence::Medium, "t"}).confidence == Confidence::Medium);
}

inline void axonPrecedence() {
  using namespace dragon;
  // An Axon clue replaces the parsed match only when it is the stronger one.
  AxonEvidence name, oui, vendor;
  name.signals = AxonName;
  oui.signals = AxonOui;
  vendor.signals = BodyCameraVendor;
  const dragon::Match drone{dragon::Family::Drone, dragon::Confidence::Strong, "drone"};
  const dragon::Match weak{dragon::Family::Flock, dragon::Confidence::Low, "shared"};
  const dragon::Match none{dragon::Family::Ble, dragon::Confidence::None, "none"};
  SIG_CHECK(withAxon(drone, name).family == dragon::Family::Drone);
  SIG_CHECK(withAxon(drone, oui).family == dragon::Family::Drone);
  SIG_CHECK(withAxon(weak, name).family == dragon::Family::Flock);
  SIG_CHECK(withAxon(weak, vendor).family == dragon::Family::Axon);
  SIG_CHECK(withAxon(weak, oui).confidence == dragon::Confidence::Strong);
  SIG_CHECK(withAxon(none, name).family == dragon::Family::Axon);
  SIG_CHECK(withAxon(none, AxonEvidence{}).family == dragon::Family::Ble);
  // A registered identifier outranks the Medium vendor clue when both are present.
  AxonEvidence both;
  both.signals = BodyCameraVendor | AxonBwc;
  SIG_CHECK(both.match().confidence == dragon::Confidence::Strong);

  // End to end: a Find Hub tag whose public address sits in a shared-module block SquachWatch
  // lists under Axon stays a Find Hub tag.
  const uint8_t shared[6] = {0xe4, 0x05, 0x40, 1, 2, 3};
  Bytes p = {24, 0x16, 0xaa, 0xfe, 0x40};
  p.resize(25, 0x5a);
  auto f = dash::classifyBle(p.data(), p.size(), shared, true);
  SIG_CHECK(is(f.match, dash::Family::GoogleTag, dash::Confidence::High) &&
            labelled(f.label, "Find Hub"));
  // Alone, that address is still the weak Axon hint it always was.
  p = {2, 1, 6};
  SIG_CHECK(is(dash::classifyBle(p.data(), p.size(), shared, true).match, dash::Family::Axon,
               dash::Confidence::Low));
}

inline void disconnectRules() {
  // The documented numbers; docs/DETECTIONS.md and the evidence text quote them.
  static_assert(DisconnectFrames == 12 && DisconnectWindowMs == 2000, "per-sender rule");
  static_assert(BroadcastFrames == 6 && BroadcastAllFrames == 24 && BroadcastWindowMs == 3000,
                "broadcast rules");
  const uint8_t ap[6] = {0x10, 0x20, 0x30, 9, 9, 9};
  // Six broadcast-addressed frames from one sender.
  {
    TrafficHints hints;
    for (uint16_t i = 1; i <= BroadcastFrames; ++i) {
      auto p = disconnect(ap, true, i);
      auto hit = hints.wifi(p.data(), p.size(), i * 100);
      SIG_CHECK((hit.family == Family::Deauth) == (i == BroadcastFrames));
      if (i == BroadcastFrames)
        SIG_CHECK(hit.confidence == Confidence::Medium && strstr(hit.evidence, "6 broadcast"));
    }
    // The burst is reported once, not on every further frame.
    auto p = disconnect(ap, true, 90);
    SIG_CHECK(hints.wifi(p.data(), p.size(), 700).family == Family::None);
  }
  // The same six frames to one station are ordinary; that takes twelve.
  {
    TrafficHints hints;
    for (uint16_t i = 1; i <= DisconnectFrames; ++i) {
      auto p = disconnect(ap, false, i);
      SIG_CHECK((hints.wifi(p.data(), p.size(), i * 100).family == Family::Deauth) ==
                (i == DisconnectFrames));
    }
  }
  // Six broadcasts spread wider than the window never add up, and retries are not new frames.
  {
    TrafficHints hints;
    for (uint16_t i = 1; i <= 12; ++i) {
      auto p = disconnect(ap, true, i);
      SIG_CHECK(hints.wifi(p.data(), p.size(), i * 700).family == Family::None);
    }
    TrafficHints repeats;
    for (uint32_t i = 1; i <= 12; ++i) {
      auto p = disconnect(ap, true, 5);
      SIG_CHECK(repeats.wifi(p.data(), p.size(), i * 100).family == Family::None);
    }
  }
  // Twenty-four broadcasts from any mix of senders: a tool spoofing one AP after another.
  {
    TrafficHints hints;
    for (uint16_t i = 1; i <= BroadcastAllFrames; ++i) {
      uint8_t spoofed[6] = {0x10, 0x20, 0x30, 0x40, uint8_t(i), 1};
      auto p = disconnect(spoofed, true, i);
      auto hit = hints.wifi(p.data(), p.size(), i * 100);
      SIG_CHECK((hit.family == Family::Deauth) == (i == BroadcastAllFrames));
      if (i == BroadcastAllFrames)
        SIG_CHECK(hit.confidence == Confidence::Medium && strstr(hit.evidence, "24 broadcast"));
    }
  }
  // The same spread of senders talking to single stations is a busy network, not a flood.
  {
    TrafficHints hints;
    for (uint16_t i = 1; i <= 60; ++i) {
      uint8_t sender[6] = {0x10, 0x20, 0x30, 0x40, uint8_t(i), 1};
      auto p = disconnect(sender, false, i);
      SIG_CHECK(hints.wifi(p.data(), p.size(), i * 40).family == Family::None);
    }
    // Twenty-three broadcasts, a pause longer than the window, then one more.
    TrafficHints paused;
    for (uint16_t i = 1; i <= 24; ++i) {
      uint8_t sender[6] = {0x10, 0x20, 0x30, 0x40, uint8_t(i), 1};
      auto p = disconnect(sender, true, i);
      uint32_t at = i < 24 ? i * 100 : 2300 + BroadcastWindowMs + 100;
      SIG_CHECK(paused.wifi(p.data(), p.size(), at).family == Family::None);
    }
  }
}

inline void evilTwinRule() {
  const uint8_t home[6] = {0x34, 0x12, 0x98, 1, 2, 3};
  const uint8_t meshNode[6] = {0x34, 0x12, 0x98, 7, 8, 9};   // Same maker.
  const uint8_t guestRadio[6] = {0x36, 0x12, 0x98, 1, 2, 3}; // Same base, local bit set.
  const uint8_t stranger[6] = {0x44, 0x55, 0x66, 1, 2, 3};
  SIG_CHECK(sameVendor(home, meshNode) && sameVendor(home, guestRadio) &&
            !sameVendor(home, stranger));
  auto real = beacon(home, "Home", true);
  // One maker's open guest radio and open mesh node beside its protected network: ordinary.
  TrafficHints sameMaker;
  auto mesh = beacon(meshNode, "Home", false);
  auto guest = beacon(guestRadio, "Home", false);
  for (uint32_t i = 0; i < 5; ++i) {
    SIG_CHECK(sameMaker.wifi(real.data(), real.size(), 100 + i).family == Family::None);
    SIG_CHECK(sameMaker.wifi(mesh.data(), mesh.size(), 200 + i).family == Family::None);
    SIG_CHECK(sameMaker.wifi(guest.data(), guest.size(), 300 + i).family == Family::None);
  }
  // A different maker that agrees about security is a mixed-vendor network.
  TrafficHints mixed;
  auto agreeing = beacon(stranger, "Home", true);
  for (uint32_t i = 0; i < 5; ++i) {
    SIG_CHECK(mixed.wifi(real.data(), real.size(), 100 + i).family == Family::None);
    SIG_CHECK(mixed.wifi(agreeing.data(), agreeing.size(), 400 + i).family == Family::None);
  }
  // A different maker offering the same name without protection, each seen three times.
  TrafficHints hints;
  const uint8_t rogue[6] = {0x02, 0xc0, 0xca, 1, 2, 3};
  auto twin = beacon(rogue, "Home", false);
  for (uint32_t i = 0; i < 3; ++i)
    SIG_CHECK(hints.wifi(real.data(), real.size(), 100 + i).family == Family::None);
  for (uint32_t i = 0; i < 3; ++i) {
    auto hit = hints.wifi(twin.data(), twin.size(), 500 + i);
    SIG_CHECK((hit.family == Family::EvilTwin) == (i == 2));
    if (i == 2)
      SIG_CHECK(hit.confidence == Confidence::Medium);
  }
  // After 30 quiet seconds the pair has to be seen again from the start.
  SIG_CHECK(hints.wifi(twin.data(), twin.size(), 40000).family == Family::None);
}

// Labels are fixed-size, terminated and printable whatever arrives.
inline void labelFuzz() {
  std::mt19937 rng(20261115);
  const uint8_t seeds[][6] = {{0x10, 0x20, 0x30, 1, 2, 3},
                              {0, 0x25, 0xdf, 1, 2, 3},
                              {0x18, 0x7f, 0x88, 1, 2, 3},
                              {0x02, 0xc0, 0xca, 1, 2, 3}};
  auto clean = [](const char *label) {
    size_t n = 0;
    for (; n < LabelChars && label[n]; ++n)
      if (label[n] < 32 || label[n] > 126)
        return false;
    return n < LabelChars;
  };
  for (int i = 0; i < 20000; ++i) {
    Bytes q(rng() % 96);
    for (auto &v : q)
      v = uint8_t(rng());
    // Bias some cases toward the parsers that fill a label.
    if (i % 4 == 1 && q.size() > 30) {
      const uint8_t beaconHead[] = {26, 0xff, 0x4c, 0, 2, 0x15};
      memcpy(q.data(), beaconHead, sizeof(beaconHead));
      q.resize(27);
    }
    if (i % 4 == 2 && q.size() > 40) {
      q[0] = 0x80;
      q[1] = 0;
      memcpy(q.data() + 10, seeds[i % 3 + 1], 6);
      q[36] = 0;
      q[37] = uint8_t(q.size() - 38);
    }
    const uint8_t *mac = seeds[rng() % 4];
    auto f = classifyBle(q.data(), q.size(), mac, rng() & 1);
    SIG_CHECK(clean(f.label) && (f.match.family != Family::None || !*f.label));
    auto w = classifyWifi(q.data(), q.size());
    SIG_CHECK(clean(w.label) && (w.match.family != Family::None || !*w.label));
  }
}
} // namespace sigtest

inline void signatureTests() {
  sigtest::trackers();
  sigtest::beacons();
  sigtest::bodyCamerasAndGlasses();
  sigtest::gadgets();
  sigtest::names();
  sigtest::addresses();
  sigtest::wifiGadgetsAndDrones();
  sigtest::mapping();
  sigtest::axonPrecedence();
  sigtest::disconnectRules();
  sigtest::evilTwinRule();
  sigtest::labelFuzz();
  std::cout << "Signature, grading and rate-rule checks passed\n";
}
