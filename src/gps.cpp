#include "app.h"
#include "travel.h"
#include <TinyGPSPlus.h>
namespace dash {
static TinyGPSPlus gps;
static HardwareSerial gpsUart(2);
static TravelState travel;
static NmeaFrame frame;
static GpsBaudSearch search;
GpsConfig gpsConfig() {
  return {app.settings.gpsProfile, app.settings.gpsPort, app.settings.gpsRx, app.settings.gpsBaud};
}
static void openGps(uint32_t baud) {
  gpsUart.end();
  gps = TinyGPSPlus();
  frame = {};
  travel = {};
  app.fix = {};
  app.moving = false;
  app.speedKph = -1;
  app.gpsChars = app.gpsGood = app.gpsBad = 0;
  app.gpsActiveBaud = baud;
  gpsUart.setRxBufferSize(4096);
  // RX-only after end() detaches previous pins. No configuration commands,
  // TX level, or bootstrapping signal is driven into the attached module.
  gpsUart.begin(baud, SERIAL_8N1, gpsRxPin(gpsConfig()), -1);
}
void gpsBegin() {
  search.cancel();
  app.gpsSearching = false;
  openGps(app.settings.gpsBaud);
  snprintf(app.gpsMessage, sizeof(app.gpsMessage), "Listening for NMEA at %lu",
           (unsigned long)app.settings.gpsBaud);
}
bool gpsApply(const GpsConfig &c) {
  if (app.session.armed || !validGpsConfig(c))
    return false;
  app.settings.gpsProfile = c.profile;
  app.settings.gpsPort = c.port;
  app.settings.gpsRx = c.rx;
  app.settings.gpsBaud = c.baud;
  gpsBegin();
  saveSettings();
  return true;
}
bool gpsFindBaud() {
  if (app.session.armed)
    return false;
  search.start(millis());
  app.gpsSearching = true;
  openGps(search.baud());
  snprintf(app.gpsMessage, sizeof(app.gpsMessage), "Finding baud; no fix needed");
  return true;
}
void gpsCancelSearch() {
  if (!app.gpsSearching)
    return;
  gpsBegin();
  snprintf(app.gpsMessage, sizeof(app.gpsMessage), "Search canceled; settings restored");
}
void gpsTick() {
  unsigned budget = 1024;
  while (gpsUart.available() && budget--) {
    ++app.gpsChars;
    if (!frame.feed(char(gpsUart.read())))
      continue;
    for (const char *p = frame.line(); *p; ++p)
      gps.encode(*p);
    if (search.sentence()) {
      app.settings.gpsBaud = search.baud();
      app.gpsSearching = false;
      saveSettings();
      snprintf(app.gpsMessage, sizeof(app.gpsMessage), "NMEA found: %lu baud saved",
               (unsigned long)app.settings.gpsBaud);
    } else if (!app.gpsSearching && !strncmp(app.gpsMessage, "Listening", 9))
      snprintf(app.gpsMessage, sizeof(app.gpsMessage), "NMEA received; waiting for fix");
  }
  app.gpsGood = frame.good;
  app.gpsBad = frame.bad;
  int next = search.tick(millis());
  if (next == 1)
    openGps(search.baud());
  else if (next == -1) {
    gpsBegin();
    snprintf(app.gpsMessage, sizeof(app.gpsMessage), "No NMEA found; old baud restored");
  }
  if (app.gpsSearching) {
    app.fix = {};
    return;
  }
  Fix f;
  f.age = gps.location.age();
  f.lat = gps.location.lat();
  f.lon = gps.location.lng();
  f.alt = gps.altitude.meters();
  f.sats = gps.satellites.value();
  f.hdop = gps.hdop.hdop();
  f.valid = gps.location.isValid() && gps.altitude.isValid() && gps.altitude.age() <= 2000 &&
            gps.hdop.isValid() && gps.hdop.age() <= 2000 && gps.satellites.isValid() &&
            gps.satellites.age() <= 2000 && gps.date.isValid() && gps.time.isValid() &&
            gps.date.age() <= 2000 && gps.time.age() <= 2000 && gps.date.year() >= 2024 &&
            gps.date.year() <= 2099;
  if (f.valid)
    snprintf(f.utc, sizeof(f.utc), "%04d-%02d-%02d %02d:%02d:%02d", gps.date.year(),
             gps.date.month(), gps.date.day(), gps.time.hour(), gps.time.minute(),
             gps.time.second());
  app.fix = f;
  bool fresh = usableFix(f) && gps.speed.isValid() && gps.speed.age() <= 2000;
  app.speedKph = fresh ? gps.speed.kmph() : -1;
  app.moving = travel.update(millis(), fresh, app.speedKph);
}
} // namespace dash
