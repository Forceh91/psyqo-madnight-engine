#ifndef _CONTROLLER_H
#define _CONTROLLER_H

#include "psyqo/advancedpad.hh"

class ControllerHelper final
{
public:
    enum AnalogStickIndex
    {
        RightStickX,
        RightStickY,
        LeftStickX,
        LeftStickY
    };

    static void init(void);
    static int8_t get_normalized_analog_stick_input(psyqo::AdvancedPad::Pad pad, uint8_t index);
};

#endif
