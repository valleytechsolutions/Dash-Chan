#include "app.h"
#include <M5StackChan.h>
namespace dash {
void printStatus() {
  Serial.printf(
      "STATUS "
      "{\"version\":\"%s\",\"board\":%u,\"width\":%d,\"height\":%d,\"psram\":%lu,\"heap\":%lu,"
      "\"radio\":%s,\"sd\":%s,\"recording\":%s,\"gps_chars\":%lu,\"gps_fix\":%s,\"sats\":%u,"
      "\"wifi\":%lu,\"ble\":%lu,\"rows\":%lu,\"drops\":%lu,\"motion\":%s,"
      "\"fuel\":%u,\"food_progress\":%u,\"discoveries\":%lu,\"uptime\":%lu,"
      "\"gps_good\":%lu,\"gps_bad\":%lu,\"sd_bytes\":%llu,\"storage_error\":\"%s\","
      "\"theme\":%u,\"volume\":%u,\"tone\":%u,\"pets\":%lu,\"recent\":%u,\"suppressed\":%lu,"
      "\"sound\":%s,\"voice\":%s}\n",
      Version, unsigned(M5.getBoard()), M5.Display.width(), M5.Display.height(),
      (unsigned long)ESP.getPsramSize(), (unsigned long)ESP.getFreeHeap(),
      app.radioReady ? "true" : "false", app.sdReady ? "true" : "false",
      app.session.armed ? "true" : "false", (unsigned long)app.gpsChars,
      usableFix(app.fix) ? "true" : "false", app.fix.sats, (unsigned long)app.wifiSeen,
      (unsigned long)app.bleSeen, (unsigned long)app.rows, (unsigned long)app.drops,
      app.settings.motion ? "true" : "false", app.energy, app.food.progress(),
      (unsigned long)app.food.discoveries(), (unsigned long)millis(), (unsigned long)app.gpsGood,
      (unsigned long)app.gpsBad, (unsigned long long)storageCardSize(), app.storageError,
      unsigned(app.settings.theme), app.settings.volume, unsigned(app.settings.tone),
      (unsigned long)app.pets, app.historyCount, (unsigned long)app.suppressed,
      app.settings.sound ? "true" : "false", app.settings.voice ? "true" : "false");
}
void serialTick() {
  static char line[128];
  static size_t used = 0;
  static bool overflow = false;
  unsigned budget = 64;
  while (Serial.available() && budget--) {
    char c = Serial.read();
    if (c == '\r')
      continue;
    if (c == '\n') {
      line[used] = 0;
      if (!overflow) {
        if (!strcmp(line, "STATUS"))
          printStatus();
        else if (!strcmp(line, "SCREEN"))
          uiScreenshot();
        else if (!strcmp(line, "STOP"))
          stopDrive();
        else if (!strcmp(line, "START"))
          startDrive();
        else if (!strcmp(line, "CHECK LOG"))
          storageCheckLog();
        else if (!strcmp(line, "AVATAR GET"))
          printAvatarConfig();
        else if (!strncmp(line, "AVATAR SET ", 11)) {
          Serial.println(app.session.armed              ? "AVATAR ERROR stop recording first"
                         : applyAvatarConfig(line + 11) ? "AVATAR OK"
                                                        : "AVATAR ERROR invalid profile");
        } else if (!strcmp(line, "RELOAD PROFILES") && !app.session.armed)
          storageLoadAvatar();
        else if (!strcmp(line, "DEMO AXON"))
          buddyDemo();
        else if (!strcmp(line, "SOUND TEST") || !strcmp(line, "SOUND STATUS")) {
          auto config = M5.Speaker.config();
          bool test = !strcmp(line, "SOUND TEST");
          bool started = test ? M5.Speaker.begin() : M5.Speaker.isRunning();
          bool queued = test && M5.Speaker.tone(880, 1200);
          Serial.printf("AUDIO enabled=%u running=%u begin=%u queued=%u volume=%u bck=%d ws=%d "
                        "data=%d rate=%lu\n",
                        M5.Speaker.isEnabled(), M5.Speaker.isRunning(), started, queued,
                        M5.Speaker.getVolume(), config.pin_bck, config.pin_ws, config.pin_data_out,
                        (unsigned long)config.sample_rate);
          for (uint8_t reg : {0x00, 0x01, 0x04, 0x05, 0x06, 0x0c}) {
            uint8_t value[2]{};
            bool ok = M5.In_I2C.readRegister(0x36, reg, value, 2, 400000);
            Serial.printf("AMP %02x=%02x%02x read=%u\n", reg, value[0], value[1], ok);
          }
        } else {
          int x, y;
          unsigned long long expectedBytes;
          if (sscanf(line, "TAP %d %d", &x, &y) == 2)
            uiTap(x, y);
          else if (sscanf(line, "FORMAT SD %llu", &expectedBytes) == 1)
            storageFormat(expectedBytes);
        }
      }
      used = 0;
      overflow = false;
    } else if (used < sizeof(line) - 1)
      line[used++] = c;
    else
      overflow = true;
  }
}
} // namespace dash
