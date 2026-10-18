// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core.h"
#include <vector>
namespace dash {
constexpr const char *UploadColumns =
    "MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,CurrentLongitude,"
    "AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type";
// One physical line from Dash-Chan: captured names are sanitized before logging.
inline bool parseCsvLine(const std::string &line, std::vector<std::string> &fields) {
  fields.clear();
  std::string field;
  bool quoted = false, closed = false;
  for (size_t i = 0; i < line.size(); ++i) {
    char c = line[i];
    if (c == '\r' || c == '\n' || c == 0)
      return false;
    if (quoted) {
      if (c == '"') {
        if (i + 1 < line.size() && line[i + 1] == '"') {
          field += '"';
          ++i;
        } else {
          quoted = false;
          closed = true;
        }
      } else
        field += c;
    } else if (c == ',') {
      fields.push_back(field);
      field.clear();
      closed = false;
    } else if (c == '"' && field.empty() && !closed)
      quoted = true;
    else if (closed || c == '"')
      return false;
    else
      field += c;
    if (fields.size() > 14 || field.size() > 256)
      return false;
  }
  if (quoted)
    return false;
  fields.push_back(field);
  return true;
}
inline bool upgradeCsvRow(const std::string &line, std::string &out) {
  std::vector<std::string> f;
  if (!parseCsvLine(line, f) || f.size() != 11 || (f[10] != "BLE" && f[10] != "WIFI"))
    return false;
  if (f[0].size() != 17 || f[3].size() != 19)
    return false;
  for (unsigned i = 0; i < 17; ++i)
    if (i % 3 == 2 ? f[0][i] != ':' : !isxdigit((unsigned char)f[0][i]))
      return false;
  for (unsigned i = 0; i < 19; ++i) {
    char expected = i == 4 || i == 7 ? '-' : i == 10 ? ' ' : i == 13 || i == 16 ? ':' : 0;
    if (expected ? f[3][i] != expected : !isdigit((unsigned char)f[3][i]))
      return false;
  }
  char *end = nullptr;
  long rssi = strtol(f[5].c_str(), &end, 10);
  if (f[5].empty() || *end || rssi < -127 || rssi > 0)
    return false;
  long channel = strtol(f[4].c_str(), &end, 10);
  if (!*f[4].c_str() || *end || channel < 0 || channel > 14)
    return false;
  for (unsigned i : {6u, 7u, 8u, 9u}) {
    double n = strtod(f[i].c_str(), &end);
    if (f[i].empty() || *end || !isfinite(n) || (i == 6 && fabs(n) > 90) ||
        (i == 7 && fabs(n) > 180) || (i == 9 && n <= 0))
      return false;
  }
  unsigned frequency = f[10] == "BLE" || channel == 0 ? 0
                       : channel == 14                ? 2484
                                                      : 2407 + channel * 5;
  out.clear();
  for (unsigned i = 0; i < 11; ++i) {
    if (i)
      out += ',';
    if (i == 10)
      out += ",0,"; // RCOIs unavailable; manufacturer ID unknown.
    out += csv(f[i].c_str());
    if (i == 4)
      out += ',' + std::to_string(frequency);
  }
  out += '\n';
  return true;
}
} // namespace dash
