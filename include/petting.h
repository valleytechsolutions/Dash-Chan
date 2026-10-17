#pragma once
#include <stdint.h>
namespace dash {
// One pet per deliberate hold, followed by a settled release. Brief sensor
// spikes and a stuck/high sensor cannot repeatedly trigger affection.
class PetTouch {
  bool raw_ = false, armed_ = false;
  uint32_t changed_ = 0;

public:
  bool update(uint32_t now, bool pressed) {
    if (pressed != raw_) {
      raw_ = pressed;
      changed_ = now;
    }
    if (!pressed) {
      if (uint32_t(now - changed_) >= 600)
        armed_ = true;
      return false;
    }
    if (armed_ && uint32_t(now - changed_) >= 250) {
      armed_ = false;
      return true;
    }
    return false;
  }
};
} // namespace dash
