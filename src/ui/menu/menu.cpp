#include "menu.hh"
#include "psyqo/xprintf.h"
#include "../../madnight.hh"
#include "../../render/renderer.hh"

void Menu::start(StartReason startReason)
{
    g_madnightEngine.m_input.setOnEvent([&](auto event)
                                        { ProcessInputs(event); });
}

void Menu::teardown(TearDownReason teardownReason)
{
    g_madnightEngine.m_input.setOnEvent(nullptr);
    m_shouldDeactivate = false;
}

void Menu::frame(void)
{
    uint32_t deltaTime = Renderer::Instance().Process();
    if (deltaTime == 0)
        return;

    if (!m_isEnabled)
        return;

    if (m_shouldDeactivate)
    {
        Deactivate();
        return;
    }

    if (m_onFrame)
        m_onFrame(deltaTime);

    for (auto &text : m_textElements)
        text.Render(m_rect);

    for (auto &sprite : m_spriteElements)
        sprite.Render(m_rect);

    uint8_t i = 0;
    for (auto &menuItem : m_menuItems)
        menuItem.Render(m_rect, m_currentSelectedMenuItem == i++);
}

void Menu::Activate()
{
    m_isEnabled = true;
    g_madnightEngine.pushScene(this);
}

void Menu::Deactivate(void)
{
    m_isEnabled = false;
    g_madnightEngine.popScene();
}

void Menu::SetControllerBindings(const MenuControllerBinds &bindings)
{
    m_keyBindings = bindings;
}

void Menu::SetCustomInputCallbackButtons(const eastl::array<psyqo::AdvancedPad::Button, 16> &customBindings)
{
    m_keyBindings.menuItemCustom = customBindings;
}

void Menu::ProcessInputs(const psyqo::AdvancedPad::Event &event)
{
    if (event.type != m_keyBindings.onEventType.type || !m_isEnabled)
        return;

    if (event.button == m_keyBindings.menuItemNext)
        MoveSelectedMenuItemNext();
    if (event.button == m_keyBindings.menuItemPrev)
        MoveSelectedMenuItemPrev();

    if (event.button == m_keyBindings.menuItemConfirm)
        m_menuItems[m_currentSelectedMenuItem].Confirm();

    if (eastl::find(m_keyBindings.menuItemCustom.begin(), m_keyBindings.menuItemCustom.end(), event.button) != m_keyBindings.menuItemCustom.end())
        m_menuItems[m_currentSelectedMenuItem].InputCallback(event.button);

    if (event.button == m_keyBindings.menuItemBackCancel)
        m_shouldDeactivate = true;
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
