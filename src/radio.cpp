#include "app.h"
#include "detector.h"
#include "traffic.h"
#include <NimBLEDevice.h>
#include <algorithm>
#include <esp_heap_caps.h>
#include <esp_wifi.h>
namespace dash {
struct Packet {
  uint32_t at, generation;
  uint16_t len;
  int8_t rssi;
  uint8_t channel, ble, addressType, mac[6];
  uint8_t data[1024];
};
static QueueHandle_t packets;
static StaticQueue_t packetControl;
static NimBLEScan *scan;
static std::atomic<uint32_t> lost{0};
static uint32_t phaseAt = 0, hopAt = 0;
static ReconTable table;
static AlertPolicy policy;
static ReconCounts counts;
static bool tableTried = false;
static uint32_t refreshedAt = 0, clockBase = 0, clockAt = 0, lifeSeen = 0, countersAt = 0,
                filterSeen = TypeFilter::Defaults;
static bool countersDirty = false, clockPrimed = false;
static TrafficHints traffic;
static unsigned hopIndex = 0;
static constexpr uint8_t channelOrder[] = {1, 6, 11, 2, 7, 3, 8, 4, 9, 5, 10};
static uint32_t bleWindow = BleWatchMs;
static std::atomic<bool> paused{false};
static bool beginBle();
void radioPause(bool pause) {
  if (!app.radioReady || pause == paused)
    return;
  paused = pause;
  if (pause) {
    esp_wifi_set_promiscuous(false);
    if (scan)
      scan->stop();
    xQueueReset(packets);
  } else {
    xQueueReset(packets);
    app.blePhase = false;
    app.channel = 1;
    hopIndex = 0;
    phaseAt = hopAt = millis();
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    if (esp_wifi_set_promiscuous(true) != ESP_OK) {
      app.radioReady = false;
      say("Radio restart failed. Restart Dash-Chan.");
    }
  }
}
static bool addressCandidate(const uint8_t *mac) {
  return dragon::addressHint(mac, true, {dragon::Family::Wifi, dragon::Confidence::None, ""})
             .confidence != dragon::Confidence::None;
}
static void onWifi(void *buffer, wifi_promiscuous_pkt_type_t type) {
  if (paused || (type != WIFI_PKT_MGMT && type != WIFI_PKT_DATA))
    return;
  auto *p = static_cast<wifi_promiscuous_pkt_t *>(buffer);
  if (p->rx_ctrl.rx_state || p->rx_ctrl.sig_len < 28)
    return;
  size_t n = p->rx_ctrl.sig_len - 4;
  if (type == WIFI_PKT_DATA) {
    if (!addressCandidate(p->payload + 4) && !addressCandidate(p->payload + 10))
      return;
    n = 24; // Only addresses: never queue a data-frame payload.
  } else {
    uint8_t subtype = p->payload[0] & 0xfc;
    if (subtype != 0x80 && subtype != 0x50 && subtype != 0x40 && subtype != 0xd0 &&
        subtype != 0xc0 && subtype != 0xa0)
      return;
  }
  if (n > sizeof(Packet::data)) {
    ++lost;
    return;
  }
  Packet o{};
  o.at = millis();
  o.generation = captureGeneration.load();
  o.len = n;
  o.rssi = p->rx_ctrl.rssi;
  o.channel = p->rx_ctrl.channel;
  memcpy(o.data, p->payload, n);
  if (xQueueSend(packets, &o, 0) != pdTRUE)
    ++lost;
}
class Callbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice *d) override {
    if (paused)
      return;
    const auto &payload = d->getPayload();
    if (payload.size() > sizeof(Packet::data)) {
      ++lost;
      return;
    }
    Packet o{};
    o.at = millis();
    o.generation = captureGeneration.load();
    o.ble = 1;
    o.rssi = d->getRSSI();
    o.len = payload.size();
    auto address = d->getAddress();
    o.addressType = address.getType();
    for (int i = 0; i < 6; ++i)
      o.mac[i] = address.getBase()->val[5 - i];
    memcpy(o.data, payload.data(), o.len);
    if (xQueueSend(packets, &o, 0) != pdTRUE)
      ++lost;
  }
};
static Callbacks callbacks;
ReconTable &reconTable() {
  if (!tableTried) {
    tableTried = true;
    // About 50 KB: PSRAM when it is there, otherwise a smaller table from internal RAM.
    unsigned rows = ReconCapacity;
    auto memory = static_cast<ReconDevice *>(
        heap_caps_malloc(rows * sizeof(ReconDevice), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!memory) {
      rows = 64;
      memory = static_cast<ReconDevice *>(malloc(rows * sizeof(ReconDevice)));
    }
    table.attach(memory, memory ? rows : 0);
  }
  return table;
}
AlertPolicy &reconPolicy() {
  return policy;
}
const ReconCounts &reconCounts() {
  return counts;
}
AlertContext reconContext() {
  AlertContext ctx;
  ctx.filter.mask = app.settings.typeFilter & TypeFilter::All;
  ctx.threshold = app.settings.threshold;
  ctx.quietRoad = app.settings.quietRoad;
  ctx.onRoad = app.session.armed || app.moving;
  ctx.autoSnooze = app.settings.autoSnooze;
  ctx.familyQuietMs = app.settings.familyQuietMs;
  ctx.repeat = app.settings.repeatAlerts;
  return ctx;
}
uint32_t reconClock() {
  return clockBase ? clockBase + uint32_t(millis() - clockAt) / 1000 : 0;
}
// GPS is the clock. Without a fix, the pet's own time-keeping (RTC, last fix) shows up as a
// change to life.lastSeen, which is exact at the moment it changes.
static void learnClock(uint32_t now) {
  if (!clockPrimed) { // What flash remembered is the last shutdown, not the time now.
    clockPrimed = true;
    lifeSeen = app.life.lastSeen;
  }
  uint32_t seconds = usableFix(app.fix) ? utcFromText(app.fix.utc) : 0;
  if (app.life.lastSeen != lifeSeen) {
    lifeSeen = app.life.lastSeen;
    if (!seconds)
      seconds = lifeSeen;
  }
  if (seconds) {
    clockBase = seconds;
    clockAt = now;
  }
}
static Alert toAlert(const ReconDevice &d, uint32_t now) {
  Alert a;
  a.match = {d.family, d.confidence, d.evidence};
  snprintf(a.name, sizeof(a.name), "%s", d.name);
  macText(a.mac, d.mac);
  a.rssi = d.rssi;
  a.at = d.lastAt;
  a.firstAt = d.firstAt;
  a.sightings = d.encounters;
  a.ble = d.ble;
  a.addressType = d.addressType;
  if (d.restored) {
    // Screens age rows against millis(); give history its real age when the clock allows.
    uint32_t clock = reconClock(), age = INT32_MAX;
    if (clock && d.utc && clock >= d.utc && clock - d.utc < INT32_MAX / 1000)
      age = (clock - d.utc) * 1000;
    a.at = a.firstAt = now - age;
  }
  return a;
}
void reconRefresh() {
  uint32_t now = millis();
  refreshedAt = now;
  auto &t = reconTable();
  learnClock(now);
  uint32_t filter = app.settings.typeFilter & TypeFilter::All;
  if (filter != filterSeen) { // A family was switched off: its rows leave the list.
    filterSeen = filter;
    TypeFilter f;
    f.mask = filter;
    t.prune(f);
  }
  t.maintain(now);
  t.counts(counts, now);
  // Rows keep their place between refreshes, so a tap lands on the row that was drawn.
  uint16_t order[HistoryCapacity];
  unsigned n = t.sorted(order, HistoryCapacity, now, ReconSort::Priority);
  for (unsigned i = 0; i < n; ++i)
    app.history[i] = toAlert(t.at(order[i]), now);
  app.historyCount = n;
  // Lifetime counters reach flash at most once a minute.
  countersDirty |= t.takeLifetimeDirty();
  if (countersDirty && uint32_t(now - countersAt) >= 60000) {
    countersAt = now;
    countersDirty = false;
    reconSave();
  }
}
bool reconIgnore(ReconDevice &row, bool ignore) {
  bool changed =
      ignore ? policy.ignores.add(row.mac, row.family) : policy.ignores.remove(row.mac, row.family);
  reconTable().applyIgnores(policy.ignores);
  if (changed)
    reconSave();
  reconRefresh();
  return changed || row.ignored == ignore;
}
// List the device and decide whether it may interrupt. Listing never depends on the alert
// threshold; only a family the user has switched off is left out.
static void recon(const Contact &c, const uint8_t *mac, const Packet &p) {
  AlertContext ctx = reconContext();
  if (!ctx.filter.tracks(c.match.family))
    return;
  ReconSighting s;
  s.mac = mac;
  s.ble = p.ble;
  s.addressType = p.addressType;
  s.match = c.match;
  s.name = c.name;
  s.channel = p.ble ? 0 : c.channel;
  s.rssi = c.rssi;
  s.utc = reconClock();
  s.boot = app.life.boots;
  if (usableFix(app.fix)) {
    s.fix = true;
    s.lat = float(app.fix.lat);
    s.lon = float(app.fix.lon);
  }
  ReconSeen seen = reconTable().observe(s, p.at);
  if (!seen.row)
    return;
  ReconDevice &row = *seen.row;
  AlertVerdict verdict = policy.decide(row, seen, ctx, p.at);
  // First sight and each return go to the ledger, except the throwaway addresses of a flood.
  if (seen.sight != Sight::Seen && !policy.flooding(row.family, p.at))
    row.pendingLog = true;
  if (table.followDue(row)) {
    policy.announceFollow(row, p.at);
    snprintf(row.label, sizeof(row.label), "following");
    row.pendingLog = true;
    Alert a = toAlert(row, p.at);
    a.kind = AlertFollowing;
    a.match.evidence = "Heard at 3+ places over 10+ minutes; it may be travelling with you";
    buddyAlert(a);
  } else if (verdict == AlertVerdict::Alert) {
    row.pendingLog = true; // The ledger line for this encounter should say it alerted.
    buddyAlert(toAlert(row, p.at));
  } else if (verdict == AlertVerdict::Flood) {
    snprintf(row.label, sizeof(row.label), "flood");
    row.alerted = row.pendingLog = true;
    Alert a = toAlert(row, p.at);
    a.kind = AlertFlood;
    snprintf(a.name, sizeof(a.name), "Many new addresses at once");
    a.match.evidence = "40+ new addresses of this kind in a minute; a spam gadget or a crowd";
    buddyAlert(a);
  } else if (row.confidence >= ctx.threshold)
    ++app.suppressed;
}
static Contact &findContact(const uint8_t *mac, bool ble, uint8_t type, uint32_t now) {
  Contact *empty = nullptr;
  Contact *oldest = &contacts[0];
  for (auto &c : contacts) {
    if (c.used && c.ble == ble && c.addressType == type && !memcmp(c.mac, mac, 6))
      return c;
    if (!c.used && !empty)
      empty = &c;
    if (uint32_t(now - c.seen) > uint32_t(now - oldest->seen))
      oldest = &c;
  }
  auto &c = *(empty ? empty : oldest);
  c = {};
  c.used = true;
  c.ble = ble;
  c.addressType = type;
  memcpy(c.mac, mac, 6);
  if (ble)
    ++app.bleSeen;
  else
    ++app.wifiSeen;
  return c;
}
static void process(const Packet &p) {
  Match m;
  char name[64]{}, auth[64] = "[BLE]";
  uint8_t mac[6];
  bool loggable = p.ble;
  memcpy(mac, p.mac, 6);
  if (p.ble) {
    auto f = classifyBle(p.data, p.len, p.mac, p.addressType == 0);
    if (!f.valid) {
      ++app.malformed;
      return;
    }
    m = f.match;
    promote(m, traffic.ble(p.data, p.len, p.mac, p.at));
    snprintf(name, sizeof(name), "%s", f.name);
  } else {
    auto f = classifyWifi(p.data, p.len);
    if (!f.valid) {
      ++app.malformed;
      return;
    }
    m = f.match;
    loggable = f.loggable;
    promote(m, traffic.wifi(p.data, p.len, p.at));
    memcpy(mac, f.mac, 6);
    snprintf(name, sizeof(name), "%s", f.name);
    snprintf(auth, sizeof(auth), "%s", f.auth);
  }
  promote(m, watchlist.match(mac, !p.ble || p.addressType == 0, name, p.ble ? p.data : nullptr,
                             p.ble ? p.len : 0));
  // Unknown probe/data senders need no long-lived contact or WiGLE row.
  if (!loggable && m.family == Family::None)
    return;
  auto &c = findContact(mac, p.ble, p.addressType, p.at);
  c.seen = p.at;
  c.rssi = p.rssi;
  c.channel = p.channel;
  if (*name)
    snprintf(c.name, sizeof(c.name), "%s", name);
  // Classification persists for one encounter; do not retain an old identity indefinitely.
  if (m.confidence >= c.match.confidence)
    c.match = m;
  if (loggable)
    logObservation(c, auth, p.generation, p.at);
  if (c.match.family != Family::None)
    recon(c, mac, p);
}
static bool beginBle() {
  if (!NimBLEDevice::init("")) {
    say("BLE initialization failed");
    return false;
  }
  scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(&callbacks, true);
  scan->setActiveScan(false);
  scan->setInterval(80);
  scan->setWindow(70);
  scan->setMaxResults(0);
  return true;
}
void radioBegin() {
  reconRefresh();
  auto memory = static_cast<uint8_t *>(
      heap_caps_malloc(32 * sizeof(Packet), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  packets = memory ? xQueueCreateStatic(32, sizeof(Packet), memory, &packetControl)
                   : xQueueCreate(32, sizeof(Packet));
  if (!packets) {
    say("Radio queue allocation failed");
    return;
  }
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_err_t err = esp_wifi_init(&cfg);
  if (err == ESP_OK)
    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
  if (err == ESP_OK)
    err = esp_wifi_set_mode(WIFI_MODE_NULL);
  if (err == ESP_OK)
    err = esp_wifi_start();
  wifi_country_t country = {"US", 1, LastWifiChannel, 20, WIFI_COUNTRY_POLICY_MANUAL};
  if (err == ESP_OK)
    err = esp_wifi_set_country(&country);
  wifi_promiscuous_filter_t filter{WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA};
  if (err == ESP_OK)
    err = esp_wifi_set_promiscuous_filter(&filter);
  if (err == ESP_OK)
    err = esp_wifi_set_promiscuous_rx_cb(onWifi);
  if (err == ESP_OK)
    err = esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  if (err != ESP_OK) {
    say("Wi-Fi failed; see serial diagnostics");
    Serial.printf("RADIO error %s\n", esp_err_to_name(err));
    return;
  }
  if (!beginBle())
    return;
  app.radioReady = esp_wifi_set_promiscuous(true) == ESP_OK;
  phaseAt = hopAt = millis();
}
void radioTick() {
  uint32_t now = millis();
  // History from the card is worth showing even while the radios are paused or failed.
  if (uint32_t(now - refreshedAt) >= 500)
    reconRefresh();
  if (!app.radioReady || paused)
    return;
  Packet p;
  for (unsigned i = 0; i < 8 && xQueueReceive(packets, &p, 0) == pdTRUE; ++i) {
    process(p);
    gpsTick();
  }
  app.drops = lost.load();
  for (auto &c : contacts)
    if (c.used && uint32_t(now - c.seen) > 120000)
      c.used = false;
  if (app.blePhase) {
    if (uint32_t(now - phaseAt) >= bleWindow) {
      scan->stop();
      app.blePhase = false;
      app.channel = 1;
      hopIndex = 0;
      esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
      esp_wifi_set_promiscuous(true);
      hopAt = phaseAt = now;
    }
  } else if (uint32_t(now - hopAt) >= (app.session.armed ? WifiDwellDriveMs : WifiDwellWatchMs)) {
    hopAt = now;
    if (++hopIndex < sizeof(channelOrder)) {
      app.channel = channelOrder[hopIndex];
      esp_wifi_set_channel(app.channel, WIFI_SECOND_CHAN_NONE);
    } else {
      esp_wifi_set_promiscuous(false);
      app.blePhase = true;
      phaseAt = now;
      bleWindow = app.session.armed ? BleDriveMs : BleWatchMs;
      if (!scan->start(bleWindow, false, true)) {
        app.blePhase = false;
        app.channel = 1;
        hopIndex = 0;
        esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
        esp_wifi_set_promiscuous(true);
        say("BLE scan retry on next cycle");
      }
    }
  }
}
} // namespace dash
