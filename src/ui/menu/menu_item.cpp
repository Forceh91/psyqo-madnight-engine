#include "menu_item.hh"

void MenuItem::Render(void)
{
    if (!m_isEnabled)
        return;

    // TODO: handle is focused etc. etc.
    m_text.Render(m_rect);
}
