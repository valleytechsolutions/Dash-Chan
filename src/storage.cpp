#include "app.h"
#include "uploads.h"
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <diskio.h>
#include <esp_heap_caps.h>
#include <ff.h>
#include <sd_diskio.h>
#include <vfs_api.h>
namespace dash {
// Own the SD instance so formatting always uses this card's assigned drive,
// never a guessed FatFs drive number or another filesystem.
class DashSD : public fs::SDFS {
public:
  DashSD() : SDFS(fs::FSImplPtr(new VFSImpl())) {}
  bool formatBlank(uint64_t expectedBytes) {
    end();
    SPI.begin(SdSck, SdMiso, SdMosi, SdCs);
    _pdrv = sdcard_init(SdCs, &SPI, 4000000);
    if (_pdrv == 0xff)
      return false;
    DSTATUS init = disk_initialize(_pdrv);
    uint64_t capacity = (init & STA_NOINIT) ? 0 : uint64_t(sdcard_refresh_sectors(_pdrv)) * 512;
    snprintf(app.storageError, sizeof(app.storageError), "Raw init %u; bytes %llu", unsigned(init),
             (unsigned long long)capacity);
    Serial.printf("SD raw init=%u bytes=%llu expected=%llu\n", unsigned(init),
                  (unsigned long long)capacity, (unsigned long long)expectedBytes);
    if ((init & STA_NOINIT) || capacity != expectedBytes) {
      Serial.println("SD raw format refused: card initialization/capacity mismatch");
      end();
      return false;
    }
    char volume[] = {char('0' + _pdrv), ':', 0};
    uint8_t work[FF_MAX_SS];
    const MKFS_PARM options = {FM_FAT32, 0, 0, 0, 0};
    FRESULT result = f_mkfs(volume, &options, work, sizeof(work));
    snprintf(app.storageError, sizeof(app.storageError), "Raw FAT32 result %u", unsigned(result));
    Serial.printf("SD raw format result=%u\n", unsigned(result));
    if (result == FR_OK && sdcard_mount(_pdrv, "/sd", 5, false)) {
      _impl->mountpoint("/sd");
      return true;
    }
    if (result == FR_OK)
      snprintf(app.storageError, sizeof(app.storageError), "Formatted; remount failed");
    end();
    return false;
  }
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
static File drive;
File storageOpen(const char *path, const char *mode) {
  return card.open(path, mode);
}
bool storageExists(const char *path) {
  return app.sdReady && card.exists(path);
}
bool storageRemove(const char *path) {
  return card.remove(path);
}
bool storageMakeDir(const char *path) {
  return card.exists(path) || card.mkdir(path);
}
bool storageRename(const char *from, const char *to) {
  return card.rename(from, to);
}
static uint32_t lastFlush = 0;
// ---- Detection ledger: /events/recon.csv (format in docs/DEVELOPMENT.md) ----------------------
static uint32_t ledgerAt = 0, ledgerWait = 5000;
static bool ledgerSoon = false, ledgerLoaded = false;
// The file may end in a partial row: one torn by a power cut (seen when the tail is read back) or
// cut short by a failed write. The next row appended starts on a new line, or it would be glued
// to the fragment and lost with it.
static bool ledgerTorn = false;
// Bring back the most recent devices from the end of the ledger so the list is not empty after
// a restart. Once per boot, at the first successful mount; only the file's tail is read.
static void ledgerRestore() {
  if (ledgerLoaded || !app.sdReady || uploadBusy())
    return;
  ledgerLoaded = true;
  File file = card.open(LedgerPath, FILE_READ);
  if (!file)
    return;
  size_t size = file.size(), want = std::min(size, size_t(LedgerTailBytes));
  auto text = static_cast<char *>(heap_caps_malloc(want + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!text)
    text = static_cast<char *>(malloc(want + 1));
  size_t got = 0;
  if (text && file.seek(size - want))
    got = file.read(reinterpret_cast<uint8_t *>(text), want);
  file.close();
  if (!text)
    return;
  text[got] = 0;
  ledgerTorn |= ledgerScrub(text, got); // Before the text is cut up into lines.
  TypeFilter filter;
  filter.mask = app.settings.typeFilter;
  auto &table = reconTable();
  unsigned loaded = ledgerLoad(table, text, size > want, filter);
  free(text);
  table.applyIgnores(reconPolicy().ignores);
  reconRefresh();
  Serial.printf("LEDGER restored %u lines from the last %u bytes\n", loaded, unsigned(got));
}
// Append rows the radio has marked: first sight, each return after an absence, and alerts.
// Batched every few seconds so the card is not opened per packet; rows simply stay marked while
// there is no card or an upload owns it.
static void ledgerTick() {
  uint32_t now = millis();
  // For the Recon page and STATUS: no card, taking rows, or the last write failed.
  if (!app.sdReady)
    app.ledger = LedgerState::NoCard;
  else if (app.ledger == LedgerState::NoCard)
    app.ledger = LedgerState::Ready;
  if (uint32_t(now - ledgerAt) < (ledgerSoon ? std::min<uint32_t>(ledgerWait, 1000) : ledgerWait))
    return;
  ledgerAt = now;
  if (!app.sdReady || uploadBusy())
    return;
  ledgerSoon = false;
  ledgerWait = 5000;
  bool failed = false;
  auto &table = reconTable();
  File file;
  unsigned written = 0;
  for (unsigned i = 0; i < table.capacity() && written < 16; ++i) {
    auto &d = table.at(i);
    if (!d.used || !d.pendingLog)
      continue;
    if (!file) {
      bool fresh = !card.exists(LedgerPath);
      file = card.open(LedgerPath, FILE_APPEND);
      if (!file) {
        failed = true;
        break;
      }
      // An empty file left by a failed first attempt still needs its header.
      fresh = fresh || file.size() == 0;
      if (fresh) {
        ledgerTorn = false; // Nothing to be glued to.
        if (file.print(ledgerHeader()) != strlen(ledgerHeader())) {
          failed = ledgerTorn = true;
          break;
        }
      } else if (ledgerTorn) {
        if (file.write(reinterpret_cast<const uint8_t *>("\n"), 1) != 1) {
          failed = true;
          break;
        }
        ledgerTorn = false;
      }
    }
    char row[LedgerRowMax];
    size_t n = ledgerRow(row, sizeof(row), d, d.arrivedAt, d.boot);
    if (!n) {
      failed = true;
      break;
    }
    if (file.write(reinterpret_cast<const uint8_t *>(row), n) != n) {
      failed = ledgerTorn = true; // Part of the row may be in the file.
      break;
    }
    d.pendingLog = false;
    ++written;
  }
  if (file)
    file.close();
  if (failed) { // A pulled or full card can stall each attempt; do not hammer it.
    snprintf(app.storageError, sizeof(app.storageError), "Event log write failed");
    app.ledger = LedgerState::Failed;
    ledgerWait = 60000;
    return;
  }
  if (written)
    app.ledger = LedgerState::Ready;
  if (written && !strcmp(app.storageError, "Event log write failed"))
    app.storageError[0] = 0; // The card is taking rows again.
  if (written == 16)         // More are waiting: come back sooner.
    ledgerSoon = true;
}
void storageLoadAvatar() {
  if (!app.sdReady || uploadBusy())
    return;
  File config = card.open("/dashchan/avatar.cfg", FILE_READ);
  if (config) {
    char line[128]{}; // A profile line is at most limits::ProfileChars long; leave slack.
    size_t n = config.readBytesUntil('\n', line, sizeof(line) - 1);
    while (n && (line[n - 1] == '\r' || line[n - 1] == ' '))
      line[--n] = 0;
    if (!applyAvatarConfig(line))
      Serial.println("Avatar file ignored: invalid avatar profile");
    config.close();
  }
  watchlist.clear();
  File rules = card.open("/dashchan/watchlist.csv", FILE_READ);
  while (rules && rules.available()) {
    char line[100]{};
    size_t n = rules.readBytesUntil('\n', line, sizeof(line) - 1);
    if (n == sizeof(line) - 1) {
      while (rules.available() && rules.read() != '\n') {
      }
      Serial.println("Watchlist row ignored: too long");
      continue;
    }
    while (n && (line[n - 1] == '\r' || line[n - 1] == ' '))
      line[--n] = 0;
    if (*line && *line != '#' && !watchlist.add(line))
      Serial.println("Watchlist row ignored: invalid rule or limit reached");
  }
  if (rules)
    rules.close();
}
uint64_t storageCardSize() {
  return card.cardSize();
}
bool storageFormat(uint64_t expectedBytes) {
  if (uploadBusy() || app.session.armed || !expectedBytes ||
      (app.sdReady && expectedBytes != card.cardSize())) {
    Serial.println("FORMAT refused: stop recording and supply the card's exact size");
    return false;
  }
  bool mounted = app.sdReady;
  app.sdReady = false;
  bool ok = mounted ? card.formatFat32() : card.formatBlank(expectedBytes);
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
static void storageFail(const char *reason) {
  snprintf(app.storageError, sizeof(app.storageError), "%s", reason);
  bool wasRecording = app.session.armed;
  app.session.stop();
  captureGeneration.store(0);
  if (drive)
    drive.close();
  app.sdReady = false;
  say(reason);
  if (wasRecording) // Keep what the drive earned; the failure message stays on screen.
    saveSettings();
}
void storageBegin() {
  if (app.session.armed || uploadBusy())
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
  ledgerRestore();
}
bool startDrive() {
  if (uploadBusy()) {
    say("Finish the upload before starting a drive.");
    return false;
  }
  if (app.gpsSearching) {
    say("Finish GPS baud search before recording.");
    return false;
  }
  if (app.session.armed)
    return true;
  if (!app.radioReady) {
    say("Radio is not ready. Restart and check status.");
    return false;
  }
  if (!app.sdReady)
    storageBegin();
  if (!app.sdReady) {
    // In Voice ALL the buddy says so out loud, and the screen leads with the same words.
    say(voiceSpeak(Clip::NoSd, SpeechKind::Pet) ? NoCardHelp : app.storageError);
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
  app.bleCollection.reset();
  app.wifiRows = app.bleRows = 0;
  app.sessionAt = millis();
  app.session.start();
  captureGeneration.store(app.session.generation);
  lastFlush = millis();
  buddyDriveStart(usableFix(app.fix));
  return true;
}
void stopDrive() {
  bool wasRecording = app.session.armed;
  app.session.stop();
  captureGeneration.store(0);
  if (drive) {
    drive.flush();
    drive.close();
  }
  if (wasRecording)
    buddyDriveEnd(app.food.discoveries());
  else // Nothing was recording (for example the card failed mid-drive): never claim a save.
    say(*app.storageError ? app.storageError : "No drive is recording. Still keeping watch.");
  saveSettings();
  if (wasRecording && app.rows)
    uploadAfterDrive();
}
void storageTick() {
  ledgerTick();
  if (drive && uint32_t(millis() - lastFlush) >= 2000) {
    drive.flush();
    lastFlush = millis();
    if (drive.getWriteError())
      storageFail("SD error. Recording stopped.");
  }
}
void storageCheckLog() {
  if (uploadBusy() || !app.sdReady || app.session.armed || !*app.logPath) {
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
  if (c.ble) {
    ++app.bleRows;
    app.bleCollection.discover(c.mac);
  } else
    ++app.wifiRows;
  // Only a fresh Wi-Fi network actually written with a valid fix can feed us.
  if (!c.ble)
    buddyDiscovery(c.mac, c.name, !strncmp(auth, "[OPEN]", 6));
}
// The alert itself is recorded on the device's ledger row (radio.cpp marks it); an alert is the
// line most worth having if the power goes, so write it within a second instead of five.
// /events/detections.csv from older firmware is left on the card untouched and is not read.
void logAlert(const Alert &) {
  ledgerSoon = true;
}
} // namespace dash
