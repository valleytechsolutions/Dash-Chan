// Minimal host shim for the pinned TinyGPSPlus parser's integration tests.
#pragma once
#include <chrono>
#include <cmath>
#include <cstdint>
using byte = uint8_t;
constexpr double TWO_PI = 6.2831853071795864769;
inline double radians(double d) {
  return d * TWO_PI / 360.0;
}
inline double degrees(double r) {
  return r * 360.0 / TWO_PI;
}
template <class T> inline T sq(T v) {
  return v * v;
}
unsigned long millis();
