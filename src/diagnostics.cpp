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
      "\"gps_good\":%lu,\"gps_bad\":%lu,\"sd_bytes\":%llu}\n",
      Version, unsigned(M5.getBoard()), M5.Display.width(), M5.Display.height(),
      (unsigned long)ESP.getPsramSize(), (unsigned long)ESP.getFreeHeap(),
      app.radioReady ? "true" : "false", app.sdReady ? "true" : "false",
      app.session.armed ? "true" : "false", (unsigned long)app.gpsChars,
      usableFix(app.fix) ? "true" : "false", app.fix.sats, (unsigned long)app.wifiSeen,
      (unsigned long)app.bleSeen, (unsigned long)app.rows, (unsigned long)app.drops,
      app.settings.motion ? "true" : "false", app.energy, app.food.progress(),
      (unsigned long)app.food.discoveries(), (unsigned long)millis(), (unsigned long)app.gpsGood,
      (unsigned long)app.gpsBad, (unsigned long long)storageCardSize());
}
void serialTick() {
  static char line[64];
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
        else {
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
