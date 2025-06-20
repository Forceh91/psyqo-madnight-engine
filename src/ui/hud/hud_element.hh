#ifndef _UI_HUD_ELEMENT_H
#define _UI_HUD_ELEMENT_H

#include <EASTL/fixed_string.h>
#include "psyqo/vector.hh"
#include "hud_defines.hh"

class HUDElement
{
protected:
    bool m_isEnabled = false;
    eastl::fixed_string<char, GAMEPLAY_HUD_MAX_NAME_LEN> m_name;
    psyqo::Rect m_rect = {0};

public:
    HUDElement(const char *name, psyqo::Rect rect)
    {
        m_name = name;
        m_rect = rect;
        m_isEnabled = true;
    }

    ~HUDElement() = default;

    void Enable() { m_isEnabled = true; }
    void Disable() { m_isEnabled = false; }
    eastl::fixed_string<char, GAMEPLAY_HUD_MAX_NAME_LEN> &Name() { return m_name; }
};

#endif
