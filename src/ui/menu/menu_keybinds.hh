#ifndef _MENU_KEYBINDS_H
#define _MENU_KEYBINDS_H

#include "psyqo/advancedpad.hh"

struct MenuKeyBinds
{
    psyqo::AdvancedPad::Event onEventType;
    psyqo::AdvancedPad::Button menuItemNext;
    psyqo::AdvancedPad::Button menuItemPrev;
    psyqo::AdvancedPad::Button menuItemConfirm;
    psyqo::AdvancedPad::Button menuItemBackCancel;
};

#endif
