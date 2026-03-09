#ifndef _CONTROLLER_H
#define _CONTROLLER_H

#include "psyqo/advancedpad.hh"

static constexpr uint8_t ANALOG_STICK_DEADZONE = 16;
static constexpr uint8_t ANALOG_STICK_DEADZONE_X = 16;
static constexpr uint8_t ANALOG_STICK_DEADZONE_Y = 16;
// analog sticks are in the range of [-128,127]
static constexpr uint8_t ANALOG_STICK_MAX_INPUT = 127;

class ControllerHelper final {
public:
  enum AnalogStickIndex { RightStickX, RightStickY, LeftStickX, LeftStickY };

  static void init(void);
  static int GetNormalizedAnalogStickInput(psyqo::AdvancedPad::Pad pad, uint8_t index);
  static bool IsPadAnalog(psyqo::AdvancedPad::Pad pad);
};

#endif
