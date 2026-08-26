#include "menu_item.hh"

void MenuItem::Render(const psyqo::Rect parentRect, const bool isSelected, psyqo::Font<100> *defaultFont)
{
    if (!m_isEnabled)
        return;

    m_text.SetColour(isSelected ? m_selectedTextColour : m_defaultTextColour);

    // TODO: handle is focused etc. etc.
    psyqo::Rect renderPos = {static_cast<int16_t>(parentRect.pos.x + m_rect.pos.x), static_cast<int16_t>(parentRect.pos.y + m_rect.pos.y), static_cast<int16_t>(parentRect.size.x + m_rect.size.w), static_cast<int16_t>(parentRect.size.y + m_rect.size.h)};
    m_text.Render(renderPos, defaultFont);
    m_sprite.Render(renderPos);
}
