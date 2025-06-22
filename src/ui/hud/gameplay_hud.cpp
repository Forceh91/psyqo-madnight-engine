#include "gameplay_hud.hh"
#include "text_hud_element.hh"

void GameplayHUD::Render(void)
{
    // make sure its enabled
    if (!m_isEnabled)
        return;

    // for each hud element, draw to the screen...
    for (auto &element : m_textHUDElements)
    {
        if (!element.name().empty())
            element.Render(m_rect);
    }
}
