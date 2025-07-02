#include "../madnight.hh"
#include "controller.hh"

void ControllerHelper::init(void)
{
    // try and force our controller into analog mode
}

int ControllerHelper::GetNormalizedAnalogStickInput(psyqo::AdvancedPad::Pad pad, uint8_t analog_index)
{
    if (g_madnightEngine.m_input.getPadType(pad) != psyqo::AdvancedPad::PadType::AnalogPad)
        return 0;

    return static_cast<int>(g_madnightEngine.m_input.getAdc(pad, analog_index) - 0x80);
}
