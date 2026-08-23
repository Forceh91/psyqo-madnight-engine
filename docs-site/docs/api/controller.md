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

Reading the right stick for camera control, once you've confirmed the pad is analog:

```cpp
constexpr auto pad = psyqo::AdvancedPad::Pad::Pad1a;
if (ControllerHelper::IsPadAnalog(pad)) {
    int rx = ControllerHelper::GetNormalizedAnalogStickInput(pad, ControllerHelper::RightStickX);
    int ry = ControllerHelper::GetNormalizedAnalogStickInput(pad, ControllerHelper::RightStickY);

    if (ry != 0) camera.UpdateOrbitAngles(ry * ORBIT_SPEED, 0, deltaTime);
    if (rx != 0) camera.UpdateOrbitAngles(0, rx * ORBIT_SPEED, deltaTime);
}
```

### Internals

- `GetNormalizedAnalogStickInput` applies the `ANALOG_STICK_DEADZONE*` constants (16 on each axis) before returning, so stick drift near center already reads as zero.
- `init()` puts the controller into analog mode.
- Unlike `ControllerHelper`'s explicit `Pad` parameter, the engine's `AdvancedPad::Event`-driven code (menu navigation, the debug menu, the pause-menu bind) never checks `event.pad`: on a multitap, any connected pad fires those handlers, not just the one you might expect.
