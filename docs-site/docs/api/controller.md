---
title: Controller
sidebar_position: 6
---

# Controller

`src/controller/controller.hh` — thin helpers on top of `psyqo::AdvancedPad` for analog stick handling.

```cpp
static constexpr uint8_t ANALOG_STICK_DEADZONE = 16;
static constexpr uint8_t ANALOG_STICK_DEADZONE_X = 16;
static constexpr uint8_t ANALOG_STICK_DEADZONE_Y = 16;
static constexpr uint8_t ANALOG_STICK_MAX_INPUT = 127; // sticks range [-128, 127]

class ControllerHelper final {
public:
  enum AnalogStickIndex { RightStickX, RightStickY, LeftStickX, LeftStickY };

  static void init(void);
  static int GetNormalizedAnalogStickInput(psyqo::AdvancedPad::Pad pad, uint8_t index);
  static bool IsPadAnalog(psyqo::AdvancedPad::Pad pad);
};
```

`GetNormalizedAnalogStickInput` returns the raw stick value re-centered around zero (`adc - 0x80`, so roughly `-128..127`), with the Y axes sign-flipped so "up" on the stick reads positive. Use `IsPadAnalog` first to check the connected pad actually supports analog input before reading stick axes from it.

### Usage

The deadzone thresholding shipped games actually do, since the engine doesn't apply one for you:

```cpp
if (ControllerHelper::IsPadAnalog(psyqo::AdvancedPad::Pad::Pad1a)) {
    int rx = ControllerHelper::GetNormalizedAnalogStickInput(pad, ControllerHelper::RightStickX);
    int ry = ControllerHelper::GetNormalizedAnalogStickInput(pad, ControllerHelper::RightStickY);

    constexpr int deadzone = 16;
    if (ry < -deadzone || ry > deadzone) camera.UpdateOrbitAngles(ry * ORBIT_SPEED, 0, deltaTime);
    if (rx < -deadzone || rx > deadzone) camera.UpdateOrbitAngles(0, rx * ORBIT_SPEED, deltaTime);
}
```

### Internals

- No deadzone is actually applied by `GetNormalizedAnalogStickInput` despite the `ANALOG_STICK_DEADZONE*` constants existing in the header — apply your own thresholding on the returned value if stick drift near center is an issue.
