#pragma once
#include <math.h>
#include <stdint.h>
namespace dash {
// GPS movement for animation only. Hysteresis rejects stationary jitter;
// missing/stale speed immediately clears the travel pose.
class TravelState {
  bool moving_ = false, pending_ = false;
  uint32_t since_ = 0;

public:
  bool update(uint32_t now, bool freshFixAndSpeed, double kph) {
    if (!freshFixAndSpeed || !isfinite(kph) || kph < 0 || kph > 300) {
      moving_ = pending_ = false;
      return false;
    }
    bool change = moving_ ? kph <= 2 : kph >= 5;
    if (!change)
      pending_ = false;
    else if (!pending_) {
      pending_ = true;
      since_ = now;
    } else if (uint32_t(now - since_) >= (moving_ ? 5000u : 3000u)) {
      moving_ = !moving_;
      pending_ = false;
    }
    return moving_;
  }
};
inline int fieldScene(unsigned policy, bool recording, bool moving) {
  return policy == 2 ? 0 : moving ? 2 : (policy == 1 || recording) ? 1 : 0;
}
} // namespace dash
