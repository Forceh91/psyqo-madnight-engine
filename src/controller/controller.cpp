#include "../hello3d.hh"
#include "controller.hh"

void ControllerHelper::init(void)
{
    // try and force our controller into analog mode
}

int8_t ControllerHelper::get_normalized_analog_stick_input(psyqo::AdvancedPad::Pad pad, uint8_t analog_index)
{
    if (g_madnightEngine.m_input.getPadType(pad) != psyqo::AdvancedPad::PadType::AnalogPad)
        return 0;

    return static_cast<int8_t>(g_madnightEngine.m_input.getAdc(pad, analog_index) - 0x80);
}
