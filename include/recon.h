// SPDX-License-Identifier: GPL-3.0-or-later
// Recon engine: the device table behind every "what is around me" screen, the rules that decide
// which sightings may interrupt, and the on-card ledger format. Pure logic: no Arduino or M5 calls,
// so tests/recon_tests.h can exercise all of it on a PC.
//
// Listing and alerting are separate on purpose. Every classified device the type filter allows is
// listed (Low confidence and above); the user's alert threshold and the quiet rules below only
// decide whether the buddy speaks up. Behaviours and constants for auto snooze, the ignore list,
// the tracker flood gate, the closer/further trend and "tags and rings stay quiet on the road"
// follow SquachWatch-CYD (GPL-3.0); see docs/SOURCES.md. Follow detection is new here.
//
// All times are millis() values compared with unsigned subtraction, so the 49-day rollover is
// harmless as long as ReconTable::maintain() runs now and then (the firmware calls it twice a
// second).
#pragma once
#include "core.h"
#include <algorithm>
#include <new>
#include <stdlib.h>
namespace dash {
// ---- Tiers and plain-language labels --------------------------------------------------------
// Lower value = more important. The order drives sorting, eviction and the cooldown bypass.
enum class Tier : uint8_t { Surveillance, Hacker, Attack, Tracker, Beacon };
constexpr unsigned TierCount = 5;
inline Tier tierOf(Family f) {
  switch (f) {
  case Family::Axon:
  case Family::Flock:
  case Family::Alpr:
  case Family::Meta:
  case Family::Raven:
  case Family::Drone:
  case Family::Camera:
  case Family::Ring:
  case Family::Skimmer:
  case Family::Watchlist:
    return Tier::Surveillance;
  case Family::Flipper:
  case Family::Biscuit:
  case Family::Pineapple:
  case Family::Pwnagotchi:
  case Family::Nyanbox:
  case Family::Halehound:
    return Tier::Hacker;
  case Family::Deauth:
  case Family::EvilTwin:
  case Family::BleSpam:
    return Tier::Attack;
  case Family::Tracker:
  case Family::SamsungTag:
  case Family::GoogleTag:
  case Family::Tile:
    return Tier::Tracker;
  default:
    return Tier::Beacon;
  }
}
inline const char *tierName(Tier t) {
  static const char *const names[] = {"Surveillance", "Hacker gear", "Attack", "Tracker", "Beacon"};
  return unsigned(t) < TierCount ? names[unsigned(t)] : names[TierCount - 1];
}
// What the thing is and why it matters, in one or two sentences of at most FamilyInfoChars
// characters (ASCII), for a detail page or a spoken line.
constexpr unsigned FamilyInfoChars = 140;
inline const char *familyInfo(Family f) {
  static const char *const info[] = {
      "Not matched to any known signature yet.",
      "Police body camera or Taser gear made by Axon. An officer or patrol car is probably close "
      "and may be recording.",
      "Flock Safety license plate camera. It photographs passing cars and shares plates and times "
      "with police networks.",
      "Radio from a plate-reader maker. A fixed or mobile camera here may be logging every car "
      "that drives past.",
      "Smart glasses with a built-in camera and microphones. The wearer can film or record without "
      "it being obvious.",
      "Apple Find My tracker such as an AirTag. Harmless in a bag nearby; a concern if it keeps "
      "travelling with you.",
      "Flipper Zero pocket multi-tool. It can clone remotes and badges and spam Bluetooth; usually "
      "just a hobbyist.",
      "Biscuit handheld wireless test tool. Like a Flipper it can probe or pester nearby radios; "
      "someone is tinkering.",
      "WiFi Pineapple audit box. It can pose as networks your phone trusts to lure it in; avoid "
      "open WiFi around here.",
      "Pwnagotchi, a small gadget that collects WiFi handshakes for password cracking. It goes "
      "after networks, not you.",
      "Nyanbox wireless test gadget. It can scan, jam or spam WiFi and Bluetooth; somebody nearby "
      "is experimenting.",
      "HaleHound handheld radio toolkit. It is built for wireless testing and attacks, so odd "
      "glitches may not be chance.",
      "Bluetooth serial module of the kind found inside card skimmers. Common in harmless kit too; "
      "check pumps and ATMs.",
      "Signature of a Raven gunshot sensor. These pole-mounted units listen to the street around "
      "the clock.",
      "Drone broadcasting Remote ID. The signal gives the aircraft's position and often where its "
      "pilot is standing.",
      "Network camera or dashcam radio. Something nearby may be recording video of the street or "
      "the car park.",
      "Simple Bluetooth beacon used by shops and apps for indoor location. Nearly always harmless "
      "background noise.",
      "A burst of WiFi disconnect frames. Often routine roaming; sometimes a tool kicking devices "
      "off a network.",
      "One network name seen both open and secured. It may be a guest network, or a fake copy "
      "trying to lure devices.",
      "A storm of Bluetooth pairing adverts from many addresses. A crowd can cause it; so can a "
      "popup-spam gadget.",
      "Matches a rule on your own watchlist. You asked to be told whenever this device or maker "
      "shows up.",
      "Samsung SmartTag, or another Galaxy device in offline-finding mode. Worth a look only if "
      "it stays with you from place to place.",
      "Tracker on Google's Find Hub network, such as a Chipolo or Pebblebee. Watch for one that "
      "keeps following you.",
      "Tile tracker. Usually on keys or a bag close by; be wary only if the same one keeps "
      "turning up with you.",
      "Ring doorbell or security camera. It films its surroundings, and owners can hand footage "
      "to police."};
  static_assert(sizeof(info) / sizeof(info[0]) == FamilyCount, "one explanation per family");
  return unsigned(f) < FamilyCount ? info[unsigned(f)] : info[0];
}
// Stable lower-case tokens for files. Display names may be reworded; these never change.
inline const char *familyKey(Family f) {
  static const char *const keys[] = {
      "none",     "axon",      "flock",      "alpr",    "meta",      "tracker", "flipper",
      "biscuit",  "pineapple", "pwnagotchi", "nyanbox", "halehound", "skimmer", "raven",
      "drone",    "camera",    "beacon",     "deauth",  "eviltwin",  "blespam", "watchlist",
      "smarttag", "googletag", "tile",       "ring"};
  static_assert(sizeof(keys) / sizeof(keys[0]) == FamilyCount, "one file token per family");
  return unsigned(f) < FamilyCount ? keys[unsigned(f)] : keys[0];
}
inline Family familyFromKey(const char *key) {
  for (unsigned i = 1; key && i < FamilyCount; ++i)
    if (!strcmp(key, familyKey(Family(i))))
      return Family(i);
  return Family::None;
}

// ---- Tunables -------------------------------------------------------------------------------
constexpr unsigned ReconCapacity = 256;     // Rows the firmware asks for (PSRAM).
constexpr uint32_t ReconLiveMs = 60000;     // Heard within this long: the device is "live".
constexpr uint32_t ReconAbsenceMs = 120000; // Silent this long, then heard: a new encounter.
constexpr uint32_t ReconTrendMs = 2000;     // How often the comparison RSSI is re-sampled.
constexpr int ReconTrendDb = 4;             // Closer/further needs at least this much change.
constexpr uint32_t ReconClampMs = 1u << 30; // Ages saturate here (12 days) so they never wrap.
constexpr unsigned RestoreKeep = 64;        // Rows from earlier boots kept for the history list.
constexpr unsigned FollowPointCount = 4;
constexpr float FollowMeters = 500; // Places closer together than this count as one place.
// The company has to last longer than this. A tag riding with its owner changes its address
// about every 15 to 17 minutes and starts a new row each time, so the limit sits well above
// that: only an address that stays put (a tag away from its owner keeps one for about a day)
// can qualify.
constexpr uint32_t FollowMs = 1500000;
constexpr unsigned FollowPlaces = 3;    // ...across at least this many separate places.
constexpr uint32_t RenotifyMs = 300000; // Same device: quiet until absent this long (or reminder).
constexpr int SnoozeMarginDb = 7;       // A snoozed device must come this much closer to speak.
constexpr uint32_t SnoozeDecayMs = 1800000; // Its allowance returns this long after its last alert.

// ---- One row per device ---------------------------------------------------------------------
struct FollowPoint {
  float lat = 0, lon = 0;
  uint32_t at = 0; // millis() when the device was first heard at this place.
};
// Why the last sighting of a row did or did not interrupt. Anything but Alert/Flood is "quiet".
enum class AlertVerdict : uint8_t {
  Alert,          // Raise the normal alert.
  Flood,          // Raise ONE flood alert for this family; the family then stays quiet.
  FilteredType,   // Family is switched off in the type filter (it is not listed either).
  Ignored,        // On the user's ignore list.
  BelowThreshold, // Listed, but under the user's alert filter.
  FloodQuiet,     // The family is in a flood (many new addresses); wait for it to pass.
  QuietRoad,      // Tags and Ring cameras stay list-only while driving.
  SameEncounter,  // Already announced; nothing new since.
  Snoozed,        // Auto snooze: it must come closer than its strongest alert.
  FamilyCooldown, // Another device of this family alerted moments ago.
  GlobalCooldown  // Some other alert fired within the last ten seconds.
};
inline const char *verdictName(AlertVerdict v) { // Short enough for a status line: 15 chars.
  static const char *const names[] = {"alert",         "flood alert",  "type is off",
                                      "ignored",       "below filter", "flood quiet",
                                      "quiet on road", "already told", "auto snoozed",
                                      "family quiet",  "just alerted"};
  return unsigned(v) < sizeof(names) / sizeof(names[0]) ? names[unsigned(v)] : "";
}
struct ReconDevice {
  // Identity. One row per (mac, ble, addressType, family).
  uint8_t mac[6]{};
  bool used = false, ble = false;
  uint8_t addressType = 0; // BLE address type as NimBLE reports it; 0 for Wi-Fi.
  Family family = Family::None;
  Confidence confidence = Confidence::None; // Best seen for this row.
  const char *evidence = "";                // Static text from the classifier; never freed.
  char name[32]{};                          // Advertised name or SSID; empty if none.
  // Product inside the family, from the signature ("Axon Body 3", "Chipolo"). Rows read back from
  // the ledger may instead hold the engine's own tag for that line; see ledgerTag().
  char label[24]{};
  uint8_t channel = 0;                         // Wi-Fi channel; 0 for BLE.
  int8_t rssi = 0, prevRssi = 0, peakRssi = 0; // Latest, the ~2 s old comparison sample, strongest.
  // millis() times. For a restored row firstAt/lastAt hold the uptime of the boot that wrote it and
  // are only informative: use utc and boot instead.
  uint32_t firstAt = 0, lastAt = 0;
  uint32_t arrivedAt = 0;  // Start of the current encounter.
  uint32_t encounters = 0; // Grows only after an absence of ReconAbsenceMs, never per packet.
  uint32_t packets = 0;
  bool alerted = false;   // Has raised an alert (this boot, or in the ledger for a restored row).
  bool ignored = false;   // On the ignore list: listed and counted, never alerts.
  bool restored = false;  // Loaded from the ledger and not heard again since this boot began.
  bool following = false; // Tracker seen at several separate places over time (see followNote).
  bool flood = false;     // This address tripped a tracker flood alert.
  uint32_t utc = 0;       // Seconds since 2000 (UTC) of the last sighting; 0 if unknown.
  float lat = 0, lon = 0; // Last GPS position it was heard at this encounter; 0,0 if none.
  uint32_t boot = 0;      // Boot counter at the last sighting.
  // Bookkeeping owned by the table and the policy; screens only need to read it.
  AlertVerdict verdict = AlertVerdict::SameEncounter; // Outcome of the latest alert decision.
  bool notified = false, followAlerted = false, pendingLog = false;
  Confidence notifiedConfidence = Confidence::None;
  uint8_t alerts = 0;  // Alerts spent from the auto-snooze allowance.
  int8_t quietBar = 0; // Strongest RSSI it has alerted at.
  uint32_t announcedAt = 0, trendAt = 0, restoreSeq = 0;
  FollowPoint points[FollowPointCount];
  uint8_t pointCount = 0;
  uint32_t fixAt = 0; // millis() of the latest sighting that had a GPS fix.
};
inline bool reconLive(const ReconDevice &d, uint32_t now) {
  return d.used && !d.restored && uint32_t(now - d.lastAt) < ReconLiveMs;
}
// +1 closer, -1 further, 0 steady, comparing the latest reading with one about two seconds old.
inline int reconTrend(const ReconDevice &d) {
  int change = int(d.rssi) - int(d.prevRssi);
  return change >= ReconTrendDb ? 1 : change <= -ReconTrendDb ? -1 : 0;
}
// What kind of address a row is keyed by, which decides how much "seen again" can mean.
enum class AddressKind : uint8_t {
  Fixed,       // Public BLE or a burned-in Wi-Fi address: the same device every time.
  Rotating,    // BLE private address: changes every few minutes to a day.
  StaticRandom // BLE static random, or a locally administered Wi-Fi address: stable until the
               // device is reset or (Wi-Fi) joins another network.
};
inline AddressKind addressKind(bool ble, uint8_t addressType, const uint8_t *mac) {
  if (!ble)
    return mac[0] & 2 ? AddressKind::StaticRandom : AddressKind::Fixed;
  if (addressType != 1) // 0 public, 2 and 3 are identity addresses resolved by the stack.
    return AddressKind::Fixed;
  return (mac[0] & 0xc0) == 0xc0 ? AddressKind::StaticRandom : AddressKind::Rotating;
}
inline const char *addressKindName(AddressKind k) { // At most 16 characters.
  return k == AddressKind::Rotating       ? "rotating address"
         : k == AddressKind::StaticRandom ? "random address"
                                          : "fixed address";
}
// The engine's own ledger tags. A row restored from the ledger can carry one in `label`.
inline bool ledgerTag(const char *label) {
  return label && (!strcmp(label, "following") || !strcmp(label, "flood"));
}
inline void macText(char out[18], const uint8_t *mac) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4],
           mac[5]);
}

