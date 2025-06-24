#ifndef UI_MENU_MENU_ITEM_H
#define UI_MENU_MENU_ITEM_H

#include <EASTL/fixed_string.h>
#include "psyqo/primitives/common.hh"

#include "menu_defines.hh"
#include "../../render/colour.hh"
#include "../hud/text_hud_element.hh"
#include "../hud/sprite_hud_element.hh"

class MenuItem
{
    bool m_isEnabled = true;
    eastl::fixed_string<char, MENU_MAX_NAME_LEN> m_name = "";
    psyqo::Rect m_rect = {0};

    SpriteHUDElement m_sprite = SpriteHUDElement("", {0, 0});
    TextHUDElement m_text = TextHUDElement("", {0, 0});

public:
    MenuItem(const char *name, psyqo::Rect posSizeRect)
    {
        m_name = name;
        m_rect = posSizeRect;
    };

    MenuItem(const char *name, psyqo::Rect posSizeRect, const char *text) : MenuItem(name, posSizeRect)
    {
        m_text = TextHUDElement("", {0, 0});
        m_text.SetDisplayText(text);
    };

    ~MenuItem() = default;

    void Enable() { m_isEnabled = true; }
    void Disable() { m_isEnabled = false; }
    void SetSpriteElement(const SpriteHUDElement &sprite) { m_sprite = sprite; }
    void SetTextElement(const TextHUDElement &text) { m_text = text; }
    void SetText(const char *text) { m_text.SetDisplayText(text); }
    void SetTextColour(const psyqo::Color colour) { m_text.SetColour(colour); }
    void Render(void);
};

#endif
