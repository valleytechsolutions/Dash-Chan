#pragma once
#include <stdint.h>

namespace dash {
// Angles are in tenths of a degree; speed controls BSP spring stiffness. Each gesture
// returns to a comfortable forward pose before a variable, quiet rest.
struct MotionCue {
  int yaw = 0, pitch = 200, speed = 90;
  uint32_t holdMs = 1500;
};
inline MotionCue panicMotion(unsigned step) {
  static constexpr MotionCue cues[] = {
      {-280, 300, 560, 420}, {320, 180, 600, 460},  {210, 360, 500, 340}, {320, 230, 560, 330},
      {-320, 150, 600, 480}, {-160, 350, 500, 360}, {120, 150, 560, 360}, {0, 260, 380, 600}};
  return cues[step % 8];
}
class IdleMotion {
public:
  void reset(uint32_t now, uint32_t pauseMs = 2000) {
    at_ = now;
    wait_ = pauseMs;
    step_ = count_ = 0;
  }
  bool tick(uint32_t now, uint32_t random, bool sleepy, unsigned avatar, bool driving,
            MotionCue &cue) {
    if (uint32_t(now - at_) < wait_)
      return false;
    if (step_ == count_) {
      int side = random & 0x100 ? 1 : -1;
      int speed = sleepy ? 75 : (avatar == 1 ? 180 : avatar == 2 ? 130 : 150);
      rest_ = (driving ? 1300 : 2200) + (random >> 12) % 2500;
      if (sleepy) {
        cues_[0] = {side * 35, 145, speed, 2300};
        cues_[1] = {0, 190, speed, 1800};
        count_ = 2;
        rest_ += 4000;
      } else {
        switch (random % 4) {
        case 0: // Notice something, linger, then check back with our human.
          cues_[0] = {side * (120 + int((random >> 9) % 81)), 230, speed, 1700};
          cues_[1] = {side * 65, 265, speed, 1200};
          cues_[2] = {0, 200, speed, 1500};
          count_ = 3;
          break;
        case 1: // A small contented nod, not a continuous metronome.
          cues_[0] = {side * 30, 150, speed, 1100};
          cues_[1] = {0, 255, speed, 1100};
          cues_[2] = {0, 200, speed, 1500};
          count_ = 3;
          break;
        case 2: // Curious upward glance.
          cues_[0] = {side * 100, 310, speed, 2000};
          cues_[1] = {0, 200, speed, 1700};
          count_ = 2;
          break;
        default: // Subtle posture adjustment, followed by a longer still pause.
          cues_[0] = {side * 50, 230, 100, 1800};
          cues_[1] = {0, 200, 100, 1800};
          count_ = 2;
          rest_ += 2000;
          break;
        }
      }
      step_ = 0;
    }
    cue = cues_[step_++];
    at_ = now;
    wait_ = cue.holdMs + (step_ == count_ ? rest_ : 0);
    return true;
  }

private:
  MotionCue cues_[3];
  uint32_t at_ = 0, wait_ = 2000, rest_ = 5000;
  unsigned step_ = 0, count_ = 0;
};
} // namespace dash
