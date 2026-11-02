---
title: Controller
sidebar_position: 6
---

# Controller

`src/controller/controller.hh` has a small helper on top of `psyqo::AdvancedPad` for reading analog sticks, reached as `g_madnightEngine.m_controllerHelper`. Buttons and events come from `g_madnightEngine.m_input` (the `AdvancedPad`) as usual.

## Reading an analog stick

`GetNormalizedAnalogStickInput(pad, stick)` returns a stick axis as a signed number centred on zero, roughly -128 to 127. The `stick` argument is one of `ControllerHelper::RightStickX`, `RightStickY`, `LeftStickX` or `LeftStickY`. The Y axes are flipped so pushing the stick up gives a positive number.

```cpp
constexpr auto pad = psyqo::AdvancedPad::Pad::Pad1a;
auto &controller = g_madnightEngine.m_controllerHelper;

int rx = controller.GetNormalizedAnalogStickInput(pad, ControllerHelper::RightStickX);
int ry = controller.GetNormalizedAnalogStickInput(pad, ControllerHelper::RightStickY);

if (ry != 0) camera.UpdateOrbitAngles(ry * ORBIT_SPEED, 0, deltaTime);
if (rx != 0) camera.UpdateOrbitAngles(0, rx * ORBIT_SPEED, deltaTime);
```

If the pad is not connected, or is not an analog pad, it returns `0`, so you do not need to check first.

## Deadzone

The value is the raw reading with no deadzone applied, so a stick at rest rarely reads exactly zero. Apply a threshold yourself. The header gives you starting values: `ANALOG_STICK_DEADZONE`, `ANALOG_STICK_DEADZONE_X` and `ANALOG_STICK_DEADZONE_Y`, all 16, and `ANALOG_STICK_MAX_INPUT` (127).

```cpp
int rx = controller.GetNormalizedAnalogStickInput(pad, ControllerHelper::RightStickX);
if (rx > -ANALOG_STICK_DEADZONE_X && rx < ANALOG_STICK_DEADZONE_X) rx = 0;
```

## Limits

- `init()` does nothing yet. It is where forcing a pad into analog mode would go, and nothing calls it, so a pad stays in whatever mode it powers up in. A pad in digital mode reads `0`.
- The engine's own button handling (menus, the [debug menu](./core#debugmenu)) does not check which pad an event came from. On a multitap, any connected pad can drive them.
