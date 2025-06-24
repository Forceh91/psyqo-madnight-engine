#include "menu.hh"

void Menu::Render(void)
{
    if (!m_isEnabled)
        return;

    for (auto &text : m_textElements)
        text.Render(m_rect);

    for (auto &sprite : m_spriteElements)
        sprite.Render(m_rect);

    uint8_t i = 0;
    for (auto &menuItem : m_menuItems)
    {
        menuItem.Render(m_rect, m_currentSelectedMenuItem == i++);
    }
}

MenuItem *Menu::AddMenuItem(const char *name, const char *displayText, const psyqo::Rect posSize)
{
    m_menuItems.push_back(MenuItem(name, displayText, posSize));
    return &m_menuItems.back();
}

MenuItem *Menu::AddMenuItem(const MenuItem &item)
{
    m_menuItems.push_back(eastl::move(item));
    return &m_menuItems.back();
};

void Menu::AddMenuItems(const eastl::span<MenuItem> &items)
{
    for (const auto &item : items)
        m_menuItems.push_back(item);
}
