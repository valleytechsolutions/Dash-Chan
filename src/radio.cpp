#include "app.h"
#include "detector.h"
#include <NimBLEDevice.h>
#include <algorithm>
#include <esp_wifi.h>
namespace dash {
struct Packet {
  uint32_t at, generation;
  uint16_t len;
  int8_t rssi;
  uint8_t channel, ble, addressType, mac[6];
  uint8_t data[768];
};
static QueueHandle_t packets;
static NimBLEScan *scan;
static std::atomic<uint32_t> lost{0};
static uint32_t phaseAt = 0, hopAt = 0;
static Cooldown globalAlert;
static void onWifi(void *buffer, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT)
    return;
  auto *p = static_cast<wifi_promiscuous_pkt_t *>(buffer);
  if (p->rx_ctrl.rx_state || p->rx_ctrl.sig_len < 40 ||
      (p->payload[0] != 0x80 && p->payload[0] != 0x50))
    return;
  size_t n = p->rx_ctrl.sig_len - 4;
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
  memcpy(mac, p.mac, 6);
  if (p.ble) {
    auto f = classifyBle(p.data, p.len, p.mac, p.addressType == 0);
    if (!f.valid) {
      ++app.malformed;
      return;
    }
    m = f.match;
    snprintf(name, sizeof(name), "%s", f.name);
  } else {
    auto f = classifyWifi(p.data, p.len);
    if (!f.valid) {
      ++app.malformed;
      return;
    }
    m = f.match;
    memcpy(mac, f.mac, 6);
    snprintf(name, sizeof(name), "%s", f.name);
    snprintf(auth, sizeof(auth), "%s", f.auth);
  }
  auto &c = findContact(mac, p.ble, p.addressType, p.at);
  c.seen = p.at;
  c.rssi = p.rssi;
  c.channel = p.channel;
  if (*name)
    snprintf(c.name, sizeof(c.name), "%s", name);
  // Classification persists for one encounter; do not retain an old identity indefinitely.
  if (m.confidence >= c.match.confidence)
    c.match = m;
  logObservation(c, auth, p.generation, p.at);
  if (c.match.family != Family::None && c.match.confidence >= app.settings.threshold &&
      c.alerted.ready(p.at, 60000) && globalAlert.ready(p.at, 9000)) {
    Alert a;
    a.match = c.match;
    a.at = p.at;
    a.rssi = c.rssi;
    snprintf(a.name, sizeof(a.name), "%s", c.name);
    snprintf(a.mac, sizeof(a.mac), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3],
             mac[4], mac[5]);
    c.alerted.mark(p.at);
    globalAlert.mark(p.at);
    buddyAlert(a);
  }
}
void radioBegin() {
  packets = xQueueCreate(24, sizeof(Packet));
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
  wifi_promiscuous_filter_t filter{WIFI_PROMIS_FILTER_MASK_MGMT};
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
  if (!NimBLEDevice::init("")) {
    say("BLE initialization failed");
    return;
  }
  scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(&callbacks, true);
  scan->setActiveScan(false);
  scan->setInterval(80);
  scan->setWindow(70);
  scan->setMaxResults(0);
  app.radioReady = esp_wifi_set_promiscuous(true) == ESP_OK;
  phaseAt = hopAt = millis();
}
void radioTick() {
  if (!app.radioReady)
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
    if (uint32_t(now - phaseAt) >= BleWindowMs) {
      scan->stop();
      app.blePhase = false;
      app.channel = 1;
      esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
      esp_wifi_set_promiscuous(true);
      hopAt = phaseAt = now;
    }
  } else if (uint32_t(now - hopAt) >= WifiDwellMs) {
    hopAt = now;
    if (app.channel < LastWifiChannel) {
      ++app.channel;
      esp_wifi_set_channel(app.channel, WIFI_SECOND_CHAN_NONE);
    } else {
      esp_wifi_set_promiscuous(false);
      app.blePhase = true;
      phaseAt = now;
      if (!scan->start(BleWindowMs, false, true)) {
        app.blePhase = false;
        app.channel = 1;
        esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
        esp_wifi_set_promiscuous(true);
        say("BLE scan retry on next cycle");
      }
    }
  }
}
} // namespace dash
