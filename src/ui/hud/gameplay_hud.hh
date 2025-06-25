#ifndef _UI_HUD_GAMEPLAY_H
#define _UI_HUD_GAMEPLAY_H

#include <EASTL/fixed_string.h>
#include <EASTL/vector.h>
#include <EASTL/fixed_vector.h>
#include "psyqo/vector.hh"
#include "psyqo/xprintf.h"

#include "hud_defines.hh"
#include "text_hud_element.hh"
#include "sprite_hud_element.hh"

/*
 * this is designed for you to create a dumb hud on top of your gameplay
 * it can update things like health, lives, a timer, etc. etc., stuff like that
 * but what it can't do is accept inputs to do different things like selecting options,
 * you would want a `Menu` screen for that.
 * realistically you only have one of these on the screen at one time, however,
 * its entirely possible that you want to switch between a few different ones
 * during gameplay. You don't have to hide others to do so but lets be real,
 * you probably should do...
 */
class GameplayHUD final
{
    bool m_isEnabled = false;
    eastl::fixed_string<char, GAMEPLAY_HUD_MAX_NAME_LEN> m_name;
    psyqo::Rect m_rect = {0};
    eastl::fixed_vector<TextHUDElement, 20, false> m_textHUDElements;
    eastl::fixed_vector<SpriteHUDElement, 40, false> m_spriteHUDElements;

public:
    GameplayHUD()
    {
        m_name = "";
        m_rect = {0};
    }

    GameplayHUD(const char *name, psyqo::Rect rect)
    {
        m_name = name;
        m_rect = rect;
        m_isEnabled = true;
    }

    ~GameplayHUD() = default;

    void Enable() { m_isEnabled = true; }
    void Disable() { m_isEnabled = false; }
    void Render(void);

    // dont lose track of the hud element!
    // has a limit of 20 elements for now, i don't see why you would want more than that
    TextHUDElement *AddTextHUDElement(TextHUDElement &&textElement)
    {
        m_textHUDElements.push_back(eastl::move(textElement));
        return &m_textHUDElements.back();
    }

    void RemoveTextHUDElement(TextHUDElement *element)
    {
        auto it = eastl::find_if(m_textHUDElements.begin(), m_textHUDElements.end(), [element](TextHUDElement &el)
                                 { return &el == element; });
        if (it != m_textHUDElements.end())
            m_textHUDElements.erase(it);
    }

    // dont lose track of the hud element!
    // has a limit of 40 elements for now, i don't see why you would want more than that
    SpriteHUDElement *AddSpriteHUDElement(SpriteHUDElement &&spriteElement)
    {
        m_spriteHUDElements.push_back(eastl::move(spriteElement));
        return &m_spriteHUDElements.back();
    }

    void RemoveSpriteHUDElement(SpriteHUDElement *element)
    {
        auto it = eastl::find_if(m_spriteHUDElements.begin(), m_spriteHUDElements.end(), [element](SpriteHUDElement &el)
                                 { return &el == element; });
        if (it != m_spriteHUDElements.end())
            m_spriteHUDElements.erase(it);
    }
};

#endif
