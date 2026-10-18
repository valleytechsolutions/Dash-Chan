// SPDX-License-Identifier: GPL-3.0-or-later
#include "uploads.h"
#include "app.h"
#include "upload_csv.h"
#include <Network.h>
#include <algorithm>
#include <cJSON.h>
#include <esp_crt_bundle.h>
#include <esp_event.h>
#include <esp_heap_caps.h>
#include <esp_http_client.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <mbedtls/base64.h>
#include <mbedtls/platform.h>
#include <mbedtls/sha256.h>
#include <time.h>
namespace dash {
UploadStatus uploads;
static UploadConfig job;
static char jobPath[96];
static std::atomic<bool> canceled{false}, done{false};
static QueueHandle_t messages = nullptr;
static esp_netif_t *station = nullptr;
static char setupBuffer[2049]{};
static size_t setupSize = 0;
static uint32_t setupAt = 0;
static bool setupActive = false;
// K151 has 8 MiB PSRAM. Keep TLS record/certificate buffers out of scarce
// internal RAM without tearing down the live NimBLE host during a scan event.
static void *tlsCalloc(size_t n, size_t size) {
  if (size && n > SIZE_MAX / size)
    return nullptr;
  void *p =
      n * size >= 4096 ? heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : nullptr;
  return p ? p : heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}
struct Message {
  char text[96];
};
static void progress(const char *s) {
  Message m{};
  snprintf(m.text, sizeof(m.text), "%s", s);
  xQueueOverwrite(messages, &m);
}
static bool copyString(cJSON *root, const char *key, char *dst, size_t capacity) {
  cJSON *v = cJSON_GetObjectItemCaseSensitive(root, key);
  if (!cJSON_IsString(v) || strlen(v->valuestring) >= capacity)
    return false;
  for (const unsigned char *p = (const unsigned char *)v->valuestring; *p; ++p)
    if (*p < 32 || *p == 127)
      return false;
  strcpy(dst, v->valuestring);
  return true;
}
bool uploadConfigParse(const char *text, UploadConfig &out) {
  out = {};
  const char *end = nullptr;
  cJSON *r = cJSON_ParseWithOpts(text, &end, true);
  if (!r)
    return false;
  auto flag = [&](const char *key, bool &value) {
    auto item = cJSON_GetObjectItemCaseSensitive(r, key);
    if (!cJSON_IsBool(item))
      return false;
    value = cJSON_IsTrue(item);
    return true;
  };
  auto schema = cJSON_GetObjectItemCaseSensitive(r, "schema");
  bool ok = cJSON_IsObject(r) && cJSON_IsNumber(schema) && schema->valuedouble == 1 &&
            copyString(r, "ssid", out.ssid, sizeof(out.ssid)) && *out.ssid &&
            copyString(r, "password", out.password, sizeof(out.password)) &&
            copyString(r, "wigleName", out.wigleName, sizeof(out.wigleName)) &&
            copyString(r, "wigleToken", out.wigleToken, sizeof(out.wigleToken)) &&
            copyString(r, "wdgKey", out.wdgKey, sizeof(out.wdgKey)) && flag("wigle", out.wigle) &&
            flag("wdg", out.wdg) && flag("automatic", out.automatic) && flag("donate", out.donate);
  if (out.wigle && (!*out.wigleName || !*out.wigleToken || strchr(out.wigleName, ':')))
    ok = false;
  if (out.wdg && (strlen(out.wdgKey) != 64 || strspn(out.wdgKey, "0123456789abcdefABCDEF") != 64))
    ok = false;
  if (!out.wigle && !out.wdg)
    ok = false;
  cJSON_Delete(r);
  if (!ok)
    out = {};
  return ok;
}
static bool readConfig(UploadConfig &config) {
  if (!storageExists("/dashchan/upload.json") &&
      !(storageExists("/dashchan/upload-backup.json") &&
        storageRename("/dashchan/upload-backup.json", "/dashchan/upload.json")))
    return false;
  File f = storageOpen("/dashchan/upload.json");
  if (!f || f.size() > 2048)
    return false;
  char data[2049]{};
  size_t size = f.size(), n = f.readBytes(data, size);
  f.close();
  bool ok = n == size && uploadConfigParse(data, config);
  memset(data, 0, sizeof(data));
  return ok;
}
bool uploadBusy() {
  return uploads.busy;
}
bool uploadSetupCommand(const char *line) {
  if (strncmp(line, "SETUP ", 6))
    return false;
  auto clear = [] {
    memset(setupBuffer, 0, sizeof(setupBuffer));
    setupActive = false;
    setupSize = 0;
  };
  if (uploads.busy || app.session.armed || !app.sdReady) {
    clear();
    Serial.println("SETUP ERROR stop drive/upload; insert SD");
    return true;
  }
  if (!strcmp(line, "SETUP BEGIN")) {
    clear();
    setupActive = true;
    setupAt = millis();
    Serial.println("SETUP OK BEGIN");
  } else if (!strcmp(line, "SETUP CLEAR")) {
    clear();
    storageRemove("/dashchan/upload-new.json");
    storageRemove("/dashchan/upload-backup.json");
    File existing = storageOpen("/dashchan/upload.json");
    bool existed = bool(existing);
    existing.close();
    bool ok = !existed || storageRemove("/dashchan/upload.json");
    uploadRefresh();
    Serial.println(ok ? "SETUP OK CLEAR" : "SETUP ERROR cannot remove setup");
  } else if (!strncmp(line, "SETUP DATA ", 11) && setupActive) {
    const char *hex = line + 11;
    size_t length = strlen(hex);
    if (uint32_t(millis() - setupAt) > 10000 || !length || length % 2 || length > 96 ||
        setupSize + length / 2 > 2048 || strspn(hex, "0123456789abcdefABCDEF") != length) {
      clear();
      Serial.println("SETUP ERROR invalid/expired chunk");
      return true;
    }
    for (size_t i = 0; i < length; i += 2) {
      char byte[] = {hex[i], hex[i + 1], 0};
      unsigned value = strtoul(byte, nullptr, 16);
      if (!value) {
        clear();
        Serial.println("SETUP ERROR binary data");
        return true;
      }
      setupBuffer[setupSize++] = char(value);
    }
    setupAt = millis();
    Serial.println("SETUP OK DATA");
  } else if (!strcmp(line, "SETUP END") && setupActive) {
    UploadConfig c{};
    bool ok = uint32_t(millis() - setupAt) <= 10000 && uploadConfigParse(setupBuffer, c);
    c = {};
    if (ok) {
      storageMakeDir("/dashchan");
      storageRemove("/dashchan/upload-new.json");
      File f = storageOpen("/dashchan/upload-new.json", "w");
      ok = f && f.write((const uint8_t *)setupBuffer, setupSize) == setupSize;
      if (f) {
        f.flush();
        ok &= !f.getWriteError();
        f.close();
      }
      File verify = storageOpen("/dashchan/upload-new.json");
      ok = ok && verify && verify.size() == setupSize;
      for (size_t i = 0; ok && i < setupSize; ++i)
        if (verify.read() != (uint8_t)setupBuffer[i])
          ok = false;
      verify.close();
      if (ok) {
        // FAT rename cannot replace an existing file; retain a recovery copy until the new name
        // commits.
        File old = storageOpen("/dashchan/upload.json");
        bool hadOld = bool(old);
        old.close();
        storageRemove("/dashchan/upload-backup.json");
        ok = !hadOld || storageRename("/dashchan/upload.json", "/dashchan/upload-backup.json");
        if (ok)
          ok = storageRename("/dashchan/upload-new.json", "/dashchan/upload.json");
        if (!ok && hadOld)
          storageRename("/dashchan/upload-backup.json", "/dashchan/upload.json");
        if (ok)
          storageRemove("/dashchan/upload-backup.json");
      }
    }
    clear();
    uploadRefresh();
    Serial.println(ok ? "SETUP OK SAVED" : "SETUP ERROR invalid setup or SD write failed");
  } else {
    clear();
    Serial.println("SETUP ERROR begin a new transfer");
  }
  return true;
}
static bool driveName(const char *s) {
  if (strlen(s) != 17 || strncmp(s, "dash-", 5) || strcmp(s + 13, ".csv"))
    return false;
  for (int i = 5; i < 13; ++i)
    if (!isdigit((unsigned char)s[i]))
      return false;
  return true;
}
void uploadRefresh() {
  if (uploads.busy)
    return;
  static bool allocatorReady = false;
  if (!allocatorReady) {
    mbedtls_platform_set_calloc_free(tlsCalloc, heap_caps_free);
    allocatorReady = true;
  }
  uploads.count = uploads.selected = 0;
  UploadConfig c{};
  uploads.configured = app.sdReady && readConfig(c);
  uploads.wigle = c.wigle;
  uploads.wdg = c.wdg;
  uploads.automatic = c.automatic;
  c = {};
  if (!app.sdReady) {
    strcpy(uploads.message, "Insert an SD card first.");
    return;
  }
  File folder = storageOpen("/wardrives");
  for (File f = folder.openNextFile(); f; f = folder.openNextFile()) {
    const char *name = f.name();
    const char *slash = strrchr(name, '/');
    if (slash)
      name = slash + 1;
    if (!f.isDirectory() && driveName(name)) {
      unsigned pos = 0;
      while (pos < uploads.count && strcmp(uploads.files[pos], name) > 0)
        ++pos;
      if (pos < 24) {
        unsigned last = std::min(23u, uploads.count);
        for (unsigned j = last; j > pos; --j)
          strcpy(uploads.files[j], uploads.files[j - 1]);
        snprintf(uploads.files[pos], sizeof(uploads.files[pos]), "%s", name);
        uploads.count = std::min(24u, uploads.count + 1);
      }
    }
    f.close();
  }
  folder.close();
  snprintf(uploads.message, sizeof(uploads.message), "%s",
           uploads.configured
               ? "Uploads share GPS, names and addresses. Choose a drive, then send."
               : "Set up on the website. Copy upload.json to /dashchan/ on SD, then Refresh.");
}
class CsvReader {
  File &file;
  uint8_t buffer[2048];
  size_t position = 0, length = 0, remaining;

public:
  bool error = false;
  explicit CsvReader(File &f) : file(f), remaining(f.size()) {}
  bool next(std::string &line) {
    line.clear();
    while (!canceled) {
      if (position == length) {
        if (!remaining)
          return !line.empty();
        length = file.read(buffer, std::min(remaining, sizeof(buffer)));
        position = 0;
        if (!length) {
          error = true;
          return false;
        }
        remaining -= length;
      }
      char c = buffer[position++];
      if (c == '\n')
        return true;
      if (c != '\r')
        line += c;
      if (line.size() > 1024) {
        error = true;
        return false;
      }
    }
    return false;
  }
};
static bool prepareFile(char digest[65]) {
  File f = storageOpen(jobPath);
  if (!f || f.size() > 38u * 1024 * 1024 || f.size() < 200) {
    progress("Empty, missing or oversized drive (38 MiB maximum).");
    return false;
  }
  mbedtls_sha256_context hash;
  mbedtls_sha256_init(&hash);
  mbedtls_sha256_starts(&hash, 0);
  uint8_t block[2048], result[32];
  size_t remaining = f.size();
  while (remaining && !canceled) {
    size_t n = f.read(block, std::min(remaining, sizeof(block)));
    if (!n)
      break;
    mbedtls_sha256_update(&hash, block, n);
    remaining -= n;
    delay(1);
  }
  mbedtls_sha256_finish(&hash, result);
  mbedtls_sha256_free(&hash);
  if (remaining || canceled) {
    f.close();
    return false;
  }
  for (unsigned i = 0; i < 32; ++i)
    snprintf(digest + i * 2, 3, "%02x", result[i]);
  f.seek(0);
  CsvReader reader(f);
  std::string line, converted, spool;
  spool.reserve(4096);
  if (!reader.next(line) || line.rfind("WigleWifi-1.4,", 0) != 0 || !reader.next(line) ||
      line != "MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,CurrentLatitude,CurrentLongitude,"
              "AltitudeMeters,AccuracyMeters,Type") {
    progress("This uploader needs an original Dash-Chan WiGLE 1.4 drive.");
    return false;
  }
  storageMakeDir("/dashchan");
  storageRemove("/dashchan/upload-temp.csv");
  File output = storageOpen("/dashchan/upload-temp.csv", "w");
  if (!output) {
    progress("Cannot create upload copy on SD.");
    return false;
  }
  output.printf(
      "WigleWifi-1.6,appRelease=%s,model=StackChan,device=ESP32-S3,board=K151,brand=M5Stack\n%s\n",
      Version, UploadColumns);
  unsigned rows = 0;
  bool ok = true;
  while (reader.next(line) && !canceled) {
    if (!upgradeCsvRow(line, converted)) {
      ok = false;
      break;
    }
    spool += converted;
    if (spool.size() >= 4096) {
      if (output.write((const uint8_t *)spool.data(), spool.size()) != spool.size()) {
        ok = false;
        break;
      }
      spool.clear();
    }
    ++rows;
    if (rows % 64 == 0) {
      char message[96];
      snprintf(message, sizeof(message), "Preparing compatible CSV: %u observations...", rows);
      progress(message);
      delay(1);
    }
  }
  if (!spool.empty() && output.write((const uint8_t *)spool.data(), spool.size()) != spool.size())
    ok = false;
  ok &= !reader.error;
  output.flush();
  ok = ok && rows > 0 && !output.getWriteError() && output.size() <= 40u * 1024 * 1024;
  output.close();
  f.close();
  if (!ok)
    progress("Drive has no usable rows, invalid CSV, or SD write failed.");
  return ok && !canceled;
}
static bool joinNetwork() {
  if (!station) {
    if (esp_netif_init() != ESP_OK) {
      progress("Network initialization failed.");
      return false;
    }
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
      progress("Network event initialization failed.");
      return false;
    }
    station = esp_netif_create_default_wifi_sta();
    if (!station) {
      progress("Network interface allocation failed.");
      return false;
    }
  }
  wifi_config_t config{};
  memcpy(config.sta.ssid, job.ssid, strlen(job.ssid));
  memcpy(config.sta.password, job.password, strlen(job.password));
  config.sta.pmf_cfg.capable = true;
  esp_wifi_stop();
  bool ok = esp_wifi_set_mode(WIFI_MODE_STA) == ESP_OK &&
            esp_wifi_set_config(WIFI_IF_STA, &config) == ESP_OK && esp_wifi_start() == ESP_OK &&
            esp_wifi_connect() == ESP_OK;
  memset(&config, 0, sizeof(config));
  uint32_t at = millis();
  while (ok && !canceled && uint32_t(millis() - at) < 25000) {
    esp_netif_ip_info_t ip{};
    if (esp_netif_get_ip_info(station, &ip) == ESP_OK && ip.ip.addr)
      return true;
    delay(100);
  }
  progress("Wi-Fi connection failed. Check saved network and password.");
  return false;
}
static bool postFile(bool wdg, const char *digest) {
  char marker[110];
  snprintf(marker, sizeof(marker), "/dashchan/uploads/%s-%s", digest, wdg ? "wdg" : "wigle");
  std::string pending = std::string(marker) + ".pending",
              accepted = std::string(marker) + ".accepted";
  if (storageExists(accepted.c_str())) {
    progress(wdg ? "WDGWars: already accepted; skipped." : "WiGLE: already accepted; skipped.");
    return true;
  }
  if (storageExists(pending.c_str())) {
    progress("Earlier result uncertain. Check account; see upload guide before retry.");
    return false;
  }
  File f = storageOpen(wdg ? "/dashchan/upload-temp.csv" : jobPath);
  if (!f) {
    progress("Cannot reopen drive on SD.");
    return false;
  }
  const char *boundary = "DashChanUploadBoundary0d61";
  std::string prefix = std::string("--") + boundary +
                       "\r\nContent-Disposition: form-data; name=\"file\"; "
                       "filename=\"dashchan.csv\"\r\nContent-Type: text/csv\r\n\r\n";
  std::string suffix = "\r\n";
  if (!wdg)
    suffix += std::string("--") + boundary +
              "\r\nContent-Disposition: form-data; name=\"donate\"\r\n\r\n" +
              (job.donate ? "on" : "false") + "\r\n";
  suffix += std::string("--") + boundary + "--\r\n";
  esp_http_client_config_t config{};
  config.url =
      wdg ? "https://wdgwars.pl/api/upload-csv" : "https://api.wigle.net/api/v2/file/upload";
  config.method = HTTP_METHOD_POST;
  config.crt_bundle_attach = esp_crt_bundle_attach;
  config.timeout_ms = 15000;
  config.disable_auto_redirect = true;
  config.buffer_size = 2048;
  auto client = esp_http_client_init(&config);
  if (!client) {
    progress("HTTPS allocation failed.");
    return false;
  }
  esp_http_client_set_header(client, "Content-Type",
                             (std::string("multipart/form-data; boundary=") + boundary).c_str());
  esp_http_client_set_header(client, "Accept", "application/json");
  esp_http_client_set_header(client, "User-Agent", "Dash-Chan");
  if (wdg)
    esp_http_client_set_header(client, "X-API-Key", job.wdgKey);
  else {
    char source[260]{}, encoded[360]{};
    size_t n = 0;
    snprintf(source, sizeof(source), "%s:%s", job.wigleName, job.wigleToken);
    mbedtls_base64_encode((unsigned char *)encoded, sizeof(encoded), &n,
                          (const unsigned char *)source, strlen(source));
    esp_http_client_set_header(client, "Authorization", (std::string("Basic ") + encoded).c_str());
    memset(source, 0, sizeof(source));
    memset(encoded, 0, sizeof(encoded));
  }
  bool marked = false;
  bool ok =
      !canceled && esp_http_client_open(client, prefix.size() + f.size() + suffix.size()) == ESP_OK;
  if (ok) {
    File p = storageOpen(pending.c_str(), "w");
    marked = p && p.println(jobPath) > 0;
    if (p) {
      p.flush();
      marked &= !p.getWriteError();
      p.close();
    }
    ok = marked;
  }
  auto writeAll = [&](const char *data, size_t n) {
    while (n && !canceled) {
      int sent = esp_http_client_write(client, data, n);
      if (sent <= 0)
        return false;
      data += sent;
      n -= sent;
      delay(1);
    }
    return !n;
  };
  if (ok)
    ok = writeAll(prefix.data(), prefix.size());
  char buffer[2048];
  size_t remaining = f.size();
  while (ok && remaining && !canceled) {
    size_t n = f.read((uint8_t *)buffer, std::min(remaining, sizeof(buffer)));
    ok = n && writeAll(buffer, n);
    remaining -= n;
  }
  if (ok && !canceled && !remaining)
    ok = writeAll(suffix.data(), suffix.size());
  else
    ok = false;
  int status = 0;
  bool success = false;
  if (ok && esp_http_client_fetch_headers(client) >= 0) {
    status = esp_http_client_get_status_code(client);
    int n = esp_http_client_read_response(client, buffer, sizeof(buffer) - 1);
    if (n > 0 && n < int(sizeof(buffer) - 1)) {
      buffer[n] = 0;
      auto response = cJSON_Parse(buffer);
      success = status >= 200 && status < 300 &&
                cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(response, wdg ? "ok" : "success"));
      if (response)
        cJSON_Delete(response);
    }
  }
  esp_http_client_close(client);
  esp_http_client_cleanup(client);
  f.close();
  if (success) {
    File p = storageOpen(accepted.c_str(), "w");
    bool saved = p && p.println(jobPath) > 0;
    if (p) {
      p.flush();
      saved &= !p.getWriteError();
      p.close();
    }
    if (saved)
      storageRemove(pending.c_str());
    progress(wdg ? "WDGWars accepted this drive. Processing may continue online."
                 : "WiGLE accepted this drive. Processing may continue online.");
  } else {
    if (status >= 400 && status < 500 && status != 408)
      storageRemove(pending.c_str());
    char message[96];
    snprintf(message, sizeof(message), "%s HTTP %d. %s", wdg ? "WDGWars" : "WiGLE", status,
             marked && (status < 400 || status >= 500 || status == 408)
                 ? "Result uncertain: check account before retry."
                 : "Not accepted. Check connection/key; try manually later.");
    progress(message);
  }
  return success;
}
static void worker(void *) {
  char hash[65]{};
  bool ok = false;
  progress("Checking drive and preparing compatible upload copy...");
  if (prepareFile(hash)) {
    progress("Connecting to saved Wi-Fi. Watching is paused.");
    if (joinNetwork()) {
      progress("Checking internet time for secure HTTPS...");
      configTime(0, 0, "time.cloudflare.com", "pool.ntp.org");
      uint32_t at = millis();
      while (!canceled && time(nullptr) < 1735689600 && uint32_t(millis() - at) < 20000)
        delay(100);
      if (time(nullptr) >= 1735689600 && !canceled) {
        storageMakeDir("/dashchan/uploads");
        ok = true;
        if (job.wigle) {
          progress("Sending drive to WiGLE over HTTPS...");
          ok = postFile(false, hash);
        }
        if (job.wdg && !canceled) {
          bool first = ok;
          progress("Sending compatible drive to WDGWars over HTTPS...");
          ok = postFile(true, hash) && first;
        }
        if (ok)
          progress("Selected services accepted/skipped this drive. Watching resumes.");
        else if (job.wigle && job.wdg)
          progress("One or more uploads failed/uncertain. Check accounts and SD receipts.");
      } else
        progress("Internet time unavailable. Secure upload canceled; try later.");
    }
  }
  esp_wifi_disconnect();
  esp_wifi_stop();
  wifi_config_t empty{};
  esp_wifi_set_config(WIFI_IF_STA, &empty);
  bool restored = esp_wifi_set_mode(WIFI_MODE_NULL) == ESP_OK && esp_wifi_start() == ESP_OK;
  storageRemove("/dashchan/upload-temp.csv");
  job = {};
  if (canceled)
    progress("Canceled. Check accounts if sending began before cancellation.");
  if (!restored)
    progress("Radio restore failed. Restart Dash-Chan.");
  done.store(true);
  vTaskDelete(nullptr);
}
void uploadStart(bool automatic) {
  if (uploads.busy || app.session.armed || !app.sdReady || !app.radioReady) {
    strcpy(uploads.message, "Stop and save first. SD and radio must be ready.");
    return;
  }
  if (!readConfig(job)) {
    strcpy(uploads.message, "Missing or invalid /dashchan/upload.json. Use website setup.");
    return;
  }
  if (automatic && !job.automatic) {
    job = {};
    return;
  }
  if (!uploads.count || uploads.selected >= uploads.count) {
    strcpy(uploads.message, "No drive selected. Refresh first.");
    job = {};
    return;
  }
  snprintf(jobPath, sizeof(jobPath), "/wardrives/%s", uploads.files[uploads.selected]);
  if (!messages)
    messages = xQueueCreate(1, sizeof(Message));
  if (!messages) {
    job = {};
    strcpy(uploads.message, "Upload queue allocation failed.");
    return;
  }
  canceled = false;
  done = false;
  uploads.busy = true;
  radioPause(true);
  if (xTaskCreate(worker, "dash-upload", 12288, nullptr, 1, nullptr) != pdPASS) {
    uploads.busy = false;
    job = {};
    radioPause(false);
    strcpy(uploads.message, "Upload task allocation failed.");
  }
}
void uploadCancel() {
  if (uploads.busy)
    canceled = true;
}
void uploadTick() {
  if (setupActive && uint32_t(millis() - setupAt) > 10000) {
    memset(setupBuffer, 0, sizeof(setupBuffer));
    setupSize = 0;
    setupActive = false;
  }
  if (!uploads.busy)
    return;
  Message m;
  if (xQueueReceive(messages, &m, 0) == pdTRUE)
    snprintf(uploads.message, sizeof(uploads.message), "%s", m.text);
  if (done.load()) {
    if (xQueueReceive(messages, &m, 0) == pdTRUE)
      snprintf(uploads.message, sizeof(uploads.message), "%s", m.text);
    uploads.busy = false;
    radioPause(false);
    say(uploads.message);
  }
}
void uploadAfterDrive() {
  uploadRefresh();
  if (!uploads.configured || !uploads.automatic)
    return;
  const char *name = strrchr(app.logPath, '/');
  if (!name)
    return;
  for (unsigned i = 0; i < uploads.count; ++i)
    if (!strcmp(uploads.files[i], name + 1)) {
      uploads.selected = i;
      uploadStart(true);
      break;
    }
}
} // namespace dash
