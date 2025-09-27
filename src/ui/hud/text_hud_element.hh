#ifndef _UI_TEXT_HUD_ELEMENT_H
#define _UI_TEXT_HUD_ELEMENT_H

#include <EASTL/fixed_string.h>
#include "psyqo/font.hh"
#include "hud_element.hh"
#include "hud_defines.hh"
#include "../../render/colour.hh"

class TextHUDElement final : public HUDElement
{
    eastl::fixed_string<char, GAMEPLAY_HUD_ELEMENT_MAX_STR_LEN> m_displayText;
    psyqo::Color m_colour = COLOUR_WHITE;
    psyqo::Font<> *m_font = nullptr;

public:
    TextHUDElement() : HUDElement("", {0, 0}) {};
    TextHUDElement(const char *name, psyqo::Rect rect) : HUDElement(name, rect) {};
    TextHUDElement(const char *name, psyqo::Rect rect, psyqo::Color colour) : TextHUDElement(name, rect) { m_colour = colour; }
    void SetFont(psyqo::Font<> *font) { m_font = font; }
    void SetDisplayText(const char *displayText) { m_displayText = displayText; }
    void SetColour(const psyqo::Color colour) { m_colour = colour; }
    void SetPositionSize(psyqo::Rect rect) { m_rect = rect; }
    void Render(const psyqo::Rect &parentRect);
    void Render(const psyqo::Rect &parentRect, psyqo::Font<> *defaultFont);
};

#endif
