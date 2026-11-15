// Host checks for include/recon.h and the tier-aware cooldowns in include/encounters.h.
// core_test.cpp calls reconTests() from main.
#pragma once
#include "encounters.h"
#include "personality.h"
#include "recon.h"
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
namespace recon_tests {
using namespace dash;
#define RECON_CHECK(expr)                                                                          \
  do {                                                                                             \
    if (!(expr)) {                                                                                 \
      std::cerr << "recon CHECK failed at " << __LINE__ << ": " << #expr << '\n';                  \
      std::exit(1);                                                                                \
    }                                                                                              \
  } while (0)

// A table over a plain array, as the firmware does over PSRAM.
struct Bench {
  std::vector<ReconDevice> rows;
  ReconTable table;
  AlertPolicy policy;
  AlertContext ctx;
  explicit Bench(unsigned capacity = 32) : rows(capacity) {
    table.attach(rows.data(), capacity);
  }
  static void mac(uint8_t out[6], unsigned id) {
    out[0] = 0x02;
    out[1] = 0;
    out[2] = uint8_t(id >> 24);
    out[3] = uint8_t(id >> 16);
    out[4] = uint8_t(id >> 8);
    out[5] = uint8_t(id);
  }
  ReconSighting sighting(const uint8_t *address, Family family, int rssi,
                         Confidence confidence = Confidence::Medium) {
    ReconSighting s;
    s.mac = address;
    s.ble = true;
    s.addressType = 1;
    s.match = {family, confidence, "test evidence"};
    s.rssi = rssi;
    return s;
  }
  ReconSeen see(unsigned id, Family family, uint32_t now, int rssi = -70,
                Confidence confidence = Confidence::Medium) {
    uint8_t address[6];
    mac(address, id);
    return table.observe(sighting(address, family, rssi, confidence), now);
  }
  // Observe and decide in one go, as radio.cpp does for each packet.
  AlertVerdict hear(unsigned id, Family family, uint32_t now, int rssi = -70,
                    Confidence confidence = Confidence::Medium) {
    ReconSeen seen = see(id, family, now, rssi, confidence);
    RECON_CHECK(seen.row);
    return policy.decide(*seen.row, seen, ctx, now);
  }
  ReconDevice *row(unsigned id, Family family) {
    uint8_t address[6];
    mac(address, id);
    return table.find(address, true, 1, family);
  }
};

// The numbers the behaviour was specified with; a change here should be a decision, not a slip.
static_assert(ReconLiveMs == 60000 && ReconAbsenceMs == 120000 && ReconTrendMs == 2000 &&
                  ReconTrendDb == 4 && ReconCapacity == 256 && RestoreKeep == 64,
              "table timings");
static_assert(SnoozeMarginDb == 7 && SnoozeDecayMs == 30 * 60000 && RenotifyMs == 5 * 60000 &&
                  AlertCooldowns::GlobalMs == 10000 && IgnoreList::Capacity == 64,
              "alert policy");
static_assert(FloodGate::Trip == 40 && FloodGate::WindowMs == 60000 &&
                  FloodGate::QuietMs == 5 * 60000,
              "flood gate");
static_assert(FollowPlaces == 3 && FollowMeters == 500 && FollowMs == 10 * 60000 &&
                  FollowPointCount == 4,
              "follow detection");
inline void tiersAndLabels() {
  struct Expect {
    Tier tier;
    std::vector<Family> families;
  };
  const Expect expected[] = {
      {Tier::Surveillance,
       {Family::Axon, Family::Flock, Family::Alpr, Family::Meta, Family::Raven, Family::Drone,
        Family::Camera, Family::Ring, Family::Skimmer, Family::Watchlist}},
      {Tier::Hacker,
       {Family::Flipper, Family::Biscuit, Family::Pineapple, Family::Pwnagotchi, Family::Nyanbox,
        Family::Halehound}},
      {Tier::Attack, {Family::Deauth, Family::EvilTwin, Family::BleSpam}},
      {Tier::Tracker, {Family::Tracker, Family::SamsungTag, Family::GoogleTag, Family::Tile}},
      {Tier::Beacon, {Family::Beacon, Family::None}}};
  unsigned covered = 0;
  for (const auto &e : expected)
    for (Family f : e.families) {
      RECON_CHECK(tierOf(f) == e.tier);
      ++covered;
    }
  RECON_CHECK(covered == FamilyCount); // Every family has been given a tier on purpose.
  RECON_CHECK(Tier::Surveillance < Tier::Hacker && Tier::Hacker < Tier::Attack &&
              Tier::Attack < Tier::Tracker && Tier::Tracker < Tier::Beacon);
  RECON_CHECK(!strcmp(tierName(Tier::Surveillance), "Surveillance") &&
              !strcmp(tierName(Tier::Beacon), "Beacon"));
  for (unsigned t = 0; t < TierCount; ++t)
    RECON_CHECK(*tierName(Tier(t)) && strlen(tierName(Tier(t))) <= 12);
  for (unsigned i = 0; i < FamilyCount; ++i) {
    const char *info = familyInfo(Family(i));
    RECON_CHECK(strlen(info) >= 20 && strlen(info) <= FamilyInfoChars);
    for (const char *p = info; *p; ++p)
      RECON_CHECK(*p >= 32 && *p < 127);
    for (unsigned k = 0; k < i; ++k) {
      RECON_CHECK(strcmp(info, familyInfo(Family(k))));
      RECON_CHECK(strcmp(familyKey(Family(i)), familyKey(Family(k))));
    }
    // File tokens: lower-case letters only, and they map back.
    for (const char *p = familyKey(Family(i)); *p; ++p)
      RECON_CHECK(*p >= 'a' && *p <= 'z');
    RECON_CHECK(familyFromKey(familyKey(Family(i))) == Family(i));
  }
  RECON_CHECK(familyFromKey("nonsense") == Family::None && familyFromKey(nullptr) == Family::None);
  RECON_CHECK(!strcmp(familyKey(Family::Flock), "flock") &&
              !strcmp(familyKey(Family::Ring), "ring"));
  for (unsigned v = 0; v <= unsigned(AlertVerdict::GlobalCooldown); ++v)
    RECON_CHECK(*verdictName(AlertVerdict(v)) && strlen(verdictName(AlertVerdict(v))) <= 15);
}

inline void keyingAndEncounters() {
  Bench b;
  uint8_t address[6];
  Bench::mac(address, 7);
  auto first = b.see(7, Family::Flock, 1000, -70);
  RECON_CHECK(first.sight == Sight::New && first.row && first.gapMs == 0);
  RECON_CHECK(first.row->encounters == 1 && first.row->packets == 1 && first.row->firstAt == 1000 &&
              first.row->arrivedAt == 1000);
  // A burst of packets is still one encounter.
  for (uint32_t t = 1100; t <= 60000; t += 100)
    RECON_CHECK(b.see(7, Family::Flock, t).sight == Sight::Seen);
  RECON_CHECK(first.row->encounters == 1 && first.row->packets == 591 && b.table.size() == 1);
  // Just short of the absence gap is the same encounter; the gap itself starts a new one.
  auto near = b.see(7, Family::Flock, 60000 + ReconAbsenceMs - 1);
  RECON_CHECK(near.sight == Sight::Seen && near.gapMs == ReconAbsenceMs - 1 &&
              near.row->encounters == 1);
  uint32_t back = near.row->lastAt + ReconAbsenceMs;
  auto again = b.see(7, Family::Flock, back);
  RECON_CHECK(again.sight == Sight::Returned && again.gapMs == ReconAbsenceMs &&
              again.row->encounters == 2 && again.row->arrivedAt == back &&
              again.row->firstAt == 1000);
  // The key is address + transport + address type + family.
  ReconSighting s = b.sighting(address, Family::Flock, -60);
  s.ble = false;
  s.addressType = 0;
  RECON_CHECK(b.table.observe(s, back).sight == Sight::New);
  s.ble = true;
  RECON_CHECK(b.table.observe(s, back).sight == Sight::New); // Public, not random.
  s.addressType = 1;
  RECON_CHECK(b.table.observe(s, back).sight == Sight::Seen);
  s.match.family = Family::Alpr;
  RECON_CHECK(b.table.observe(s, back).sight == Sight::New);
  RECON_CHECK(b.table.size() == 4);
  RECON_CHECK(b.see(8, Family::Flock, back).sight == Sight::New && b.table.size() == 5);
  // Evidence follows the best confidence; names and labels are kept when a packet has none.
  s = b.sighting(address, Family::Alpr, -60, Confidence::High);
  s.match.evidence = "strong";
  s.name = "PlateCam";
  s.label = "tag";
  s.channel = 6;
  auto row = b.table.observe(s, back + 10).row;
  s = b.sighting(address, Family::Alpr, -61, Confidence::Low);
  s.match.evidence = "weak";
  RECON_CHECK(b.table.observe(s, back + 20).row == row);
  RECON_CHECK(row->confidence == Confidence::High && !strcmp(row->evidence, "strong") &&
              !strcmp(row->name, "PlateCam") && !strcmp(row->label, "tag") && row->rssi == -61 &&
              row->peakRssi == -60);
  // Unclassified sightings and a table without storage are refused, not crashed on.
  s.match.family = Family::None;
  RECON_CHECK(b.table.observe(s, back).sight == Sight::Dropped);
  ReconTable empty;
  s.match.family = Family::Flock;
  RECON_CHECK(!empty.observe(s, 0).row && empty.capacity() == 0);
  // Names longer than the row are cut, never overrun.
  s.name = "A very long advertised name that will not fit in thirty-one characters";
  row = b.table.observe(s, back + 30).row;
  RECON_CHECK(strlen(row->name) == sizeof(row->name) - 1);
  s.rssi = -300;
  RECON_CHECK(b.table.observe(s, back + 40).row->rssi == -127);
  // The position is the last fix of the current encounter; a return without GPS has none.
  s.fix = true;
  s.lat = 40.5f;
  s.lon = -75.5f;
  s.utc = 800000000;
  row = b.table.observe(s, back + 50).row;
  s.fix = false;
  s.utc = 0;
  b.table.observe(s, back + 60);
  RECON_CHECK(row->lat == 40.5f && row->lon == -75.5f && row->utc == 800000000);
  RECON_CHECK(b.table.observe(s, back + 60 + ReconAbsenceMs).sight == Sight::Returned &&
              row->lat == 0 && row->lon == 0 && row->utc == 800000000);
}

inline void trendAndLive() {
  Bench b;
  auto row = b.see(1, Family::Flock, 0, -80).row;
  RECON_CHECK(reconTrend(*row) == 0 && reconLive(*row, 0));
  // Inside the two-second sample the comparison point stays where it was.
  b.see(1, Family::Flock, 500, -77);
  RECON_CHECK(row->prevRssi == -80 && reconTrend(*row) == 0); // 3 dB is noise.
  b.see(1, Family::Flock, 1000, -76);
  RECON_CHECK(reconTrend(*row) == 1); // 4 dB stronger: closer.
  b.see(1, Family::Flock, 2000, -70);
  RECON_CHECK(row->prevRssi == -76 && reconTrend(*row) == 1);
  b.see(1, Family::Flock, 4000, -74);
  RECON_CHECK(row->prevRssi == -70 && reconTrend(*row) == -1); // 4 dB weaker: further.
  b.see(1, Family::Flock, 6000, -75);
  RECON_CHECK(row->prevRssi == -74 && reconTrend(*row) == 0);
  RECON_CHECK(row->peakRssi == -70);
  // Live for a minute after the last packet, then stale.
  RECON_CHECK(reconLive(*row, 6000 + ReconLiveMs - 1) && !reconLive(*row, 6000 + ReconLiveMs));
  // A return after absence starts the trend afresh instead of comparing across the gap.
  b.see(1, Family::Flock, 6000 + ReconAbsenceMs, -40);
  RECON_CHECK(reconTrend(*row) == 0 && row->prevRssi == -40 && row->peakRssi == -40);
  ReconDevice blank;
  RECON_CHECK(!reconLive(blank, 0));
}

inline void eviction() {
  {
    // Four cameras in range: a tag or a gadget cannot take a slot, another camera can.
    Bench b(4);
    for (unsigned i = 0; i < 4; ++i)
      b.see(i, Family::Flock, 1000 + i * 1000);
    RECON_CHECK(b.see(10, Family::Tracker, 6000).sight == Sight::Dropped);
    RECON_CHECK(b.see(11, Family::Beacon, 6000).sight == Sight::Dropped);
    RECON_CHECK(b.see(12, Family::Flipper, 6000).sight == Sight::Dropped);
    RECON_CHECK(b.table.size() == 4 && b.row(0, Family::Flock));
    RECON_CHECK(b.see(13, Family::Axon, 6000).sight == Sight::New);
    RECON_CHECK(!b.row(0, Family::Flock) && b.row(1, Family::Flock)); // Longest unheard went.
    // Once the cameras are out of range a tag may replace the stalest of them.
    uint32_t later = 6000 + ReconLiveMs;
    RECON_CHECK(b.see(10, Family::Tracker, later).sight == Sight::New);
    RECON_CHECK(!b.row(1, Family::Flock) && b.row(13, Family::Axon) && b.table.size() == 4);
  }
  {
    // Order of sacrifice: restored, then stale, then lowest tier, then longest unheard.
    Bench b(5);
    LedgerEntry old;
    Bench::mac(old.mac, 50);
    old.ble = true;
    old.addressType = 1;
    old.family = Family::Axon;
    old.confidence = Confidence::High;
    RECON_CHECK(b.table.restore(old));
    b.see(1, Family::Tracker, 0);      // Will be stale.
    b.see(2, Family::Flock, 1000);     // Will be stale.
    b.see(3, Family::Tracker, 190000); // Live.
    b.see(4, Family::Flock, 195000);   // Live.
    uint32_t now = 200000;
    RECON_CHECK(b.see(20, Family::Tracker, now).sight == Sight::New);
    // History goes first, even a camera's while a stale tag is still listed.
    RECON_CHECK(!b.row(50, Family::Axon) && b.row(1, Family::Tracker) && b.row(2, Family::Flock));
    RECON_CHECK(b.see(21, Family::Tracker, now + 1).sight == Sight::New);
    RECON_CHECK(!b.row(1, Family::Tracker) && b.row(2, Family::Flock)); // Stale, lowest tier.
    RECON_CHECK(b.see(22, Family::Tracker, now + 2).sight == Sight::New);
    RECON_CHECK(!b.row(2, Family::Flock) && b.row(3, Family::Tracker)); // Stale before any live.
    RECON_CHECK(b.see(23, Family::Tracker, now + 3).sight == Sight::New);
    RECON_CHECK(!b.row(3, Family::Tracker) && b.row(4, Family::Flock)); // Oldest live tag.
    for (unsigned i = 24; i < 40; ++i) // A crowd of tags never pushes the live camera out.
      b.see(i, Family::Tracker, now + i);
    RECON_CHECK(b.row(4, Family::Flock) && b.table.size() == 5);
    // A newcomer of a higher tier takes the oldest live tag.
    RECON_CHECK(b.see(60, Family::Flipper, now + 100).sight == Sight::New);
    RECON_CHECK(b.row(4, Family::Flock) && !b.row(36, Family::Tracker) &&
                b.row(37, Family::Tracker));
  }
  {
    // Among stale rows the lower tier goes before the higher, and older before newer.
    Bench b(3);
    b.see(1, Family::Flock, 0);
    b.see(2, Family::Tracker, 5000);
    b.see(3, Family::Tracker, 1000);
    uint32_t now = 500000;
    b.see(4, Family::Axon, now);
    RECON_CHECK(!b.row(3, Family::Tracker) && b.row(2, Family::Tracker) && b.row(1, Family::Flock));
    b.see(5, Family::Axon, now);
    RECON_CHECK(!b.row(2, Family::Tracker) && b.row(1, Family::Flock));
    b.see(6, Family::Tracker, now);
    RECON_CHECK(!b.row(1, Family::Flock) && b.table.size() == 3);
  }
}

inline void sortingAndCounts() {
  Bench b(8);
  LedgerEntry old;
  Bench::mac(old.mac, 99);
  old.ble = true;
  old.addressType = 1;
  old.family = Family::Axon;
  old.confidence = Confidence::High;
  RECON_CHECK(b.table.restore(old));
  enum { A = 1, B, C, D, E };
  b.see(C, Family::Flock, 10000); // Long gone.
  b.see(A, Family::Tracker, 100000);
  b.see(B, Family::Flock, 120000);
  b.see(D, Family::Flipper, 130000);
  b.see(E, Family::Flock, 140000);
  b.see(E, Family::Flock, 141000);
  b.see(B, Family::Flock, 150000);
  b.see(D, Family::Flipper, 155000);
  b.see(A, Family::Tracker, 159000);
  uint32_t now = 160000;
  auto ids = [&](ReconSort order, unsigned capacity = 8) {
    uint16_t index[8];
    unsigned n = b.table.sorted(index, capacity, now, order);
    std::string out;
    for (unsigned i = 0; i < n; ++i) {
      const auto &d = b.table.at(index[i]);
      RECON_CHECK(d.used);
      out += d.mac[5] == 99 ? 'R' : char('A' + d.mac[5] - 1);
    }
    return out;
  };
  // Live first, then tier, then most recently arrived; history last.
  RECON_CHECK(ids(ReconSort::Priority) == "EBDACR");
  RECON_CHECK(ids(ReconSort::Recent) == "ADBECR");
  RECON_CHECK(ids(ReconSort::Priority, 3) == "EBD" && ids(ReconSort::Recent, 2) == "AD");
  RECON_CHECK(ids(ReconSort::Priority, 0).empty());
  // Another packet from B changes the recency order but not the steady priority order.
  b.see(B, Family::Flock, 159500);
  RECON_CHECK(ids(ReconSort::Priority) == "EBDACR" && ids(ReconSort::Recent) == "BADECR");
  ReconCounts c;
  b.table.counts(c, now);
  RECON_CHECK(c.total == 6 && c.live == 4 && c.restored == 1 && c.following == 0);
  RECON_CHECK(c.liveTier[unsigned(Tier::Surveillance)] == 2 &&
              c.liveTier[unsigned(Tier::Hacker)] == 1 && c.liveTier[unsigned(Tier::Tracker)] == 1 &&
              c.liveTier[unsigned(Tier::Attack)] == 0);
  RECON_CHECK(c.totalTier[unsigned(Tier::Surveillance)] == 4 &&
              c.liveFamily[unsigned(Family::Flock)] == 2 &&
              c.liveFamily[unsigned(Family::Axon)] == 0 &&
              c.liveFamily[unsigned(Family::Tracker)] == 1);
  // When everything has gone quiet the order is tier, then last heard.
  now = 400000;
  RECON_CHECK(ids(ReconSort::Priority) == "BECRDA" && ids(ReconSort::Recent) == "BADECR");
  b.table.counts(c, now);
  RECON_CHECK(c.live == 0 && c.total == 6);

  // Lifetime counters: one per new device, never per packet or per return, and they persist.
  RECON_CHECK(b.table.lifetime(Family::Flock) == 3 && b.table.lifetime(Family::Tracker) == 1 &&
              b.table.lifetime(Family::Axon) == 0 && b.table.lifetimeTotal() == 5);
  RECON_CHECK(b.table.takeLifetimeDirty() && !b.table.takeLifetimeDirty());
  b.see(B, Family::Flock, 900000);
  RECON_CHECK(b.table.lifetime(Family::Flock) == 3 && !b.table.takeLifetimeDirty());
  uint8_t blob[ReconTable::LifetimeBytes];
  RECON_CHECK(b.table.saveLifetime(blob, sizeof(blob)) == sizeof(blob));
  RECON_CHECK(b.table.saveLifetime(blob, sizeof(blob) - 1) == 0);
  Bench other;
  other.table.loadLifetime(blob, sizeof(blob));
  RECON_CHECK(other.table.lifetime(Family::Flock) == 3 && other.table.lifetimeTotal() == 5 &&
              !other.table.takeLifetimeDirty());
  other.see(1, Family::Ring, 0);
  RECON_CHECK(other.table.lifetime(Family::Ring) == 1 && other.table.lifetimeTotal() == 6);
  // A blob saved before the newest families existed still loads.
  other.table.loadLifetime(blob, (unsigned(Family::Flock) + 1) * 4);
  RECON_CHECK(other.table.lifetime(Family::Flock) == 3 &&
              other.table.lifetime(Family::Tracker) == 0);
  other.table.loadLifetime(nullptr, 0);
  RECON_CHECK(other.table.lifetimeTotal() == 0);
}

inline void typeFilter() {
  TypeFilter filter;
  RECON_CHECK(!filter.tracks(Family::Beacon) && !filter.tracks(Family::None));
  for (unsigned i = 1; i < FamilyCount; ++i)
    RECON_CHECK(filter.tracks(Family(i)) == (Family(i) != Family::Beacon));
  RECON_CHECK(filter.enabled() == TypeFilter::total() - 1 &&
              TypeFilter::total() == FamilyCount - 1);
  filter.set(Family::Beacon, true);
  filter.set(Family::Tile, false);
  filter.set(Family::None, true); // "Unclassified" can never be tracked.
  RECON_CHECK(filter.tracks(Family::Beacon) && !filter.tracks(Family::Tile) &&
              !filter.tracks(Family::None) && filter.enabled() == TypeFilter::total() - 1);
  // Saved as the families that are off, so junk bits and future families cannot switch one off.
  RECON_CHECK(filter.disabled() == TypeFilter::flag(Family::Tile));
  RECON_CHECK(TypeFilter::fromDisabled(filter.disabled()).mask == filter.mask);
  RECON_CHECK(TypeFilter::fromDisabled(0xfe000001u).enabled() == TypeFilter::total());
  RECON_CHECK(TypeFilter().disabled() == TypeFilter::flag(Family::Beacon));
  // The policy refuses a family that is off, and switching one off clears its rows.
  Bench b;
  b.ctx.filter = filter;
  RECON_CHECK(b.hear(1, Family::Tile, 0) == AlertVerdict::FilteredType);
  RECON_CHECK(b.hear(2, Family::Beacon, 0, -70, Confidence::High) == AlertVerdict::Alert);
  b.see(3, Family::Flock, 0);
  RECON_CHECK(b.table.prune(filter) == 1 && b.table.size() == 2 && !b.row(1, Family::Tile) &&
              b.row(3, Family::Flock));
}

inline void ignoreList() {
  IgnoreList list;
  uint8_t a[6], c[6];
  Bench::mac(a, 1);
  Bench::mac(c, 2);
  RECON_CHECK(list.add(a, Family::Tracker) && !list.add(a, Family::Tracker)); // No duplicates.
  RECON_CHECK(list.add(a, Family::Ring) && list.add(c, Family::Tracker) && list.size() == 3);
  RECON_CHECK(!list.add(c, Family::None));
  RECON_CHECK(list.contains(a, Family::Tracker) && !list.contains(c, Family::Ring));
  uint8_t blob[IgnoreList::BlobBytes];
  size_t bytes = list.save(blob, sizeof(blob));
  RECON_CHECK(bytes == 3 * IgnoreList::RecordBytes && list.save(blob, bytes - 1) == 0);
  IgnoreList copy;
  RECON_CHECK(copy.load(blob, bytes) && copy.size() == 3);
  for (unsigned i = 0; i < 3; ++i)
    RECON_CHECK(!memcmp(copy.at(i).mac, list.at(i).mac, 6) &&
                copy.at(i).family == list.at(i).family);
  RECON_CHECK(copy.remove(a, Family::Ring) && !copy.remove(a, Family::Ring) && copy.size() == 2 &&
              copy.contains(c, Family::Tracker));
  // A torn blob is refused and leaves the list alone; an unknown family is skipped.
  RECON_CHECK(!copy.load(blob, bytes - 1) && copy.size() == 2);
  blob[6] = 200;
  RECON_CHECK(copy.load(blob, bytes) && copy.size() == 2 && !copy.contains(a, Family::Tracker));
  RECON_CHECK(copy.load(nullptr, 0) && copy.size() == 0);
  // Sixty-four entries, then full.
  IgnoreList full;
  for (unsigned i = 0; i < IgnoreList::Capacity; ++i) {
    Bench::mac(a, 100 + i);
    RECON_CHECK(full.add(a, Family::Tile));
  }
  Bench::mac(a, 999);
  RECON_CHECK(!full.add(a, Family::Tile) && full.size() == IgnoreList::Capacity);
  RECON_CHECK(full.save(blob, sizeof(blob)) == sizeof(blob) && copy.load(blob, sizeof(blob)) &&
              copy.size() == IgnoreList::Capacity);
  full.clear();
  RECON_CHECK(full.size() == 0);

  // An ignored device is listed and counted but never alerts; un-ignoring brings it back.
  Bench b;
  Bench::mac(a, 5);
  b.policy.ignores.add(a, Family::Flock);
  RECON_CHECK(b.hear(5, Family::Flock, 1000, -40, Confidence::High) == AlertVerdict::Ignored);
  auto row = b.row(5, Family::Flock);
  ReconCounts counts;
  b.table.counts(counts, 1000);
  RECON_CHECK(row && row->ignored && !row->alerted && row->verdict == AlertVerdict::Ignored &&
              counts.live == 1 && b.table.lifetime(Family::Flock) == 1);
  RECON_CHECK(b.hear(5, Family::Alpr, 1000) == AlertVerdict::Alert); // Family is part of the key.
  b.policy.ignores.remove(a, Family::Flock);
  RECON_CHECK(b.hear(5, Family::Flock, 70000) == AlertVerdict::Alert && !row->ignored);
  b.policy.ignores.add(a, Family::Flock);
  b.table.applyIgnores(b.policy.ignores);
  RECON_CHECK(row->ignored && !b.row(5, Family::Alpr)->ignored);
}

inline void thresholdAndQuietRoad() {
  Bench b;
  // Everything is listed; the threshold only decides who may interrupt.
  RECON_CHECK(b.hear(1, Family::Flock, 0, -70, Confidence::Low) == AlertVerdict::BelowThreshold);
  RECON_CHECK(b.table.size() == 1 && !b.row(1, Family::Flock)->alerted);
  // Stronger evidence for the same device lifts it over the filter.
  RECON_CHECK(b.hear(1, Family::Flock, 1000, -70, Confidence::Medium) == AlertVerdict::Alert);
  RECON_CHECK(b.row(1, Family::Flock)->alerted &&
              b.row(1, Family::Flock)->verdict == AlertVerdict::Alert);
  RECON_CHECK(b.hear(1, Family::Flock, 2000) == AlertVerdict::SameEncounter);
  // ...and stronger still may alert again once the cooldowns allow.
  RECON_CHECK(b.hear(1, Family::Flock, 3000, -70, Confidence::High) ==
              AlertVerdict::FamilyCooldown);
  RECON_CHECK(b.hear(1, Family::Flock, 62000) == AlertVerdict::Alert);
  RECON_CHECK(b.hear(1, Family::Flock, 63000) == AlertVerdict::SameEncounter);

  // Quiet road: tags and Ring are list-only while a drive records or the car moves.
  Bench road;
  road.ctx.onRoad = true;
  for (Family f :
       {Family::Tracker, Family::SamsungTag, Family::GoogleTag, Family::Tile, Family::Ring}) {
    RECON_CHECK(quietOnRoad(f));
    RECON_CHECK(road.hear(unsigned(f), f, 1000, -50, Confidence::High) == AlertVerdict::QuietRoad);
    RECON_CHECK(!road.row(unsigned(f), f)->alerted);
  }
  RECON_CHECK(!quietOnRoad(Family::Flock) && !quietOnRoad(Family::Camera) &&
              !quietOnRoad(Family::Flipper) && !quietOnRoad(Family::Beacon));
  RECON_CHECK(road.table.size() == 5);
  RECON_CHECK(road.hear(40, Family::Flock, 1000) == AlertVerdict::Alert); // Cameras still alert.
  // Parked again: the tag that is still here is news now.
  road.ctx.onRoad = false;
  RECON_CHECK(road.hear(unsigned(Family::Tile), Family::Tile, 20000, -50, Confidence::High) ==
              AlertVerdict::Alert);
  // The user can switch the rule off.
  Bench loud;
  loud.ctx.onRoad = true;
  loud.ctx.quietRoad = false;
  RECON_CHECK(loud.hear(1, Family::Tracker, 0) == AlertVerdict::Alert);
  RECON_CHECK(loud.hear(2, Family::Ring, 11000) == AlertVerdict::Alert);
}

inline void autoSnooze() {
  RECON_CHECK(autoSnoozeChoice(0) == 0 && autoSnoozeChoice(3) == 0 && autoSnoozeChoice(5) == 5 &&
              autoSnoozeChoice(7) == 5 && autoSnoozeChoice(10) == 10 &&
              autoSnoozeChoice(200) == 10);
  const uint32_t step = RenotifyMs + 1000; // Each visit is a fresh encounter.
  {
    Bench b;
    uint32_t t = 0;
    const int rssi[] = {-80, -60, -80, -85, -80};
    for (int value : rssi) { // The default allowance: five alerts.
      RECON_CHECK(b.hear(1, Family::Flock, t, value) == AlertVerdict::Alert);
      t += step;
    }
    auto row = b.row(1, Family::Flock);
    RECON_CHECK(row->alerts == 5 && row->quietBar == -60); // The bar is its strongest alert.
    // Out of free ones: it must now beat that bar by 7 dB.
    RECON_CHECK(b.hear(1, Family::Flock, t, -60) == AlertVerdict::Snoozed);
    RECON_CHECK(row->verdict == AlertVerdict::Snoozed);
    RECON_CHECK(b.hear(1, Family::Flock, t += step, -54) == AlertVerdict::Snoozed);
    RECON_CHECK(b.hear(1, Family::Flock, t += step, -53) == AlertVerdict::Alert);
    RECON_CHECK(row->quietBar == -53 && row->alerts == 5);
    RECON_CHECK(b.hear(1, Family::Flock, t += step, -50) == AlertVerdict::Snoozed);
    RECON_CHECK(b.hear(1, Family::Flock, t += step, -46) == AlertVerdict::Alert);
    uint32_t last = t;
    // Half an hour after its last alert the allowance is handed back.
    RECON_CHECK(b.hear(1, Family::Flock, last + SnoozeDecayMs, -90) == AlertVerdict::Snoozed);
    RECON_CHECK(b.hear(1, Family::Flock, last + SnoozeDecayMs + step, -90) == AlertVerdict::Alert);
    RECON_CHECK(row->alerts == 1 && row->quietBar == -90);
    // A different device has its own allowance.
    RECON_CHECK(b.hear(2, Family::Flock, last + SnoozeDecayMs + 2 * step, -90) ==
                AlertVerdict::Alert);
  }
  {
    Bench b; // Ten, when the user picks ten.
    b.ctx.autoSnooze = 10;
    uint32_t t = 0;
    for (unsigned i = 0; i < 10; ++i, t += step)
      RECON_CHECK(b.hear(1, Family::Axon, t, -70) == AlertVerdict::Alert);
    RECON_CHECK(b.hear(1, Family::Axon, t, -70) == AlertVerdict::Snoozed);
    b.ctx.autoSnooze = 0; // Switched off: nothing is held.
    RECON_CHECK(b.hear(1, Family::Axon, t += step, -70) == AlertVerdict::Alert);
    b.ctx.autoSnooze = 5; // Switched back on, the alerts already spent still count.
    RECON_CHECK(b.hear(1, Family::Axon, t += step, -70) == AlertVerdict::Snoozed);
  }
  {
    Bench b; // Off never snoozes; a watchlist match is exempt at any setting.
    b.ctx.autoSnooze = 0;
    uint32_t t = 0;
    for (unsigned i = 0; i < 12; ++i, t += step)
      RECON_CHECK(b.hear(1, Family::Flock, t) == AlertVerdict::Alert);
    b.ctx.autoSnooze = 5;
    for (unsigned i = 0; i < 12; ++i, t += step)
      RECON_CHECK(b.hear(2, Family::Watchlist, t, -70, Confidence::High) == AlertVerdict::Alert);
  }
  {
    Bench b; // Reminders spend the allowance too, so a parked neighbour stops nagging.
    b.ctx.repeat = true;
    uint32_t t = 0;
    unsigned alerts = 0;
    for (; t <= 40 * 60000u; t += 10000)
      alerts += b.hear(1, Family::Ring, t) == AlertVerdict::Alert;
    RECON_CHECK(alerts == 5 && b.row(1, Family::Ring)->verdict == AlertVerdict::Snoozed);
  }
}

inline void floodGate() {
  {
    Bench b(128);
    // Thirty-nine new tags in under a minute: the first alerts, the rest wait their turn.
    RECON_CHECK(b.hear(0, Family::Tracker, 0) == AlertVerdict::Alert);
    for (unsigned i = 1; i < FloodGate::Trip - 1; ++i)
      RECON_CHECK(b.hear(i, Family::Tracker, i * 1000) == AlertVerdict::FamilyCooldown);
    RECON_CHECK(!b.policy.flooding(Family::Tracker, 38000));
    // A packet from a tag that already has a row is not a new address.
    RECON_CHECK(b.hear(3, Family::Tracker, 38500) == AlertVerdict::FamilyCooldown);
    // The fortieth raises ONE flood alert; after that the family is silent.
    RECON_CHECK(b.hear(39, Family::Tracker, 39000) == AlertVerdict::Flood);
    RECON_CHECK(b.policy.flooding(Family::Tracker, 39000));
    for (unsigned i = 40; i < 90; ++i)
      RECON_CHECK(b.hear(i, Family::Tracker, i * 1000) == AlertVerdict::FloodQuiet);
    RECON_CHECK(b.hear(5, Family::Tracker, 90000, -30, Confidence::High) ==
                AlertVerdict::FloodQuiet);
    RECON_CHECK(b.table.size() == 90); // Still listed.
    // Other families carry on as usual.
    RECON_CHECK(b.hear(200, Family::SamsungTag, 91000) == AlertVerdict::Alert);
    RECON_CHECK(b.hear(201, Family::Flock, 102000) == AlertVerdict::Alert);
    RECON_CHECK(!b.policy.flooding(Family::SamsungTag, 91000) &&
                !b.policy.flooding(Family::Flock, 91000));
    // A trickle of new ones keeps it shut; five minutes without any reopens it.
    uint32_t last = 89000 + 200000;
    RECON_CHECK(b.hear(300, Family::Tracker, last) == AlertVerdict::FloodQuiet);
    RECON_CHECK(b.policy.flooding(Family::Tracker, last + FloodGate::QuietMs - 1));
    RECON_CHECK(b.hear(5, Family::Tracker, last + FloodGate::QuietMs - 1) ==
                AlertVerdict::FloodQuiet);
    RECON_CHECK(!b.policy.flooding(Family::Tracker, last + FloodGate::QuietMs));
    RECON_CHECK(b.hear(301, Family::Tracker, last + FloodGate::QuietMs) == AlertVerdict::Alert);
    // The next flood needs a whole new burst, and gets its own single alert.
    uint32_t t = last + FloodGate::QuietMs + 1000;
    unsigned floods = 0;
    for (unsigned i = 400; i < 400 + FloodGate::Trip; ++i, t += 500)
      floods += b.hear(i, Family::Tracker, t) == AlertVerdict::Flood;
    RECON_CHECK(floods == 1 && b.policy.flooding(Family::Tracker, t));
  }
  {
    // Forty spread over more than a minute is a busy street, not a flood.
    FloodGate gate;
    for (unsigned i = 0; i < 200; ++i)
      RECON_CHECK(!gate.add(Family::Tile, i * 1600));
    RECON_CHECK(!gate.active(Family::Tile, 320000));
    // Exactly forty inside the window trips; families without a gate never do.
    FloodGate exact;
    for (unsigned i = 0; i < FloodGate::Trip - 1; ++i)
      RECON_CHECK(!exact.add(Family::GoogleTag, i * 1500));
    RECON_CHECK(exact.add(Family::GoogleTag, FloodGate::WindowMs));
    RECON_CHECK(!exact.add(Family::GoogleTag, FloodGate::WindowMs + 1));
    for (unsigned i = 0; i < 100; ++i)
      RECON_CHECK(!exact.add(Family::Flock, i) && !exact.add(Family::Ring, i));
  }
  {
    // On a quiet road the flood is swallowed too, and still silences the family.
    Bench b(128);
    b.ctx.onRoad = true;
    for (unsigned i = 0; i < 60; ++i) {
      AlertVerdict v = b.hear(i, Family::Tile, i * 500);
      RECON_CHECK(v ==
                  (i < FloodGate::Trip - 1 ? AlertVerdict::QuietRoad : AlertVerdict::FloodQuiet));
    }
    b.ctx.onRoad = false;
    RECON_CHECK(b.hear(1, Family::Tile, 31000) == AlertVerdict::FloodQuiet);
    // Ignored devices are known quantities and do not feed the gate.
    Bench own(128);
    uint8_t address[6];
    for (unsigned i = 0; i < 60; ++i) {
      Bench::mac(address, i);
      own.policy.ignores.add(address, Family::Tracker);
      RECON_CHECK(own.hear(i, Family::Tracker, i * 100) == AlertVerdict::Ignored);
    }
    RECON_CHECK(!own.policy.flooding(Family::Tracker, 6000));
  }
}

inline void cooldowns() {
  {
    Bench b;
    RECON_CHECK(b.hear(1, Family::Tracker, 1000) == AlertVerdict::Alert);
    // Another family of the same tier inside ten seconds waits...
    RECON_CHECK(b.hear(2, Family::SamsungTag, 2000) == AlertVerdict::GlobalCooldown);
    // ...but a camera or a hacker gadget is never swallowed by a tag alert.
    RECON_CHECK(b.hear(3, Family::Flipper, 3000) == AlertVerdict::Alert);
    RECON_CHECK(b.hear(4, Family::Flock, 4000) == AlertVerdict::Alert);
    // Nothing outranks the camera, so the ten seconds now hold for everyone.
    RECON_CHECK(b.hear(5, Family::Axon, 5000) == AlertVerdict::GlobalCooldown);
    RECON_CHECK(b.hear(6, Family::Pineapple, 6000) == AlertVerdict::GlobalCooldown);
    RECON_CHECK(b.hear(2, Family::SamsungTag, 13999) == AlertVerdict::GlobalCooldown);
    // The ones that waited alert as soon as the quiet ends, if they are still around.
    RECON_CHECK(b.hear(5, Family::Axon, 14000) == AlertVerdict::Alert);
    RECON_CHECK(b.hear(2, Family::SamsungTag, 24000) == AlertVerdict::Alert);
    // The family quiet period still limits a second device of one family.
    RECON_CHECK(b.hear(7, Family::Axon, 40000) == AlertVerdict::FamilyCooldown);
    RECON_CHECK(b.hear(7, Family::Axon, 73999) == AlertVerdict::FamilyCooldown);
    RECON_CHECK(b.hear(7, Family::Axon, 74000) == AlertVerdict::Alert);
    // An attack outranks a tag as well.
    RECON_CHECK(b.hear(8, Family::Tile, 100000) == AlertVerdict::Alert);
    RECON_CHECK(b.hear(9, Family::Deauth, 101000) == AlertVerdict::Alert);
    RECON_CHECK(b.hear(10, Family::Beacon, 102000, -70, Confidence::High) ==
                AlertVerdict::FilteredType);
  }
  {
    Bench b; // Same device: quiet until it has been away five minutes, unless reminders are on.
    RECON_CHECK(b.hear(1, Family::Flock, 0) == AlertVerdict::Alert);
    for (uint32_t t = 1000; t <= 600000; t += 1000)
      RECON_CHECK(b.hear(1, Family::Flock, t) == AlertVerdict::SameEncounter);
    // Away for two minutes is a new encounter in the list but not yet news.
    RECON_CHECK(b.hear(1, Family::Flock, 600000 + ReconAbsenceMs) == AlertVerdict::SameEncounter);
    RECON_CHECK(b.row(1, Family::Flock)->encounters == 2);
    uint32_t back = 600000 + ReconAbsenceMs + RenotifyMs;
    RECON_CHECK(b.hear(1, Family::Flock, back) == AlertVerdict::Alert);
    b.ctx.repeat = true;
    RECON_CHECK(b.hear(1, Family::Flock, back + RenotifyMs - 1) == AlertVerdict::SameEncounter);
    RECON_CHECK(b.hear(1, Family::Flock, back + RenotifyMs) == AlertVerdict::Alert);
  }
  {
    // The stand-alone tracker shares the same bypass.
    Encounters e;
    uint8_t address[6];
    Bench::mac(address, 1);
    auto &tag = e.observe(address, true, 1, Family::Tracker, 0, 300000);
    RECON_CHECK(e.eligible(tag, Confidence::Medium, 0, 60000, false));
    e.announce(tag, Confidence::Medium, 0);
    auto &tile = e.observe(address, true, 1, Family::Tile, 3000, 300000);
    auto &camera = e.observe(address, true, 1, Family::Flock, 3000, 300000);
    RECON_CHECK(!e.eligible(tile, Confidence::Medium, 3000, 60000, false));
    RECON_CHECK(e.eligible(camera, Confidence::Medium, 3000, 60000, false));
    e.announce(camera, Confidence::Medium, 3000);
    auto &axon = e.observe(address, true, 1, Family::Axon, 4000, 300000);
    RECON_CHECK(!e.eligible(axon, Confidence::Medium, 4000, 60000, false));
    RECON_CHECK(e.eligible(axon, Confidence::Medium, 13000, 60000, false));
    RECON_CHECK(e.eligible(tile, Confidence::Medium, 13000, 60000, false));
  }
}

inline void followDetection() {
  const float lat = 40.0f, lon = -75.0f, step = 0.01f; // 0.01 degrees of latitude is ~1.1 km.
  RECON_CHECK(fabsf(metersBetween(lat, lon, lat + step, lon) - 1113.2f) < 2);
  RECON_CHECK(fabsf(metersBetween(lat, lon, lat, lon + step) - 852.8f) < 2); // Shorter at 40 N.
  RECON_CHECK(metersBetween(lat, lon, lat, lon) == 0);
  RECON_CHECK(fabsf(metersBetween(0, 179.999f, 0, -179.999f) - 222.6f) <
              5); // Across the date line.
  RECON_CHECK(metersBetween(lat, lon, lat + 0.0044f, lon) < FollowMeters &&
              metersBetween(lat, lon, lat + 0.0046f, lon) > FollowMeters);
  const uint32_t minute = 60000;
  {
    ReconDevice tag;
    tag.family = Family::Tracker;
    RECON_CHECK(!followNote(tag, lat, lon, 0) && tag.pointCount == 1);
    RECON_CHECK(!followNote(tag, lat + step, lon, 5 * minute) && tag.pointCount == 2);
    // Three places, but only eight minutes: could be the car ahead at the lights.
    RECON_CHECK(!followNote(tag, lat + 2 * step, lon, 8 * minute) && tag.pointCount == 3);
    RECON_CHECK(!followNote(tag, lat + 2 * step, lon, 10 * minute)); // Exactly ten is not "more".
    // Still with us past ten minutes: flagged, even though it is the same third place.
    RECON_CHECK(followNote(tag, lat + 2 * step + 0.001f, lon, 10 * minute + 1));
    RECON_CHECK(tag.following && tag.pointCount == 3 && followSpanMs(tag) == 10 * minute + 1);
  }
  {
    ReconDevice tag; // Parked next to a tag for an hour, GPS wandering 100 m: never.
    for (unsigned i = 0; i <= 60; ++i)
      RECON_CHECK(!followNote(tag, lat + (i % 3) * 0.0009f, lon, i * minute));
    RECON_CHECK(tag.pointCount == 1);
    // Two places all day (home and the shop): never.
    ReconDevice commute;
    for (unsigned i = 0; i < 200; ++i)
      RECON_CHECK(!followNote(commute, lat + (i % 2) * step, lon, i * minute));
    RECON_CHECK(commute.pointCount == 2);
  }
  {
    ReconDevice tag; // A long drive keeps the first place as the anchor.
    for (unsigned i = 0; i < 9; ++i)
      followNote(tag, lat + i * step, lon, i * minute);
    RECON_CHECK(tag.pointCount == FollowPointCount && tag.points[0].lat == lat &&
                tag.points[0].at == 0 && tag.points[3].at == 8 * minute && !tag.following);
    for (unsigned a = 0; a < tag.pointCount; ++a)
      for (unsigned c = a + 1; c < tag.pointCount; ++c)
        RECON_CHECK(metersBetween(tag.points[a].lat, tag.points[a].lon, tag.points[c].lat,
                                  tag.points[c].lon) > FollowMeters);
    RECON_CHECK(followNote(tag, lat + 11 * step, lon, 11 * minute));
    ReconDevice mine; // The user's own tag is never flagged, however far it travels.
    mine.ignored = true;
    for (unsigned i = 0; i < 30; ++i)
      RECON_CHECK(!followNote(mine, lat + i * step, lon, i * minute));
  }
  RECON_CHECK(!followDetected(nullptr, 0, 0));
  {
    // Through the table: only tracker-tier rows keep a trail, and the warning is due once.
    Bench b;
    uint8_t address[6];
    Bench::mac(address, 1);
    auto drive = [&](Family family, unsigned i) {
      ReconSighting s = b.sighting(address, family, -60);
      s.fix = true;
      s.lat = lat + i * step;
      s.lon = lon;
      s.utc = 800000000 + i * 60;
      return b.table.observe(s, i * 4 * minute).row;
    };
    ReconDevice *tag = nullptr, *camera = nullptr;
    for (unsigned i = 0; i < 4; ++i) {
      RECON_CHECK(!tag || !b.table.followDue(*tag));
      tag = drive(Family::SamsungTag, i);
      camera = drive(Family::Flock, i);
    }
    RECON_CHECK(tag->following && tag->pointCount == 4 && tag->utc == 800000180 &&
                fabsf(tag->lat - (lat + 3 * step)) < 1e-5f && tag->lon == lon);
    RECON_CHECK(!camera->following && camera->pointCount == 0 && camera->lat == tag->lat);
    ReconCounts counts;
    b.table.counts(counts, 12 * minute);
    RECON_CHECK(counts.following == 1);
    RECON_CHECK(b.table.followDue(*tag) && !b.table.followDue(*tag) && !b.table.followDue(*camera));
    b.policy.announceFollow(*tag, 12 * minute);
    RECON_CHECK(tag->alerted);
    // Nothing talks over a follow warning for ten seconds, not even a camera.
    RECON_CHECK(b.hear(9, Family::Flock, 12 * minute + 5000) == AlertVerdict::GlobalCooldown);
    // Ignoring it afterwards clears the flag; a sighting without a fix leaves the trail alone.
    b.policy.ignores.add(address, Family::SamsungTag);
    b.table.applyIgnores(b.policy.ignores);
    RECON_CHECK(!tag->following && tag->ignored);
    Bench c;
    c.see(1, Family::Tile, 0);
    RECON_CHECK(c.row(1, Family::Tile)->pointCount == 0 && c.row(1, Family::Tile)->lat == 0);
  }
}

inline void ledger() {
  // Time stamps agree with the pet's clock helper and survive the round trip.
  char stamp[20];
  for (const char *text : {"2024-01-01 00:00:00", "2024-02-29 23:59:59", "2026-11-15 08:30:07",
                           "2028-03-01 12:00:00", "2099-12-31 23:59:59"}) {
    uint32_t seconds = utcFromText(text);
    RECON_CHECK(seconds && seconds == utcSeconds(text));
    utcText(stamp, seconds);
    RECON_CHECK(!strcmp(stamp, text));
  }
  utcText(stamp, 0);
  RECON_CHECK(!*stamp && utcFromText("") == 0 && utcFromText("2026-13-01 00:00:00") == 0 &&
              utcFromText("2026-01-01 00:00:00x") == 0 && utcFromText("yesterday") == 0);
  utcText(stamp, utcFromText("2100-03-01 00:00:00")); // 2100 is not a leap year.
  RECON_CHECK(!strcmp(stamp, "2100-03-01 00:00:00") &&
              utcFromText("2100-03-01 00:00:00") - utcFromText("2100-02-28 00:00:00") == 86400);

  ReconDevice d;
  const uint8_t mac[6] = {0xAA, 0xBB, 0x0C, 0x00, 0x1D, 0xEF};
  memcpy(d.mac, mac, 6);
  d.ble = true;
  d.addressType = 1;
  d.family = Family::SamsungTag;
  d.confidence = Confidence::Medium;
  snprintf(d.name, sizeof(d.name), "%s", "Cafe, \"Mochi\"\x01");
  snprintf(d.label, sizeof(d.label), "%s", "following");
  d.rssi = -67;
  d.channel = 0;
  d.utc = utcFromText("2026-11-15 08:30:07");
  d.lat = 39.952583f;
  d.lon = -75.165222f;
  d.alerted = true;
  char line[LedgerRowMax];
  size_t length = ledgerRow(line, sizeof(line), d, 123456, 42);
  RECON_CHECK(length && line[length - 1] == '\n' && strlen(line) == length);
  RECON_CHECK(std::string(line) ==
              "2026-11-15 08:30:07,123456,42,smarttag,medium,BLE1,AA:BB:0C:00:1D:EF,"
              "\"Cafe, \"\"Mochi\"\"?\",\"following\",-67,0,39.952583,-75.165222,1\n");
  LedgerEntry e;
  RECON_CHECK(ledgerParse(line, e));
  RECON_CHECK(e.utc == d.utc && e.uptimeMs == 123456 && e.boot == 42 &&
              e.family == Family::SamsungTag && e.confidence == Confidence::Medium && e.ble &&
              e.addressType == 1 && !memcmp(e.mac, mac, 6) && !strcmp(e.name, "Cafe, \"Mochi\"?") &&
              !strcmp(e.label, "following") && e.rssi == -67 && e.channel == 0 &&
              fabsf(e.lat - d.lat) < 1e-5f && fabsf(e.lon - d.lon) < 1e-5f && e.alerted);
  // Wi-Fi, no clock, no position, no name.
  ReconDevice w;
  memcpy(w.mac, mac, 6);
  w.family = Family::Flock;
  w.confidence = Confidence::High;
  w.rssi = -90;
  w.channel = 11;
  length = ledgerRow(line, sizeof(line), w, 4000000000u, 7);
  RECON_CHECK(std::string(line) ==
              ",4000000000,7,flock,high,WIFI,AA:BB:0C:00:1D:EF,\"\",\"\",-90,11,,,0\n");
  RECON_CHECK(ledgerParse(line, e) && e.utc == 0 && e.uptimeMs == 4000000000u && !e.ble &&
              e.addressType == 0 && e.channel == 11 && e.lat == 0 && e.lon == 0 && !e.alerted &&
              !*e.name && e.confidence == Confidence::High);
  // Every family and transport round-trips.
  for (unsigned i = 1; i < FamilyCount; ++i)
    for (unsigned type = 0; type < 4; ++type) {
      w.family = Family(i);
      w.ble = true;
      w.addressType = uint8_t(type);
      RECON_CHECK(ledgerRow(line, sizeof(line), w, 1, 1) && ledgerParse(line, e) &&
                  e.family == Family(i) && e.ble && e.addressType == type);
    }
  // The longest possible row still fits.
  ReconDevice big = d;
  memset(big.name, '"', sizeof(big.name) - 1);
  memset(big.label, '"', sizeof(big.label) - 1);
  big.family = Family::Pwnagotchi;
  big.utc = utcFromText("2099-12-31 23:59:59");
  big.lat = -89.999999f;
  big.lon = -179.99999f;
  big.rssi = -127;
  big.channel = 165;
  length = ledgerRow(line, sizeof(line), big, 0xffffffffu, 0xffffffffu);
  RECON_CHECK(length && length < LedgerRowMax - 1 && ledgerParse(line, e) &&
              strlen(e.name) == sizeof(e.name) - 1 && strlen(e.label) == sizeof(e.label) - 1 &&
              e.boot == 0xffffffffu);
  char small[40];
  RECON_CHECK(ledgerRow(small, sizeof(small), big, 1, 1) == 0);
  // Things that must not parse: the header, the old alert log, torn or doctored lines.
  RECON_CHECK(!ledgerParse(ledgerHeader(), e));
  RECON_CHECK(!ledgerParse("\"2026-01-01 00:00:00\",1234,\"Flock camera\",\"high\","
                           "\"AA:BB:CC:DD:EE:FF\",\"name\",-60,\"evidence\"",
                           e));
  const char *good = ",1,1,flock,high,WIFI,AA:BB:0C:00:1D:EF,\"\",\"\",-90,11,,,0";
  RECON_CHECK(ledgerParse(good, e));
  for (const char *bad : {"", "\n", ",1,1,flock,high,WIFI,AA:BB:0C:00:1D:EF,\"\",\"\",-90,11,,",
                          ",1,1,flock,high,WIFI,AA:BB:0C:00:1D:EF,\"\",\"\",-90,11,,,0,extra",
                          ",1,1,flock,high,WIFI,AA:BB:0C:00:1D:EF,\"torn",
                          ",1,1,flocks,high,WIFI,AA:BB:0C:00:1D:EF,\"\",\"\",-90,11,,,0",
                          ",1,1,flock,none,WIFI,AA:BB:0C:00:1D:EF,\"\",\"\",-90,11,,,0",
                          ",1,1,flock,high,LORA,AA:BB:0C:00:1D:EF,\"\",\"\",-90,11,,,0",
                          ",1,1,flock,high,WIFI,AA:BB:0C:00:1D,\"\",\"\",-90,11,,,0",
                          ",1,1,flock,high,WIFI,AA-BB-0C-00-1D-EF,\"\",\"\",-90,11,,,0",
                          ",1,1,flock,high,WIFI,AA:BB:0C:00:1D:EF,\"\",\"\",-900,11,,,0",
                          ",1,1,flock,high,WIFI,AA:BB:0C:00:1D:EF,\"\",\"\",-90,11,91.0,0.0,0",
                          ",1,1,flock,high,WIFI,AA:BB:0C:00:1D:EF,\"\",\"\",-90,11,12.0,,0",
                          ",-1,1,flock,high,WIFI,AA:BB:0C:00:1D:EF,\"\",\"\",-90,11,,,0",
                          ",1,99999999999,flock,high,WIFI,AA:BB:0C:00:1D:EF,\"\",\"\",-90,11,,,0",
                          "not a date,1,1,flock,high,WIFI,AA:BB:0C:00:1D:EF,\"\",\"\",-90,11,,,0",
                          ",1,1,flock,high,WIFI,AA:BB:0C:00:1D:EF,\"\",\"\",-90,11,,,2"})
    RECON_CHECK(!ledgerParse(bad, e));
  RECON_CHECK(!ledgerParse(nullptr, e));
  std::string tooLong(LedgerRowMax + 10, 'x');
  RECON_CHECK(!ledgerParse(tooLong.c_str(), e));

  // Restoring: the newest 64 distinct devices from the lines fed oldest first.
  Bench b(128);
  auto entry = [&](unsigned id, uint32_t uptime, uint32_t boot, bool alerted = false) {
    LedgerEntry r;
    Bench::mac(r.mac, id);
    r.ble = true;
    r.addressType = 1;
    r.family = id % 2 ? Family::Flock : Family::Tracker;
    r.confidence = Confidence::Medium;
    r.uptimeMs = uptime;
    r.boot = boot;
    r.rssi = -70;
    r.alerted = alerted;
    r.utc = 800000000 + id;
    snprintf(r.name, sizeof(r.name), "dev%u", id);
    return r;
  };
  for (unsigned id = 0; id < 70; ++id)
    RECON_CHECK(b.table.restore(entry(id, id * 1000, 3)));
  ReconCounts counts;
  b.table.counts(counts, 5000);
  RECON_CHECK(b.table.size() == RestoreKeep && counts.restored == RestoreKeep && counts.live == 0);
  RECON_CHECK(!b.row(5, Family::Flock) && b.row(6, Family::Tracker) && b.row(69, Family::Flock));
  RECON_CHECK(b.table.lifetimeTotal() == 0); // History was counted by the boot that saw it.
  uint16_t order[128];
  RECON_CHECK(b.table.sorted(order, 128, 5000, ReconSort::Recent) == RestoreKeep);
  RECON_CHECK(b.table.at(order[0]).mac[5] == 69 && b.table.at(order[63]).mac[5] == 6);
  RECON_CHECK(b.table.sorted(order, 128, 5000, ReconSort::Priority) == RestoreKeep);
  RECON_CHECK(b.table.at(order[0]).mac[5] == 69 && b.table.at(order[1]).mac[5] == 67 &&
              b.table.at(order[32]).mac[5] == 68); // Cameras before tags, newest first in each.
  auto row = b.row(69, Family::Flock);
  RECON_CHECK(row->restored && row->encounters == 1 && !row->alerted && row->boot == 3 &&
              row->utc == 800000069 && !strcmp(row->name, "dev69") && *row->evidence);
  // The alert line that follows a first sighting is the same encounter; a later line is not.
  RECON_CHECK(b.table.restore(entry(69, 69000, 3, true)));
  RECON_CHECK(row->encounters == 1 && row->alerted);
  RECON_CHECK(b.table.restore(entry(69, 400000, 3)) && row->encounters == 2 && row->alerted);
  RECON_CHECK(b.table.restore(entry(69, 400000, 4)) && row->encounters == 3);
  RECON_CHECK(b.table.size() == RestoreKeep);
  // Hearing a restored device again is a return, not a new device.
  ReconSeen seen = b.see(69, Family::Flock, 5000, -55);
  RECON_CHECK(seen.sight == Sight::Returned && seen.gapMs == UINT32_MAX && seen.row == row &&
              !row->restored && row->encounters == 4 && row->firstAt == 5000 &&
              row->peakRssi == -55 && reconLive(*row, 5000) && b.table.lifetimeTotal() == 0);
  RECON_CHECK(b.policy.decide(*row, seen, b.ctx, 5000) == AlertVerdict::Alert);
  b.table.counts(counts, 5000);
  RECON_CHECK(counts.restored == RestoreKeep - 1 && counts.live == 1);
  // A late card mount never overwrites what this boot has already heard...
  RECON_CHECK(!b.table.restore(entry(69, 1, 9)) && row->boot == 0 && row->encounters == 4);
  // ...and history never pushes out a row from this boot.
  Bench tiny(2);
  tiny.see(1, Family::Tracker, 0);
  tiny.see(2, Family::Tracker, 0);
  RECON_CHECK(!tiny.table.restore(entry(3, 0, 1)) && tiny.table.size() == 2);
  LedgerEntry none;
  RECON_CHECK(!b.table.restore(none));

  // The file tail as the firmware reads it: starts mid-line, ends with a line cut by power loss.
  auto fileLine = [&](unsigned id, Family family, uint32_t uptime) {
    ReconDevice d;
    Bench::mac(d.mac, id);
    d.ble = true;
    d.addressType = 1;
    d.family = family;
    d.confidence = Confidence::High;
    d.rssi = -60;
    char text[LedgerRowMax];
    RECON_CHECK(ledgerRow(text, sizeof(text), d, uptime, 8));
    return std::string(text);
  };
  std::string file = ledgerHeader();
  for (unsigned id = 1; id <= 5; ++id)
    file += fileLine(id, id == 3 ? Family::Beacon : Family::Flock, id * 1000);
  file += "\r\n";
  file += fileLine(2, Family::Flock, 9000);
  std::string torn = fileLine(6, Family::Flock, 10000);
  file += torn.substr(0, torn.size() - 5);
  Bench whole;
  std::string copy = file;
  RECON_CHECK(ledgerLoad(whole.table, &copy[0], false, whole.ctx.filter) == 5);
  RECON_CHECK(
      whole.table.size() == 4 && whole.row(1, Family::Flock) && !whole.row(3, Family::Beacon) &&
      !whole.row(6, Family::Flock) && whole.row(2, Family::Flock)->encounters == 2 &&
      whole.row(2, Family::Flock)->arrivedAt == 9000 && whole.row(2, Family::Flock)->boot == 8);
  RECON_CHECK(whole.table.sorted(order, 128, 0, ReconSort::Recent) == 4 &&
              whole.table.at(order[0]).mac[5] == 2 && whole.table.at(order[1]).mac[5] == 5);
  // Cut into the middle of device 1's line: that half line is dropped, the rest load.
  Bench tail;
  copy = file.substr(strlen(ledgerHeader()) + 20);
  RECON_CHECK(ledgerLoad(tail.table, &copy[0], true, tail.ctx.filter) == 4);
  RECON_CHECK(tail.table.size() == 3 && !tail.row(1, Family::Flock) && tail.row(4, Family::Flock));
  // Only the newest few when asked, and nothing at all from junk.
  Bench few;
  copy = file;
  RECON_CHECK(ledgerLoad(few.table, &copy[0], false, few.ctx.filter, 2) == 5 &&
              few.table.size() == 2 && few.row(2, Family::Flock) && few.row(5, Family::Flock));
  char junk[] = "no newline at all";
  char empty[] = "";
  RECON_CHECK(ledgerLoad(few.table, junk, true, few.ctx.filter) == 0 &&
              ledgerLoad(few.table, junk, false, few.ctx.filter) == 0 &&
              ledgerLoad(few.table, empty, false, few.ctx.filter) == 0);
  RECON_CHECK(ReconTable::live(*b.row(69, Family::Flock), 5000));
}

inline void wraparound() {
  Bench b;
  const uint32_t start = 0xffffff00u; // 256 ms before millis() rolls over.
  auto a = b.see(1, Family::Flock, start, -80).row;
  auto seen = b.see(1, Family::Flock, start + 1000, -80); // Now 744.
  RECON_CHECK(seen.sight == Sight::Seen && seen.gapMs == 1000 && a->encounters == 1);
  b.see(1, Family::Flock, start + 2500, -70);
  RECON_CHECK(reconTrend(*a) == 1); // The 2 s trend sample spans the rollover.
  RECON_CHECK(reconLive(*a, start + 2500 + ReconLiveMs - 1) &&
              !reconLive(*a, start + 2500 + ReconLiveMs));
  auto c = b.see(2, Family::Flock, start + 3000).row; // Arrived after the rollover.
  uint16_t order[32];
  RECON_CHECK(b.table.sorted(order, 32, start + 4000, ReconSort::Recent) == 2 &&
              &b.table.at(order[0]) == c && &b.table.at(order[1]) == a);
  RECON_CHECK(b.table.sorted(order, 32, start + 4000, ReconSort::Priority) == 2 &&
              &b.table.at(order[0]) == c);
  RECON_CHECK(b.see(1, Family::Flock, start + 2500 + ReconAbsenceMs).sight == Sight::Returned &&
              a->encounters == 2);
  // The policy's timers span the rollover too.
  Bench p;
  RECON_CHECK(p.hear(1, Family::Tracker, start) == AlertVerdict::Alert);
  RECON_CHECK(p.hear(2, Family::Tile, start + 9999) == AlertVerdict::GlobalCooldown);
  RECON_CHECK(p.hear(2, Family::Tile, start + 10000) == AlertVerdict::Alert);
  RECON_CHECK(p.hear(3, Family::Tracker, start + 59999) == AlertVerdict::FamilyCooldown);
  RECON_CHECK(p.hear(3, Family::Tracker, start + 60000) == AlertVerdict::Alert);
  RECON_CHECK(p.hear(1, Family::Tracker, start + 61000) == AlertVerdict::SameEncounter);
  FloodGate gate;
  bool tripped = false;
  for (unsigned i = 0; i < FloodGate::Trip; ++i)
    tripped = gate.add(Family::Tile, start + i * 100);
  RECON_CHECK(tripped && gate.active(Family::Tile, start + FloodGate::QuietMs) &&
              !gate.active(Family::Tile, start + 3900 + FloodGate::QuietMs));
  ReconDevice tag;
  followNote(tag, 40.00f, -75, start);
  followNote(tag, 40.01f, -75, start + 300000);
  RECON_CHECK(!followNote(tag, 40.02f, -75, start + 600000) &&
              followNote(tag, 40.02f, -75, start + 600001));

  // A device heard once and never again must not come back to life when millis() comes round
  // (about every 49.7 days), as long as the table is maintained now and then.
  Bench longRun;
  auto old = longRun.see(1, Family::Tracker, 1000).row;
  ReconSighting fix = longRun.sighting(old->mac, Family::Tracker, -70);
  fix.fix = true;
  fix.lat = 40;
  fix.lon = -75;
  longRun.table.observe(fix, 2000);
  RECON_CHECK(old->pointCount == 1);
  uint32_t now = 2000;
  for (unsigned i = 0; i < 16; ++i) { // Sixteen steps of 2^28 ms make one full turn.
    now += 1u << 28;
    longRun.table.maintain(now);
    RECON_CHECK(!reconLive(*old, now) && uint32_t(now - old->lastAt) <= ReconClampMs &&
                uint32_t(now - old->firstAt) <= ReconClampMs);
  }
  RECON_CHECK(now == 2000 && old->pointCount == 0 && !old->notified && old->alerts == 0);
  ReconCounts counts;
  longRun.table.counts(counts, now + 10);
  RECON_CHECK(counts.live == 0 && counts.total == 1);
  // Hearing it again after all that is a return like any other.
  RECON_CHECK(longRun.see(1, Family::Tracker, now + 10).sight == Sight::Returned &&
              reconLive(*old, now + 10) && old->encounters == 2);
  // maintain() leaves fresh rows exactly as they were.
  uint32_t arrived = old->arrivedAt, last = old->lastAt;
  longRun.table.maintain(now + 20);
  RECON_CHECK(old->arrivedAt == arrived && old->lastAt == last && old->encounters == 2 &&
              uint32_t(now + 20 - old->firstAt) == ReconClampMs); // First seen: "ages ago".
}

inline void run() {
  tiersAndLabels();
  keyingAndEncounters();
  trendAndLive();
  eviction();
  sortingAndCounts();
  typeFilter();
  ignoreList();
  thresholdAndQuietRoad();
  autoSnooze();
  floodGate();
  cooldowns();
  followDetection();
  ledger();
  wraparound();
  std::cout << "Recon table, alert policy, follow and ledger checks passed\n";
}
#undef RECON_CHECK
} // namespace recon_tests
inline void reconTests() {
  recon_tests::run();
}