// ---- Follow detection -----------------------------------------------------------------------
// Flat-earth distance; exact enough for a 500 m question anywhere a car can go.
inline float metersBetween(float lat1, float lon1, float lat2, float lon2) {
  const double rad = 0.017453292519943295, perDegree = 111320.0;
  double dLon = double(lon2) - double(lon1);
  if (dLon > 180)
    dLon -= 360;
  else if (dLon < -180)
    dLon += 360;
  double x = dLon * cos((double(lat1) + double(lat2)) * 0.5 * rad);
  double y = double(lat2) - double(lat1);
  return float(sqrt(x * x + y * y) * perDegree);
}
// Stored places are always more than FollowMeters from each other, oldest first, so the test is a
// count and a time span. lastFixAt is the latest time the device was heard with a fix.
inline bool followDetected(const FollowPoint *points, unsigned count, uint32_t lastFixAt) {
  return count >= FollowPlaces && uint32_t(lastFixAt - points[0].at) > FollowMs;
}
// Record where a tracker-tier device was just heard and refresh row.following. A stationary tag
// can never qualify: three places 500 m apart are out of one tag's radio range. An ignored device
// keeps its trail but is never flagged.
//
// Limits worth knowing: the identity is the advertised address. A tag separated from its owner
// keeps one address for about a day, which is the case that matters; a tag near its owner rotates
// roughly every 15 minutes and starts a new row each time, which is why FollowMs is longer than
// that. The ignore list is keyed by address too, so IGNORE only holds for a fixed address. Rows
// are NOT folded by advert payload:
// in Find My, SmartTag and Find Hub adverts the payload is key material that rotates together
// with the address, and the bytes that stay put are shared by every tag of that model, so a
// payload match would either never fire or merge strangers.
inline bool followNote(ReconDevice &row, float lat, float lon, uint32_t now) {
  row.fixAt = now;
  bool known = false;
  for (unsigned i = 0; i < row.pointCount; ++i)
    known |= metersBetween(row.points[i].lat, row.points[i].lon, lat, lon) <= FollowMeters;
  if (!known) {
    if (row.pointCount == FollowPointCount) {
      // Keep the first place as the time anchor; forget the second oldest.
      for (unsigned i = 1; i + 1 < FollowPointCount; ++i)
        row.points[i] = row.points[i + 1];
      --row.pointCount;
    }
    row.points[row.pointCount++] = {lat, lon, now};
  }
  row.following = !row.ignored && followDetected(row.points, row.pointCount, row.fixAt);
  return row.following;
}
inline uint32_t followSpanMs(const ReconDevice &row) {
  return row.pointCount ? uint32_t(row.fixAt - row.points[0].at) : 0;
}
// Length of the remembered trail: place to place, oldest first. A lower bound on the distance
// travelled together, since only up to FollowPointCount places are kept.
inline float followSpanMeters(const ReconDevice &row) {
  float sum = 0;
  for (unsigned i = 1; i < row.pointCount && i < FollowPointCount; ++i)
    sum += metersBetween(row.points[i - 1].lat, row.points[i - 1].lon, row.points[i].lat,
                         row.points[i].lon);
  return sum;
}

