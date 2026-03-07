#include "controller.hh"
#include "../madnight.hh"

void ControllerHelper::init(void) {
  // try and force our controller into analog mode
}

int ControllerHelper::GetNormalizedAnalogStickInput(psyqo::AdvancedPad::Pad pad, uint8_t analog_index) {
  if (!IsPadAnalog(pad))
    return 0;

  auto val = static_cast<int>(g_madnightEngine.m_input.getAdc(pad, analog_index) - 0x80);
  return analog_index == LeftStickY || analog_index == RightStickY ? -val : val; // normalize y axis so that a positive value is up on the stick
}

bool ControllerHelper::IsPadAnalog(psyqo::AdvancedPad::Pad pad) {
  return g_madnightEngine.m_input.getPadType(psyqo::AdvancedPad::Pad::Pad1a) == psyqo::AdvancedPad::PadType::AnalogPad;
}
