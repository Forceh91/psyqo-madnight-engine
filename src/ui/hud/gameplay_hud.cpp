#include "gameplay_hud.hh"

void GameplayHUD::Render(void)
{
    // make sure its enabled
    if (!m_isEnabled)
        return;

    // for each text hud element, draw to the screen...
    for (auto &element : m_textHUDElements)
    {
        if (!element.name().empty())
            element.Render(m_rect);
    }

    // for each sprite hud element, draw to the screen
    for (auto &element : m_spriteHUDElements)
    {
        if (!element.name().empty())
            element.Render(m_rect);
    }
}
