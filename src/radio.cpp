#include "app.h"
#include "detector.h"
#include "encounters.h"
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
static Encounters encounters;
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
static void remember(const Alert &a) {
  for (unsigned i = 0; i < app.historyCount; ++i) {
    auto &old = app.history[i];
    if (old.ble == a.ble && old.addressType == a.addressType &&
        old.match.family == a.match.family && !strcmp(old.mac, a.mac)) {
      old.at = a.at;
      old.rssi = a.rssi;
      ++old.sightings;
      if (a.match.confidence >= old.match.confidence)
        old.match = a.match;
      if (*a.name)
        snprintf(old.name, sizeof(old.name), "%s", a.name);
      Alert updated = old;
      for (unsigned j = i; j > 0; --j)
        app.history[j] = app.history[j - 1];
      app.history[0] = updated;
      return;
    }
  }
  for (size_t i = HistoryCapacity - 1; i > 0; --i)
    app.history[i] = app.history[i - 1];
  app.history[0] = a;
  if (app.historyCount < HistoryCapacity)
    ++app.historyCount;
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
  if (c.match.family != Family::None && c.match.confidence >= app.settings.threshold) {
    Alert a;
    a.match = c.match;
    a.at = p.at;
    a.firstAt = p.at;
    a.ble = p.ble;
    a.addressType = p.addressType;
    a.rssi = c.rssi;
    snprintf(a.name, sizeof(a.name), "%s", c.name);
    snprintf(a.mac, sizeof(a.mac), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3],
             mac[4], mac[5]);
    remember(a);
    auto &entry = encounters.observe(mac, p.ble, p.addressType, c.match.family, p.at, 300000);
    if (encounters.eligible(entry, c.match.confidence, p.at, app.settings.familyQuietMs,
                            app.settings.repeatAlerts)) {
      encounters.announce(entry, c.match.confidence, p.at);
      buddyAlert(a);
    } else
      ++app.suppressed;
  }
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
  if (!app.radioReady || paused)
    return;
  uint32_t now = millis();
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
