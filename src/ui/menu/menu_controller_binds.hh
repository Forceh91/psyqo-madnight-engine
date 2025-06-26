#ifndef _MENU_CONTROLLER_BINDS_H
#define _MENU_CONTROLLER_BINDS_H

#include <EASTL/array.h>
#include "psyqo/advancedpad.hh"

struct MenuControllerBinds
{
    psyqo::AdvancedPad::Event onEventType;
    psyqo::AdvancedPad::Button menuItemNext;
    psyqo::AdvancedPad::Button menuItemPrev;
    psyqo::AdvancedPad::Button menuItemConfirm;
    psyqo::AdvancedPad::Button menuItemBackCancel;
    eastl::array<psyqo::AdvancedPad::Button, 16> menuItemCustom;
};

#endif
