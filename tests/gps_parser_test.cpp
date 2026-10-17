#include "gps_support.h"
#include <TinyGPS++.h>
#include <cassert>
#include <cstdio>
#include <iostream>
#include <string>
static std::string nmea(const std::string &body) {
  unsigned char sum = 0;
  for (char c : body)
    sum ^= c;
  char tail[8];
  snprintf(tail, sizeof(tail), "*%02X\r\n", sum);
  return "$" + body + tail;
}
static void feed(TinyGPSPlus &gps, dash::NmeaFrame &frame, const std::string &s) {
  for (char c : s)
    if (frame.feed(c))
      for (const char *p = frame.line(); *p; ++p)
        gps.encode(*p);
}
int main() {
  for (const char *talker : {"GP", "GN", "GA", "GL", "GB", "BD", "GQ", "GI"}) {
    TinyGPSPlus gps;
    dash::NmeaFrame frame;
    feed(gps, frame,
         nmea(std::string(talker) +
              "GGA,123519.00,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,"));
    feed(gps, frame,
         nmea(std::string(talker) + "RMC,123519.00,A,4807.038,N,01131.000,E,10.0,84.4,171026,,,A"));
    assert(gps.location.isValid() && gps.altitude.isValid() && gps.speed.isValid());
    assert(std::abs(gps.location.lat() - 48.1173) < 0.00001 &&
           std::abs(gps.location.lng() - 11.5166667) < 0.00001);
    assert(gps.satellites.value() == 8 && std::abs(gps.hdop.hdop() - 0.9) < 0.01);
    assert(std::abs(gps.altitude.meters() - 545.4) < 0.01 &&
           std::abs(gps.speed.kmph() - 18.52) < 0.01);
    assert(gps.date.year() == 2026 && gps.date.month() == 10 && gps.date.day() == 17);
    assert(gps.time.hour() == 12 && gps.time.minute() == 35 && gps.time.second() == 19);
    assert(gps.passedChecksum() == 2 && gps.failedChecksum() == 0);
  }
  TinyGPSPlus gps;
  dash::NmeaFrame frame;
  feed(gps, frame, nmea("GNGGA,123519.00,,,,,0,00,99.9,,,,,,"));
  feed(gps, frame, nmea("GNRMC,123519.00,V,,,,,,,171026,,,N"));
  assert(frame.good == 2 && !gps.location.isValid());
  auto bad = nmea("BDRMC,123519.00,A,4807.038,N,01131.000,E,10.0,84.4,171026,,,A");
  bad[20] ^= 1;
  feed(gps, frame, bad);
  assert(frame.bad == 1 && !gps.location.isValid());
  std::cout << "Eight GNSS talkers decoded through pinned TinyGPSPlus; no-fix and corrupt data "
               "rejected.\n";
}
