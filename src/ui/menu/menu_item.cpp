#include "menu_item.hh"

void MenuItem::Render(const psyqo::Rect parentRect, const bool isSelected)
{
    if (!m_isEnabled)
        return;

    m_text.SetColour(isSelected ? m_selectedTextColour : m_defaultTextColour);

    // TODO: handle is focused etc. etc.
    psyqo::Rect renderPos = {parentRect.pos.x + m_rect.pos.x, parentRect.pos.y + m_rect.pos.y, parentRect.size.x + m_rect.size.y};
    m_text.Render(renderPos);
}
