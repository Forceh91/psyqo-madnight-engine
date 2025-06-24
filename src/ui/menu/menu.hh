#ifndef _UI_MENU_BASE_H
#define _UI_MENU_BASE_H

#include <EASTL/array.h>
#include <EASTL/fixed_string.h>
#include <EASTL/fixed_vector.h>
#include "psyqo/primitives/common.hh"

#include "menu_defines.hh"
#include "menu_item.hh"
#include "../hud/text_hud_element.hh"
#include "../hud/sprite_hud_element.hh"

/*
 * this is the base class for all meuns that are created via this engine
 * you shouldn't access this class directly, and instead use GameplayMenu
 * and SceneMenu. more info on those in the respective classes
 */
class Menu
{
    bool m_isEnabled = true;
    eastl::fixed_string<char, MENU_MAX_NAME_LEN> m_name = "";
    psyqo::Rect m_rect = {0};

    eastl::fixed_vector<TextHUDElement, MENU_MAX_TEXT_ELEMENTS, false> m_textElements;
    eastl::fixed_vector<SpriteHUDElement, MENU_MAX_SPRITE_ELEMENTS, false> m_spriteElements;
    eastl::fixed_vector<MenuItem, MENU_MAX_MENU_ITEMS, false> m_menuItems;
    uint8_t m_currentSelectedMenuItem = 0;

public:
    Menu() = default;
    Menu(const char *name, psyqo::Rect posSizeRect)
    {
        m_name = name;
        m_rect = posSizeRect;
    }

    ~Menu() = default;

    void Enable() { m_isEnabled = true; }
    void Disable() { m_isEnabled = false; }
    void Render(void);

    // dont lose track of the hud element!
    TextHUDElement *AddTextHUDElement(TextHUDElement &&textElement)
    {
        m_textElements.push_back(eastl::move(textElement));
        return &m_textElements.back();
    }

    void RemoveTextHUDElement(TextHUDElement *element)
    {
        auto it = eastl::find_if(m_textElements.begin(), m_textElements.end(), [element](TextHUDElement &el)
                                 { return &el == element; });
        if (it != m_textElements.end())
            m_textElements.erase(it);
    }

    // dont lose track of the hud element!
    SpriteHUDElement *AddSpriteHUDElement(SpriteHUDElement &&spriteElement)
    {
        m_spriteElements.push_back(eastl::move(spriteElement));
        return &m_spriteElements.back();
    }

    void RemoveSpriteHUDElement(SpriteHUDElement *element)
    {
        auto it = eastl::find_if(m_spriteElements.begin(), m_spriteElements.end(), [element](SpriteHUDElement &el)
                                 { return &el == element; });
        if (it != m_spriteElements.end())
            m_spriteElements.erase(it);
    }

    MenuItem *AddMenuItem(const MenuItem &item);
    MenuItem *AddMenuItem(const char *name, const char *displayText, const psyqo::Rect posSize);
    void AddMenuItems(const eastl::span<MenuItem> &items);
};

#endif
