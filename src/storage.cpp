#include "app.h"
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <TinyGPSPlus.h>
namespace dash {
static TinyGPSPlus gps;
static HardwareSerial gpsUart(2);
static File drive;
static uint32_t lastFlush = 0;
void gpsBegin() {
  gpsUart.end();
  gps = TinyGPSPlus();
  gpsUart.setRxBufferSize(2048);
  int rx = app.settings.gpsPort ? 1 : GpsRx, tx = app.settings.gpsPort ? 2 : GpsTx;
  gpsUart.begin(app.settings.gpsBaud, SERIAL_8N1, rx, tx);
  app.fix = {};
}
void gpsTick() {
  unsigned budget = 1024;
  while (gpsUart.available() && budget--)
    gps.encode(char(gpsUart.read()));
  app.gpsChars = gps.charsProcessed();
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
}
static void storageFail(const char *reason) {
  snprintf(app.storageError, sizeof(app.storageError), "%s", reason);
  app.session.stop();
  captureGeneration.store(0);
  if (drive)
    drive.close();
  app.sdReady = false;
  say(reason);
}
void storageBegin() {
  if (app.session.armed)
    return;
  SPI.begin(SdSck, SdMiso, SdMosi, SdCs);
  SD.end();
  app.sdReady = SD.begin(SdCs, SPI, 10000000) && SD.cardType() != CARD_NONE;
  if (app.sdReady)
    app.sdReady = (SD.exists("/wardrives") || SD.mkdir("/wardrives")) &&
                  (SD.exists("/events") || SD.mkdir("/events"));
  snprintf(app.storageError, sizeof(app.storageError), "%s",
           app.sdReady ? "" : "Insert FAT32 SD; tap Retry SD");
}
bool startDrive() {
  if (app.session.armed)
    return true;
  if (!app.radioReady) {
    say("Radio is not ready. Restart and check status.");
    return false;
  }
  if (!app.sdReady)
    storageBegin();
  if (!app.sdReady) {
    say(app.storageError);
    return false;
  }
  Preferences p;
  p.begin("dashchan", false);
  uint32_t seq = p.getUInt("session", 0);
  bool found = false;
  for (int i = 0; i < 1000; ++i) {
    ++seq;
    snprintf(app.logPath, sizeof(app.logPath), "/wardrives/dash-%08lu.csv", (unsigned long)seq);
    if (!SD.exists(app.logPath)) {
      found = true;
      break;
    }
  }
  if (!found) {
    p.end();
    storageFail("Cannot allocate a new log file");
    return false;
  }
  p.putUInt("session", seq);
  p.end();
  drive = SD.open(app.logPath, FILE_WRITE);
  const char *header = wigleHeader();
  if (!drive || drive.print(header) != strlen(header)) {
    storageFail("SD write failed. Drive stopped.");
    return false;
  }
  drive.flush();
  app.rows = app.skippedFix = 0;
  app.sessionAt = millis();
  app.session.start();
  captureGeneration.store(app.session.generation);
  lastFlush = millis();
  say(usableFix(app.fix) ? "Adventure time! Recording our discoveries."
                         : "Ready! Waiting for a good GPS fix.");
  return true;
}
void stopDrive() {
  app.session.stop();
  captureGeneration.store(0);
  if (drive) {
    drive.flush();
    drive.close();
  }
  saveSettings();
  say("Drive saved. I'm still keeping watch.");
}
void storageTick() {
  if (drive && uint32_t(millis() - lastFlush) >= 2000) {
    drive.flush();
    lastFlush = millis();
    if (drive.getWriteError())
      storageFail("SD error. Recording stopped.");
  }
}
void logObservation(Contact &c, const char *auth, uint32_t captureGen, uint32_t at) {
  if (!app.session.accepts(captureGen) || !drive)
    return;
  if (c.logGeneration == captureGen && !c.logged.ready(at, 30000))
    return;
  if (!usableFix(app.fix) || uint32_t(millis() - at) > 250) {
    ++app.skippedFix;
    return;
  }
  auto row = wigleRow(c.mac, c.name, auth, c.channel, c.rssi, c.ble, app.fix);
  if (row.empty())
    return;
  if (drive.write((const uint8_t *)row.data(), row.size()) != row.size()) {
    storageFail("SD full/write failed. Drive stopped.");
    return;
  }
  c.logGeneration = captureGen;
  c.logged.mark(at);
  ++app.rows;
  if (app.rows % 25 == 0) {
    ++app.settings.xp;
    app.energy = std::min(100, int(app.energy) + 1);
  }
}
void logAlert(const Alert &a) {
  if (!app.sdReady)
    return;
  const char *path = "/events/detections.csv";
  bool fresh = !SD.exists(path);
  File file = SD.open(path, FILE_APPEND);
  if (!file)
    return;
  if (fresh)
    file.println("UTC,UptimeMs,Family,Confidence,MAC,Name,RSSI,Evidence");
  std::string row = csv(usableFix(app.fix) ? app.fix.utc : "") + "," + std::to_string(a.at) + "," +
                    csv(familyName(a.match.family)) + "," +
                    csv(confidenceName(a.match.confidence)) + "," + csv(a.mac) + "," + csv(a.name) +
                    "," + std::to_string(a.rssi) + "," + csv(a.match.evidence) + "\n";
  if (file.write((const uint8_t *)row.data(), row.size()) != row.size())
    snprintf(app.storageError, sizeof(app.storageError), "Event log write failed");
  file.close();
}
} // namespace dash
