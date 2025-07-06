#ifndef UI_MENU_MENU_ITEM_H
#define UI_MENU_MENU_ITEM_H

#include <EASTL/fixed_string.h>
#include <EASTL/span.h>
#include "psyqo/primitives/common.hh"
#include "psyqo/advancedpad.hh"

#include "menu_defines.hh"
#include "../../render/colour.hh"
#include "../hud/text_hud_element.hh"
#include "../hud/sprite_hud_element.hh"

class MenuItem
{
    bool m_isEnabled = true;
    eastl::fixed_string<char, MENU_MAX_NAME_LEN> m_name = "";
    eastl::function<void(void)> m_onConfirm;
    eastl::function<void(const psyqo::AdvancedPad::Button)> m_onInput;
    psyqo::Rect m_rect = {0};

    SpriteHUDElement m_sprite = SpriteHUDElement("", {0, 0});
    TextHUDElement m_text = TextHUDElement("", {0, 0});

    psyqo::Color m_defaultTextColour = COLOUR_WHITE;
    psyqo::Color m_selectedTextColour = COLOUR_YELLOW;

public:
    MenuItem(const char *name, psyqo::Rect posSizeRect)
    {
        m_name = name;
        m_rect = posSizeRect;
    };

    MenuItem(const char *name, const char *text, psyqo::Rect posSizeRect) : MenuItem(name, posSizeRect)
    {
        m_text = TextHUDElement("", {0, 0});
        m_text.SetDisplayText(text);
        m_rect = posSizeRect;
    };

    MenuItem(const char *name, const char *text, psyqo::Rect posSizeRect, psyqo::Color defaultTextColour, psyqo::Color selectedTextColour) : MenuItem(name, text, posSizeRect)
    {
        m_defaultTextColour = defaultTextColour;
        m_selectedTextColour = selectedTextColour;
    };

    ~MenuItem() = default;

    void Enable() { m_isEnabled = true; }
    void Disable() { m_isEnabled = false; }
    void Render(const psyqo::Rect parentRect, const bool isSelected);

    void SetSpriteElement(const SpriteHUDElement &sprite) { m_sprite = sprite; }
    void SetFont(psyqo::Font<> *font) { m_text.SetFont(font); }
    void SetTextElement(const TextHUDElement &text) { m_text = text; }
    void SetText(const char *text) { m_text.SetDisplayText(text); }
    void SetTextColour(const psyqo::Color colour) { m_text.SetColour(colour); }
    void SetOnConfirm(eastl::function<void(void)> callback) { m_onConfirm = eastl::move(callback); }
    // make sure that you have entries in `menuItemCustom` via `Menu::SetKeyBindings` before using this
    void SetOnInputCallback(eastl::function<void(const psyqo::AdvancedPad::Button)> callback) { m_onInput = eastl::move(callback); }
    void Confirm(void)
    {
        if (m_onConfirm)
            m_onConfirm();
    }
    void InputCallback(const psyqo::AdvancedPad::Button button)
    {
        if (m_onInput)
            m_onInput(button);
    };
};

#endif