// ---- Type filter ----------------------------------------------------------------------------
// Which families are tracked at all. A family that is off is neither listed nor counted.
struct TypeFilter {
  static constexpr uint32_t flag(Family f) {
    return unsigned(f) < 32 ? 1u << unsigned(f) : 0;
  }
  static constexpr uint32_t All = ((FamilyCount < 32 ? (1u << FamilyCount) : 0u) - 1u) & ~1u;
  // Shop beacons are everywhere and tell you nothing, so they start switched off.
  static constexpr uint32_t Defaults = All & ~(1u << unsigned(Family::Beacon));
  uint32_t mask = Defaults;
  bool tracks(Family f) const {
    return f != Family::None && (mask & flag(f));
  }
  void set(Family f, bool on) {
    mask = (on ? mask | flag(f) : mask & ~flag(f)) & All;
  }
  unsigned enabled() const {
    unsigned n = 0;
    for (unsigned i = 1; i < FamilyCount; ++i)
      n += (mask >> i) & 1;
    return n;
  }
  static constexpr unsigned total() {
    return FamilyCount - 1;
  }
  // Saved as the families switched OFF, so a family added by a later firmware starts ON.
  uint32_t disabled() const {
    return ~mask & All;
  }
  static TypeFilter fromDisabled(uint32_t off) {
    TypeFilter f;
    f.mask = ~off & All;
    return f;
  }
};

// ---- Ignore list ----------------------------------------------------------------------------
// The user's own gear (their tag, their dashcam). Ignored devices are still listed and counted;
// they just never alert. Saved as one blob of 7-byte records: six address bytes and the family.
class IgnoreList {
public:
  static constexpr unsigned Capacity = 64, RecordBytes = 7, BlobBytes = Capacity * RecordBytes;
  struct Entry {
    uint8_t mac[6]{};
    Family family = Family::None;
  };
  unsigned size() const {
    return count_;
  }
  const Entry &at(unsigned i) const {
    return entries_[i < count_ ? i : 0];
  }
  bool contains(const uint8_t *mac, Family family) const {
    return find(mac, family) >= 0;
  }
  // False when the list is full or the device is already on it.
  bool add(const uint8_t *mac, Family family) {
    if (count_ == Capacity || family == Family::None || unsigned(family) >= FamilyCount ||
        contains(mac, family))
      return false;
    memcpy(entries_[count_].mac, mac, 6);
    entries_[count_++].family = family;
    return true;
  }
  bool remove(const uint8_t *mac, Family family) {
    int i = find(mac, family);
    if (i < 0)
      return false;
    for (unsigned k = unsigned(i); k + 1 < count_; ++k)
      entries_[k] = entries_[k + 1];
    --count_;
    return true;
  }
  void clear() {
    count_ = 0;
  }
  // Returns the bytes written (0 for an empty list, or if the buffer is too small).
  size_t save(uint8_t *out, size_t capacity) const {
    if (capacity < size_t(count_) * RecordBytes)
      return 0;
    for (unsigned i = 0; i < count_; ++i) {
      memcpy(out + i * RecordBytes, entries_[i].mac, 6);
      out[i * RecordBytes + 6] = uint8_t(entries_[i].family);
    }
    return size_t(count_) * RecordBytes;
  }
  // Replaces the list. A torn blob is refused; records naming an unknown family are skipped.
  bool load(const uint8_t *in, size_t bytes) {
    if (bytes % RecordBytes || bytes > BlobBytes || (bytes && !in))
      return false;
    count_ = 0;
    for (size_t at = 0; at < bytes; at += RecordBytes)
      add(in + at, Family(in[at + 6]));
    return true;
  }

private:
  int find(const uint8_t *mac, Family family) const {
    for (unsigned i = 0; i < count_; ++i)
      if (entries_[i].family == family && !memcmp(entries_[i].mac, mac, 6))
        return int(i);
    return -1;
  }
  Entry entries_[Capacity];
  unsigned count_ = 0;
};

