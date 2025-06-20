#ifndef _UI_TEXT_HUD_ELEMENT_H
#define _UI_TEXT_HUD_ELEMENT_H

#include <EASTL/fixed_string.h>
#include "hud_element.hh"
#include "hud_defines.hh"

class TextHUDElement final : public HUDElement
{
    eastl::fixed_string<char, GAMEPLAY_HUD_ELEMENT_MAX_STR_LEN> m_displayText;

public:
    TextHUDElement(const char *name, psyqo::Rect rect) : HUDElement(name, rect) {};
    void SetDisplayText(const char *displayText) { m_displayText = displayText; }
    void Render(const psyqo::Rect &parentRect);
};

#endif
