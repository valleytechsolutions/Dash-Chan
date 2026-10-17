#include "app.h"
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <TinyGPSPlus.h>
#include <ff.h>
#include <vfs_api.h>
namespace dash {
// Own the SD instance so formatting always uses this card's assigned drive,
// never a guessed FatFs drive number or another filesystem.
class DashSD : public fs::SDFS {
public:
  DashSD() : SDFS(fs::FSImplPtr(new VFSImpl())) {}
  bool formatFat32() {
    if (_pdrv == 0xff)
      return false;
    char volume[] = {char('0' + _pdrv), ':', 0};
    FATFS *filesystem = nullptr;
    DWORD freeClusters;
    if (f_getfree(volume, &freeClusters, &filesystem) != FR_OK)
      return false;
    uint8_t work[FF_MAX_SS];
    const MKFS_PARM options = {FM_FAT32, 0, 0, 0, 0};
    if (f_mount(nullptr, volume, 0) != FR_OK)
      return false;
    FRESULT formatted = f_mkfs(volume, &options, work, sizeof(work));
    FRESULT mounted = f_mount(filesystem, volume, 1);
    Serial.printf("SD format result=%u remount=%u\n", unsigned(formatted), unsigned(mounted));
    return formatted == FR_OK && mounted == FR_OK;
  }
};
static DashSD card;
static TinyGPSPlus gps;
static HardwareSerial gpsUart(2);
static File drive;
static uint32_t lastFlush = 0;
uint64_t storageCardSize() {
  return card.cardSize();
}
bool storageFormat(uint64_t expectedBytes) {
  if (app.session.armed || !app.sdReady || !expectedBytes || expectedBytes != card.cardSize()) {
    Serial.println("FORMAT refused: stop recording and supply the mounted card's exact size");
    return false;
  }
  app.sdReady = false;
  bool ok = card.formatFat32();
  // FatFs has already remounted this volume. Reinitializing the card here is
  // unnecessary and can leave older cards unresponsive until power-cycled.
  if (ok)
    app.sdReady = (card.exists("/wardrives") || card.mkdir("/wardrives")) &&
                  (card.exists("/events") || card.mkdir("/events"));
  if (!ok || !app.sdReady) {
    say("SD format failed. Check card status.");
    Serial.println("FORMAT failed");
    return false;
  }
  app.rows = app.skippedFix = 0;
  app.storageError[0] = 0;
  app.logPath[0] = 0;
  say("Fresh card! Ready for our next adventure.");
  Serial.println("FORMAT OK: FAT32, wardrives and events directories ready");
  return true;
}
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
  app.gpsGood = gps.passedChecksum();
  app.gpsBad = gps.failedChecksum();
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
  card.end();
  app.sdReady = card.begin(SdCs, SPI, 4000000) && card.cardType() != CARD_NONE;
  if (!app.sdReady) {
    card.end();
    delay(50);
    app.sdReady = card.begin(SdCs, SPI, 1000000) && card.cardType() != CARD_NONE;
  }
  if (app.sdReady)
    app.sdReady = (card.exists("/wardrives") || card.mkdir("/wardrives")) &&
                  (card.exists("/events") || card.mkdir("/events"));
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
    if (!card.exists(app.logPath)) {
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
  drive = card.open(app.logPath, FILE_WRITE);
  const char *header = wigleHeader();
  if (!drive || drive.print(header) != strlen(header)) {
    storageFail("SD write failed. Drive stopped.");
    return false;
  }
  drive.flush();
  app.rows = app.skippedFix = 0;
  app.food.reset();
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
void storageCheckLog() {
  if (!app.sdReady || app.session.armed || !*app.logPath) {
    Serial.println("LOGCHECK unavailable: stop a recorded session first");
    return;
  }
  File file = card.open(app.logPath, FILE_READ);
  if (!file) {
    Serial.println("LOGCHECK failed: cannot reopen file");
    return;
  }
  size_t bytes = file.size();
  bool matches = true;
  for (const char *p = wigleHeader(); *p; ++p)
    if (file.read() != *p)
      matches = false;
  file.close();
  Serial.printf("LOGCHECK header=%s bytes=%u rows=%lu\n", matches ? "OK" : "BAD", unsigned(bytes),
                (unsigned long)app.rows);
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
  // Only a fresh Wi-Fi network actually written with a valid fix can feed us.
  if (!c.ble)
    buddyDiscovery(c.mac);
}
void logAlert(const Alert &a) {
  if (!app.sdReady)
    return;
  const char *path = "/events/detections.csv";
  bool fresh = !card.exists(path);
  File file = card.open(path, FILE_APPEND);
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