// ---- Ledger rows (see docs/DEVELOPMENT.md) ---------------------------------------------------
constexpr const char *LedgerPath = "/events/recon.csv";
constexpr size_t LedgerRowMax = 256;        // Longest line ledgerRow() can produce, with its NUL.
constexpr uint32_t LedgerTailBytes = 16384; // How much of the file's end is read back at boot.
inline const char *ledgerHeader() {
  return "UTC,UptimeMs,Boot,Family,Confidence,Transport,MAC,Name,Label,RSSI,Channel,Lat,Lon,"
         "Alerted\n";
}
struct LedgerEntry {
  uint32_t utc = 0, uptimeMs = 0, boot = 0;
  Family family = Family::None;
  Confidence confidence = Confidence::None;
  bool ble = false, alerted = false;
  uint8_t addressType = 0, mac[6]{}, channel = 0;
  char name[32]{}, label[24]{};
  int8_t rssi = 0;
  float lat = 0, lon = 0;
};
inline bool leapYear(unsigned y) {
  return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
}
// Seconds since 2000-01-01 UTC as "YYYY-MM-DD HH:MM:SS"; empty text for 0 (time unknown).
inline void utcText(char out[20], uint32_t seconds) {
  out[0] = 0;
  if (!seconds)
    return;
  uint32_t days = seconds / 86400, rest = seconds % 86400;
  unsigned y = 2000;
  while (days >= (leapYear(y) ? 366u : 365u))
    days -= leapYear(y++) ? 366 : 365;
  static const uint8_t lengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  unsigned m = 0;
  for (;; ++m) {
    unsigned length = lengths[m] + (m == 1 && leapYear(y) ? 1 : 0);
    if (days < length)
      break;
    days -= length;
  }
  char text[40]; // Roomy so the compiler can see nothing is cut; the result is always 19 chars.
  snprintf(text, sizeof(text), "%04u-%02u-%02u %02u:%02u:%02u", y, m + 1, unsigned(days) + 1,
           unsigned(rest / 3600), unsigned(rest / 60 % 60), unsigned(rest % 60));
  memcpy(out, text, 19);
  out[19] = 0;
}
inline uint32_t utcFromText(const char *text) { // 0 when empty or not a valid stamp.
  unsigned y, mo, d, h, mi, s;
  char extra;
  if (!text || sscanf(text, "%4u-%2u-%2u %2u:%2u:%2u%c", &y, &mo, &d, &h, &mi, &s, &extra) != 6)
    return 0;
  if (y < 2000 || y > 2135 || mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || s > 59)
    return 0;
  static const uint16_t before[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  uint32_t days = before[mo - 1] + (d - 1) + (mo > 2 && leapYear(y) ? 1 : 0);
  for (unsigned year = 2000; year < y; ++year)
    days += leapYear(year) ? 366 : 365;
  return days * 86400u + h * 3600u + mi * 60u + s;
}
// "WIFI", "BLE" (public address) or "BLE1".."BLE3" (NimBLE address type: random, public identity,
// random identity). The type is part of a device's identity, so it has to survive the round trip.
inline void transportText(char out[8], bool ble, uint8_t addressType) {
  if (!ble)
    snprintf(out, 8, "WIFI");
  else if (!addressType)
    snprintf(out, 8, "BLE");
  else
    snprintf(out, 8, "BLE%u", unsigned(addressType % 10));
}
// One CSV line, newline included. Returns its length, or 0 if the buffer is too small.
// uptimeMs is the start of the encounter being recorded and boot is the current boot counter.
inline size_t ledgerRow(char *out, size_t capacity, const ReconDevice &d, uint32_t uptimeMs,
                        uint32_t boot) {
  char utc[20], mac[18], transport[8], position[40] = ",";
  utcText(utc, d.utc);
  macText(mac, d.mac);
  transportText(transport, d.ble, d.addressType);
  if (d.lat != 0 || d.lon != 0)
    snprintf(position, sizeof(position), "%.6f,%.6f", double(d.lat), double(d.lon));
  // Quoted text fields: quotes are doubled and anything unprintable becomes '?'.
  char quoted[2][70];
  // The engine's tag for this line wins the Label column; otherwise the product label is kept.
  const char *texts[2] = {d.name, d.following ? "following" : d.flood ? "flood" : d.label};
  for (unsigned k = 0; k < 2; ++k) {
    size_t n = 0;
    quoted[k][n++] = '"';
    for (const char *p = texts[k]; *p && n < sizeof(quoted[k]) - 3; ++p) {
      char c = *p >= 32 && *p < 127 ? *p : '?';
      if (c == '"')
        quoted[k][n++] = '"';
      quoted[k][n++] = c;
    }
    quoted[k][n++] = '"';
    quoted[k][n] = 0;
  }
  int n = snprintf(out, capacity, "%s,%lu,%lu,%s,%s,%s,%s,%s,%s,%d,%u,%s,%u\n", utc,
                   (unsigned long)uptimeMs, (unsigned long)boot, familyKey(d.family),
                   confidenceName(d.confidence), transport, mac, quoted[0], quoted[1], int(d.rssi),
                   unsigned(d.channel), position, d.alerted ? 1u : 0u);
  return n > 0 && size_t(n) < capacity ? size_t(n) : 0;
}
namespace ledger_detail {
// Split one CSV line in place into at most `max` fields; returns the number found (max + 1 if
// there are more). Quoted fields may hold commas and doubled quotes.
inline unsigned split(char *line, char **fields, unsigned max) {
  unsigned count = 0;
  char *p = line;
  for (;;) {
    if (count == max)
      return max + 1;
    char *write = p;
    fields[count++] = write;
    if (*p == '"') {
      for (++p; *p; ++p) {
        if (*p == '"') {
          if (p[1] != '"')
            break;
          ++p;
        }
        *write++ = *p;
      }
      if (*p != '"')
        return 0; // Unterminated quote: a torn line.
      ++p;
    } else
      while (*p && *p != ',')
        *write++ = *p++;
    char end = *p;
    *write = 0;
    if (end != ',')
      return end ? 0 : count;
    ++p;
  }
}
inline bool number(const char *text, long low, long high, long &out) {
  if (!*text)
    return false;
  char *end = nullptr;
  out = strtol(text, &end, 10);
  return !*end && out >= low && out <= high;
}
inline bool unsignedNumber(const char *text, uint32_t &out) {
  if (!*text || *text == '-' || *text == '+')
    return false;
  char *end = nullptr;
  unsigned long long v = strtoull(text, &end, 10);
  out = uint32_t(v);
  return !*end && v <= 0xffffffffull;
}
} // namespace ledger_detail
// Parse one ledger line (with or without its newline). The header, torn lines, rows from the old
// detections.csv and rows naming a family this firmware does not know all return false.
inline bool ledgerParse(const char *line, LedgerEntry &out) {
  char copy[LedgerRowMax];
  size_t length = line ? strlen(line) : 0;
  while (length && (line[length - 1] == '\n' || line[length - 1] == '\r'))
    --length;
  if (!length || length >= sizeof(copy))
    return false;
  memcpy(copy, line, length);
  copy[length] = 0;
  char *f[14];
  if (ledger_detail::split(copy, f, 14) != 14)
    return false;
  LedgerEntry e;
  e.utc = utcFromText(f[0]);
  if (*f[0] && !e.utc)
    return false;
  long v;
  if (!ledger_detail::unsignedNumber(f[1], e.uptimeMs) ||
      !ledger_detail::unsignedNumber(f[2], e.boot))
    return false;
  e.family = familyFromKey(f[3]);
  if (e.family == Family::None)
    return false;
  for (unsigned c = 1; c <= 3; ++c)
    if (!strcmp(f[4], confidenceName(Confidence(c))))
      e.confidence = Confidence(c);
  if (e.confidence == Confidence::None)
    return false;
  if (!strcmp(f[5], "WIFI"))
    e.ble = false;
  else if (!strncmp(f[5], "BLE", 3) &&
           (!f[5][3] || (f[5][3] >= '1' && f[5][3] <= '9' && !f[5][4]))) {
    e.ble = true;
    e.addressType = f[5][3] ? uint8_t(f[5][3] - '0') : 0;
  } else
    return false;
  if (strlen(f[6]) != 17)
    return false;
  for (unsigned i = 0; i < 6; ++i) {
    const char *p = f[6] + i * 3;
    if (!isxdigit((unsigned char)p[0]) || !isxdigit((unsigned char)p[1]) || (i < 5 && p[2] != ':'))
      return false;
    char byte[] = {p[0], p[1], 0};
    e.mac[i] = uint8_t(strtoul(byte, nullptr, 16));
  }
  snprintf(e.name, sizeof(e.name), "%s", f[7]);
  snprintf(e.label, sizeof(e.label), "%s", f[8]);
  if (!ledger_detail::number(f[9], -128, 127, v))
    return false;
  e.rssi = int8_t(v);
  if (!ledger_detail::number(f[10], 0, 255, v))
    return false;
  e.channel = uint8_t(v);
  if (*f[11] || *f[12]) {
    char *endLat = nullptr, *endLon = nullptr;
    double lat = strtod(f[11], &endLat), lon = strtod(f[12], &endLon);
    if (!*f[11] || !*f[12] || *endLat || *endLon || !(lat >= -90 && lat <= 90) ||
        !(lon >= -180 && lon <= 180))
      return false;
    e.lat = float(lat);
    e.lon = float(lon);
  }
  if (strcmp(f[13], "0") && strcmp(f[13], "1"))
    return false;
  e.alerted = f[13][0] == '1';
  out = e;
  return true;
}

// ---- The table ------------------------------------------------------------------------------
struct ReconSighting {
  const uint8_t *mac = nullptr;
  bool ble = false;
  uint8_t addressType = 0;
  Match match;
  const char *name = "";  // Empty keeps the name already on the row.
  const char *label = ""; // Empty keeps the label already on the row.
  uint8_t channel = 0;
  int rssi = 0;
  uint32_t utc = 0; // Seconds since 2000, 0 if the time is unknown.
  bool fix = false; // lat/lon hold a usable GPS position.
  float lat = 0, lon = 0;
  uint32_t boot = 0;
};
enum class Sight : uint8_t {
  Dropped, // Table full of more important live rows; the sighting was not recorded.
  Seen,    // One more packet from a device already in its current encounter.
  New,     // First time this device has a row.
  Returned // Back after ReconAbsenceMs of silence, or first heard since a restore.
};
struct ReconSeen {
  ReconDevice *row = nullptr; // Null only for Sight::Dropped.
  Sight sight = Sight::Dropped;
  uint32_t gapMs = 0; // Silence before this packet; UINT32_MAX for a restored row.
};
enum class ReconSort : uint8_t {
  Priority, // Live first (the user's own ignored gear after the rest), then tier, then most
            // recent. Steady: see ReconTable::sorted().
  Recent    // Most recently heard first, whatever it is.
};
struct ReconCounts {
  uint16_t total = 0, live = 0, restored = 0, following = 0;
  uint16_t followingLive = 0; // Flagged as following and still in range.
  uint16_t ignored = 0;       // Rows on the ignore list (live or not).
  uint16_t liveTier[TierCount]{}, totalTier[TierCount]{};
  uint16_t watchTier[TierCount]{}; // Live and not ignored: what the pet should care about.
  uint16_t liveFamily[FamilyCount]{};
};
// How much the pet should mind what is in range (ignored devices do not count): 0 nothing,
// 1 only trackers or beacons, 2 surveillance, hacker gear or an attack.
inline unsigned watchLevel(const ReconCounts &c) {
  if (c.watchTier[unsigned(Tier::Surveillance)] || c.watchTier[unsigned(Tier::Hacker)] ||
      c.watchTier[unsigned(Tier::Attack)])
    return 2;
  return c.watchTier[unsigned(Tier::Tracker)] || c.watchTier[unsigned(Tier::Beacon)] ? 1 : 0;
}
class ReconTable {
public:
  // The caller owns the storage (PSRAM on the device, a plain array in tests); attach() takes it
  // over and empties it. Until attach() is called the table has no capacity and drops everything.
  void attach(ReconDevice *rows, unsigned capacity) {
    rows_ = rows;
    capacity_ = rows ? capacity : 0;
    clear();
  }
  void clear() {
    for (unsigned i = 0; i < capacity_; ++i)
      new (&rows_[i]) ReconDevice();
    used_ = restored_ = 0;
  }
  unsigned capacity() const {
    return capacity_;
  }
  unsigned size() const {
    return used_;
  }
  // Slot access by index (0..capacity-1), as handed out by sorted(). Check row.used.
  ReconDevice &at(unsigned index) {
    return rows_[index];
  }
  const ReconDevice &at(unsigned index) const {
    return rows_[index];
  }
  static bool live(const ReconDevice &row, uint32_t now) { // Heard within ReconLiveMs.
    return reconLive(row, now);
  }
  int indexOf(const ReconDevice *row) const {
    return row && row >= rows_ && row < rows_ + capacity_ ? int(row - rows_) : -1;
  }
  ReconDevice *find(const uint8_t *mac, bool ble, uint8_t addressType, Family family) {
    for (unsigned i = 0; i < capacity_; ++i) {
      auto &d = rows_[i];
      if (d.used && d.family == family && d.ble == ble && d.addressType == addressType &&
          !memcmp(d.mac, mac, 6))
        return &d;
    }
    return nullptr;
  }

  // Record one classified packet. Call it for every packet whose family the type filter tracks,
  // whatever the alert threshold is.
  ReconSeen observe(const ReconSighting &s, uint32_t now) {
    ReconSeen seen;
    if (!capacity_ || !s.mac || s.match.family == Family::None)
      return seen;
    int rssi = s.rssi < -127 ? -127 : s.rssi > 0 ? 0 : s.rssi;
    ReconDevice *row = find(s.mac, s.ble, s.addressType, s.match.family);
    if (row) {
      seen.gapMs = row->restored ? UINT32_MAX : uint32_t(now - row->lastAt);
      seen.sight = seen.gapMs >= ReconAbsenceMs ? Sight::Returned : Sight::Seen;
    } else {
      row = claim(tierOf(s.match.family), now);
      if (!row)
        return seen;
      *row = ReconDevice();
      row->used = true;
      ++used_;
      memcpy(row->mac, s.mac, 6);
      row->ble = s.ble;
      row->addressType = s.addressType;
      row->family = s.match.family;
      row->firstAt = now;
      row->peakRssi = int8_t(rssi);
      seen.sight = Sight::New;
      if (unsigned(row->family) < FamilyCount) {
        ++lifetime_[unsigned(row->family)];
        lifetimeDirty_ = true;
      }
    }
    if (seen.sight != Sight::Seen) {
      if (row->restored) {
        row->restored = false;
        --restored_;
        row->firstAt = now;
        row->peakRssi = int8_t(rssi);
      }
      ++row->encounters;
      row->arrivedAt = now;
      row->lat = row->lon = 0;      // Where it was last time says nothing about this encounter.
      row->prevRssi = int8_t(rssi); // No trend across a gap.
      row->trendAt = now;
    } else if (uint32_t(now - row->trendAt) >= ReconTrendMs) {
      row->prevRssi = row->rssi;
      row->trendAt = now;
    }
    row->rssi = int8_t(rssi);
    if (rssi > row->peakRssi)
      row->peakRssi = int8_t(rssi);
    row->lastAt = now;
    ++row->packets;
    row->channel = s.channel;
    row->boot = s.boot;
    if (s.match.confidence >= row->confidence) {
      row->confidence = s.match.confidence;
      row->evidence = s.match.evidence ? s.match.evidence : "";
    }
    if (s.name && *s.name)
      snprintf(row->name, sizeof(row->name), "%s", s.name);
    if (s.label && *s.label)
      snprintf(row->label, sizeof(row->label), "%s", s.label);
    if (s.utc)
      row->utc = s.utc;
    if (s.fix) {
      row->lat = s.lat;
      row->lon = s.lon;
      if (tierOf(row->family) == Tier::Tracker)
        followNote(*row, s.lat, s.lon, now);
    }
    seen.row = row;
    return seen;
  }

  // Load one ledger line as a row from an earlier boot. Feed lines oldest first; only the newest
  // `keep` distinct devices stay. A device already heard this boot is left alone, and history
  // never pushes out a row from this boot.
  bool restore(const LedgerEntry &e, unsigned keep = RestoreKeep) {
    if (!capacity_ || e.family == Family::None || !keep)
      return false;
    ReconDevice *row = find(e.mac, e.ble, e.addressType, e.family);
    if (row && !row->restored)
      return false;
    if (row) {
      // The alert that follows a first sighting is logged as a second line for the same
      // encounter (same boot and uptime); that is not another encounter.
      if (row->boot != e.boot || row->arrivedAt != e.uptimeMs)
        ++row->encounters;
    } else {
      if (restored_ >= keep) {
        for (unsigned i = 0; i < capacity_; ++i)
          if (rows_[i].used && rows_[i].restored && (!row || rows_[i].restoreSeq < row->restoreSeq))
            row = &rows_[i];
      } else
        for (unsigned i = 0; i < capacity_ && !row; ++i)
          if (!rows_[i].used)
            row = &rows_[i];
      if (!row)
        return false;
      if (!row->used) {
        ++used_;
        ++restored_;
      }
      *row = ReconDevice();
      row->used = row->restored = true;
      memcpy(row->mac, e.mac, 6);
      row->ble = e.ble;
      row->addressType = e.addressType;
      row->family = e.family;
      row->evidence = "From the detection ledger; not heard since this boot began";
      row->firstAt = e.uptimeMs;
      row->encounters = 1;
      row->peakRssi = e.rssi;
    }
    row->restoreSeq = ++restoreSeq_;
    row->arrivedAt = row->lastAt = e.uptimeMs;
    row->boot = e.boot;
    row->confidence = e.confidence > row->confidence ? e.confidence : row->confidence;
    row->rssi = row->prevRssi = e.rssi;
    if (e.rssi > row->peakRssi)
      row->peakRssi = e.rssi;
    row->channel = e.channel;
    row->alerted |= e.alerted;
    if (*e.name)
      snprintf(row->name, sizeof(row->name), "%s", e.name);
    if (*e.label)
      snprintf(row->label, sizeof(row->label), "%s", e.label);
    if (e.utc)
      row->utc = e.utc;
    if (e.lat != 0 || e.lon != 0) {
      row->lat = e.lat;
      row->lon = e.lon;
    }
    return true;
  }

  // Housekeeping; call at least every few days of uptime (the firmware does twice a second).
  // Saturates old ages so millis() rollover can never make a long-gone device look fresh.
  void maintain(uint32_t now) {
    for (unsigned i = 0; i < capacity_; ++i) {
      auto &d = rows_[i];
      if (!d.used || d.restored)
        continue;
      uint32_t oldest = now - ReconClampMs;
      if (uint32_t(now - d.lastAt) > ReconClampMs) {
        // Gone for days: its trail, allowance and "already told" state are all stale.
        d.lastAt = d.fixAt = d.trendAt = oldest;
        d.pointCount = 0;
        d.following = false;
        d.alerts = 0;
        d.notified = false;
      }
      for (uint32_t *t : {&d.firstAt, &d.arrivedAt, &d.announcedAt, &d.trendAt, &d.fixAt})
        if (uint32_t(now - *t) > ReconClampMs)
          *t = oldest;
      for (unsigned k = 0; k < d.pointCount; ++k)
        if (uint32_t(now - d.points[k].at) > ReconClampMs)
          d.points[k].at = oldest;
    }
  }

  // Fill `out` with slot indices for at() in display order and return how many were written. No
  // heap is used. Pass an array of capacity() entries for the whole list, or a shorter one for
  // just the top rows.
  //
  // Priority order is live rows first, then tier, then most recent; among live rows the ones on
  // the ignore list come last, so the user's own dashcam never outranks a stranger's gadget. To
  // keep on-screen rows
  // from swapping places with every packet, "most recent" for a live row means it arrived most
  // recently (start of its current encounter); for rows no longer live it is when they were last
  // heard. Restored rows come after rows from this boot, newest ledger entry first.
  unsigned sorted(uint16_t *out, unsigned capacity, uint32_t now, ReconSort order) const {
    unsigned n = 0;
    if (!capacity || !out)
      return 0;
    auto before = [&](uint16_t a, uint16_t b) { return comesBefore(a, b, now, order); };
    if (capacity >= used_) {
      for (unsigned i = 0; i < capacity_; ++i)
        if (rows_[i].used)
          out[n++] = uint16_t(i);
      std::sort(out, out + n, before);
      return n;
    }
    for (unsigned i = 0; i < capacity_; ++i) { // Short list: keep only the best `capacity`.
      if (!rows_[i].used)
        continue;
      if (n == capacity && !before(uint16_t(i), out[n - 1]))
        continue;
      unsigned at = n < capacity ? n++ : n - 1;
      for (; at > 0 && before(uint16_t(i), out[at - 1]); --at)
        out[at] = out[at - 1];
      out[at] = uint16_t(i);
    }
    return n;
  }
  void counts(ReconCounts &out, uint32_t now) const {
    out = ReconCounts();
    for (unsigned i = 0; i < capacity_; ++i) {
      const auto &d = rows_[i];
      if (!d.used)
        continue;
      unsigned tier = unsigned(tierOf(d.family));
      ++out.total;
      ++out.totalTier[tier];
      out.restored += d.restored;
      out.following += d.following;
      out.ignored += d.ignored;
      if (reconLive(d, now)) {
        ++out.live;
        ++out.liveTier[tier];
        out.followingLive += d.following;
        out.watchTier[tier] += !d.ignored;
        if (unsigned(d.family) < FamilyCount)
          ++out.liveFamily[unsigned(d.family)];
      }
    }
  }

  // Lifetime "devices ever seen" per family: one per new row, across reboots once the caller
  // persists the blob (FamilyCount little-endian 32-bit counters; shorter blobs from an older
  // firmware load fine).
  uint32_t lifetime(Family f) const {
    return unsigned(f) < FamilyCount ? lifetime_[unsigned(f)] : 0;
  }
  uint32_t lifetimeTotal() const {
    uint32_t sum = 0;
    for (unsigned i = 1; i < FamilyCount; ++i)
      sum += lifetime_[i];
    return sum;
  }
  static constexpr size_t LifetimeBytes = FamilyCount * 4;
  size_t saveLifetime(uint8_t *out, size_t capacity) const {
    if (capacity < LifetimeBytes)
      return 0;
    for (unsigned i = 0; i < FamilyCount; ++i)
      for (unsigned b = 0; b < 4; ++b)
        out[i * 4 + b] = uint8_t(lifetime_[i] >> (8 * b));
    return LifetimeBytes;
  }
  void loadLifetime(const uint8_t *in, size_t bytes) {
    for (unsigned i = 0; i < FamilyCount; ++i) {
      lifetime_[i] = 0;
      if (in && (i + 1) * 4 <= bytes)
        for (unsigned b = 0; b < 4; ++b)
          lifetime_[i] |= uint32_t(in[i * 4 + b]) << (8 * b);
    }
    lifetimeDirty_ = false;
  }
  void resetLifetime() {
    memset(lifetime_, 0, sizeof(lifetime_));
    lifetimeDirty_ = true;
  }
  // True once after the counters change, so the caller can save them at its own pace.
  bool takeLifetimeDirty() {
    bool dirty = lifetimeDirty_;
    lifetimeDirty_ = false;
    return dirty;
  }

  // True exactly once per device, when it has just qualified as following. The caller raises the
  // special alert.
  bool followDue(ReconDevice &row) {
    if (!row.following || row.followAlerted || row.ignored)
      return false;
    row.followAlerted = true;
    return true;
  }
  // Re-stamp every row after the ignore list changes or rows are restored.
  void applyIgnores(const IgnoreList &list) {
    for (unsigned i = 0; i < capacity_; ++i) {
      auto &d = rows_[i];
      if (!d.used)
        continue;
      d.ignored = list.contains(d.mac, d.family);
      if (d.ignored)
        d.following = false;
    }
  }
  // Drop rows of families the user has switched off. Returns how many went.
  unsigned prune(const TypeFilter &filter) {
    unsigned removed = 0;
    for (unsigned i = 0; i < capacity_; ++i)
      if (rows_[i].used && !filter.tracks(rows_[i].family)) {
        release(rows_[i]);
        ++removed;
      }
    return removed;
  }
  void remove(ReconDevice &row) {
    if (indexOf(&row) >= 0 && row.used)
      release(row);
  }

private:
  void release(ReconDevice &row) {
    restored_ -= row.restored;
    --used_;
    row = ReconDevice();
  }
  // A free slot, or the most expendable row: restored first, then stale, then the lowest tier,
  // then the longest unheard. A live row is never given up for a newcomer of a lower tier, so a
  // crowd of tags cannot push out a camera that is still in range.
  ReconDevice *claim(Tier newcomer, uint32_t now) {
    ReconDevice *victim = nullptr;
    unsigned victimRank = 0;
    uint32_t victimAge = 0;
    for (unsigned i = 0; i < capacity_; ++i) {
      auto &d = rows_[i];
      if (!d.used)
        return &d;
      bool live = reconLive(d, now);
      unsigned rank = (d.restored ? 64u : 0u) + (live ? 0u : 32u) + unsigned(tierOf(d.family));
      // Restored rows have no comparable clock; the oldest ledger entry goes first.
      uint32_t age = d.restored ? UINT32_MAX - d.restoreSeq : uint32_t(now - d.lastAt);
      if (!victim || rank > victimRank || (rank == victimRank && age > victimAge)) {
        victim = &d;
        victimRank = rank;
        victimAge = age;
      }
    }
    if (!victim || (reconLive(*victim, now) && tierOf(victim->family) < newcomer))
      return nullptr;
    release(*victim);
    return victim;
  }
  bool comesBefore(uint16_t ia, uint16_t ib, uint32_t now, ReconSort order) const {
    const auto &a = rows_[ia], &b = rows_[ib];
    bool liveA = reconLive(a, now), liveB = reconLive(b, now);
    if (order == ReconSort::Priority) {
      if (liveA != liveB)
        return liveA;
      if (liveA && a.ignored != b.ignored)
        return b.ignored;
      Tier tierA = tierOf(a.family), tierB = tierOf(b.family);
      if (tierA != tierB)
        return tierA < tierB;
    }
    if (a.restored != b.restored)
      return b.restored;
    if (a.restored) {
      if (a.restoreSeq != b.restoreSeq)
        return a.restoreSeq > b.restoreSeq;
      return ia < ib;
    }
    bool byArrival = order == ReconSort::Priority && liveA;
    uint32_t ageA = uint32_t(now - (byArrival ? a.arrivedAt : a.lastAt));
    uint32_t ageB = uint32_t(now - (byArrival ? b.arrivedAt : b.lastAt));
    if (ageA != ageB)
      return ageA < ageB;
    return ia < ib;
  }
  ReconDevice *rows_ = nullptr;
  unsigned capacity_ = 0, used_ = 0, restored_ = 0;
  uint32_t restoreSeq_ = 0;
  uint32_t lifetime_[FamilyCount]{};
  bool lifetimeDirty_ = false;
};

// Make a freshly read ledger tail safe to walk as text and say how it ends. A power cut can leave
// zero bytes inside the file; each becomes a line break, so the damaged line splits into pieces
// ledgerParse() rejects and the rows after it still load. Returns true when the tail does not end
// in a line break: the last row was torn, and the next row appended must start on a new line or
// it would be glued to the fragment and lost with it.
inline bool ledgerScrub(char *text, size_t length) {
  for (size_t i = 0; i < length; ++i)
    if (!text[i])
      text[i] = '\n';
  return length && text[length - 1] != '\n';
}
// Restore from the tail of the ledger file: `text` is the NUL-terminated end of the file (it is
// cut up in place; run ledgerScrub() over it first) and `midLine` says it does not start at the
// beginning of the file, so its first line is probably the back half of one. Lines of families the
// filter has switched off, torn lines and a last line that lost its newline to a power cut are
// skipped. Returns the number of lines taken.
inline unsigned ledgerLoad(ReconTable &table, char *text, bool midLine, const TypeFilter &filter,
                           unsigned keep = RestoreKeep) {
  unsigned loaded = 0;
  char *line = text;
  if (midLine) {
    line = strchr(text, '\n');
    if (!line)
      return 0;
    ++line;
  }
  for (char *end; (end = strchr(line, '\n')) != nullptr; line = end + 1) {
    *end = 0;
    LedgerEntry entry;
    if (ledgerParse(line, entry) && filter.tracks(entry.family) && table.restore(entry, keep))
      ++loaded;
  }
  return loaded;
}

// ---- Alert policy ---------------------------------------------------------------------------
// The shared timers: one alert of any kind per ten seconds, and a per-family quiet period for
// rotating addresses. A family of a higher tier than the alert that started the ten seconds is
// let through, so a tag alert can never swallow a camera that shows up right behind it.
class AlertCooldowns {
public:
  static constexpr uint32_t GlobalMs = 10000;
  bool familyReady(Family f, uint32_t now, uint32_t quietMs) const {
    return unsigned(f) >= FamilyCount || families_[unsigned(f)].ready(now, quietMs);
  }
  bool globalReady(Family f, uint32_t now) const {
    return global_.ready(now, GlobalMs) || tierOf(f) < lastTier_;
  }
  void mark(Family f, uint32_t now) {
    markAs(f, tierOf(f), now);
  }
  // For alerts that nothing should talk over (a follow warning): counts as the top tier.
  void markAs(Family f, Tier tier, uint32_t now) {
    global_.mark(now);
    lastTier_ = tier;
    if (unsigned(f) < FamilyCount)
      families_[unsigned(f)].mark(now);
  }

private:
  Cooldown global_, families_[FamilyCount];
  Tier lastTier_ = Tier::Beacon;
};
// Tracker floods. Spam gadgets (and the odd stadium crowd) produce dozens of brand-new tracker
// addresses a minute; each would otherwise be its own alert. Trip new addresses of one family
// inside WindowMs raise a single flood alert, and the family then stays quiet until QuietMs pass
// without any new address.
class FloodGate {
public:
  static constexpr unsigned Trip = 40;
  static constexpr uint32_t WindowMs = 60000, QuietMs = 300000;
  // Note one new address. True exactly once per flood: on the address that trips it.
  bool add(Family f, uint32_t now) {
    Slot *s = slot(f);
    if (!s)
      return false;
    settle(*s, now);
    s->at[s->head] = now;
    s->head = uint8_t((s->head + 1) % Trip);
    if (s->count < Trip)
      ++s->count;
    s->lastNew = now;
    // With the ring full, head points at the oldest of the last Trip arrivals.
    if (s->flooding || s->count < Trip || uint32_t(now - s->at[s->head]) > WindowMs)
      return false;
    s->flooding = true;
    return true;
  }
  bool active(Family f, uint32_t now) {
    Slot *s = slot(f);
    if (!s)
      return false;
    settle(*s, now);
    return s->flooding;
  }

private:
  struct Slot {
    uint32_t at[Trip]{}, lastNew = 0;
    uint8_t head = 0, count = 0;
    bool flooding = false;
  };
  static void settle(Slot &s, uint32_t now) {
    if (s.flooding && uint32_t(now - s.lastNew) >= QuietMs) {
      s.flooding = false;
      s.count = s.head = 0; // The next flood needs a full fresh burst.
    }
  }
  Slot *slot(Family f) {
    switch (f) {
    case Family::Tracker:
      return &slots_[0];
    case Family::SamsungTag:
      return &slots_[1];
    case Family::GoogleTag:
      return &slots_[2];
    case Family::Tile:
      return &slots_[3];
    default:
      return nullptr;
    }
  }
  Slot slots_[4];
};
// Auto snooze. A device gets `after` alerts; then it only alerts again by coming SnoozeMarginDb
// closer than its strongest alert so far (single readings wobble by about 5 dB, so less would be
// noise). SnoozeDecayMs after its last alert the allowance is handed back. Watchlist matches are
// exempt: the user asked for those by name.
inline uint8_t autoSnoozeChoice(unsigned value) { // The menu offers OFF / 5 / 10.
  return value >= 10 ? 10 : value >= 5 ? 5 : 0;
}
inline bool snoozeHolds(const ReconDevice &row, unsigned after, uint32_t now) {
  if (!after || row.family == Family::Watchlist || row.alerts < after ||
      uint32_t(now - row.announcedAt) > SnoozeDecayMs)
    return false;
  return int(row.rssi) < int(row.quietBar) + SnoozeMarginDb;
}
inline void snoozeSpend(ReconDevice &row, unsigned after, uint32_t now) {
  if (row.alerts && uint32_t(now - row.announcedAt) > SnoozeDecayMs)
    row.alerts = 0;
  if (!after || row.alerts < after) {
    // The bar is the strongest it has ever alerted at, not the latest.
    if (!row.alerts || row.rssi > row.quietBar)
      row.quietBar = row.rssi;
    if (row.alerts < 255)
      ++row.alerts;
  } else
    row.quietBar = row.rssi; // It beat the bar; this is the new one.
}
// The user's settings and the moment's circumstances, gathered by the caller for each decision.
struct AlertContext {
  TypeFilter filter;
  Confidence threshold = Confidence::Medium; // Alert filter: notify at this confidence and above.
  bool quietRoad = true;                     // Setting: tags and Ring stay list-only on the road.
  bool onRoad = false;                       // A drive is recording, or GPS says we are moving.
  uint8_t autoSnooze = 5;                    // 0 (off), 5 or 10 alerts per device.
  uint32_t familyQuietMs = 60000;
  bool repeat = false; // Remind every RenotifyMs while a device stays in range.
};
inline bool quietOnRoad(Family f) {
  return tierOf(f) == Tier::Tracker || f == Family::Ring;
}
class AlertPolicy {
public:
  IgnoreList ignores;
  // Decide whether this sighting may interrupt, and record the outcome on the row. Call once per
  // observe() that returned a row. On Alert the row's allowance and all cooldowns are already
  // spent; the caller only has to raise it. On Flood the caller raises one family-wide alert.
  AlertVerdict decide(ReconDevice &row, const ReconSeen &seen, const AlertContext &ctx,
                      uint32_t now) {
    AlertVerdict verdict = evaluate(row, seen, ctx, now);
    row.verdict = verdict;
    if (verdict == AlertVerdict::Alert) {
      snoozeSpend(row, ctx.autoSnooze, now);
      row.notified = row.alerted = true;
      row.notifiedConfidence = row.confidence;
      row.announcedAt = now;
      cooldowns_.mark(row.family, now);
    } else if (verdict == AlertVerdict::Flood)
      cooldowns_.mark(row.family, now);
    return verdict;
  }
  // A follow warning was raised for this row: nothing may talk over it for the global cooldown.
  void announceFollow(ReconDevice &row, uint32_t now) {
    row.alerted = true;
    cooldowns_.markAs(row.family, Tier::Surveillance, now);
  }
  bool flooding(Family f, uint32_t now) {
    return flood_.active(f, now);
  }

private:
  AlertVerdict evaluate(ReconDevice &row, const ReconSeen &seen, const AlertContext &ctx,
                        uint32_t now) {
    if (!ctx.filter.tracks(row.family))
      return AlertVerdict::FilteredType;
    row.ignored = ignores.contains(row.mac, row.family);
    if (row.ignored)
      row.following = false;
    if (seen.sight == Sight::New || seen.gapMs >= RenotifyMs)
      row.notified = false; // Gone long enough that coming back is news again.
    if (row.ignored)
      return AlertVerdict::Ignored;
    bool road = ctx.quietRoad && ctx.onRoad;
    if (tierOf(row.family) == Tier::Tracker) {
      if (seen.sight == Sight::New && flood_.add(row.family, now))
        return road ? AlertVerdict::FloodQuiet : AlertVerdict::Flood;
      if (flood_.active(row.family, now))
        return AlertVerdict::FloodQuiet;
    }
    if (row.confidence < ctx.threshold)
      return AlertVerdict::BelowThreshold;
    if (road && quietOnRoad(row.family))
      return AlertVerdict::QuietRoad;
    bool changed = !row.notified || row.confidence > row.notifiedConfidence;
    bool reminder = ctx.repeat && uint32_t(now - row.announcedAt) >= RenotifyMs;
    if (!changed && !reminder)
      return AlertVerdict::SameEncounter;
    if (snoozeHolds(row, ctx.autoSnooze, now))
      return AlertVerdict::Snoozed;
    if (!cooldowns_.familyReady(row.family, now, ctx.familyQuietMs))
      return AlertVerdict::FamilyCooldown;
    if (!cooldowns_.globalReady(row.family, now))
      return AlertVerdict::GlobalCooldown;
    return AlertVerdict::Alert;
  }
  AlertCooldowns cooldowns_;
  FloodGate flood_;
};
} // namespace dash
