#ifndef _UI_MENU_BASE_H
#define _UI_MENU_BASE_H

#include <EASTL/array.h>
#include <EASTL/fixed_string.h>
#include <EASTL/fixed_vector.h>
#include <EASTL/functional.h>

#include "psyqo/primitives/common.hh"
#include "psyqo/scene.hh"

#include "menu_defines.hh"
#include "menu_item.hh"
#include "menu_controller_binds.hh"
#include "../hud/text_hud_element.hh"
#include "../hud/sprite_hud_element.hh"

/*
 * this is the base class for all menus that are created via this engine
 * once activated it will create a scene on top of what was there previously
 * by default it won't clear the frame buffers and the previous render image
 * will remain. so for example using this as a pause menu would be perfectly fine
 */
class Menu : public psyqo::Scene
{
    void start(StartReason reason) override;
    void teardown(TearDownReason reason) override;
    void frame() override;

    // this is potentially redundant
    bool m_isEnabled = false;
    bool m_shouldDeactivate = false;
    eastl::fixed_string<char, MENU_MAX_NAME_LEN> m_name = "";
    psyqo::Rect m_rect = {0};

    eastl::fixed_vector<TextHUDElement, MENU_MAX_TEXT_ELEMENTS, false> m_textElements;
    eastl::fixed_vector<SpriteHUDElement, MENU_MAX_SPRITE_ELEMENTS, false> m_spriteElements;
    eastl::fixed_vector<MenuItem, MENU_MAX_MENU_ITEMS, false> m_menuItems;
    uint8_t m_currentSelectedMenuItem = 0;

    MenuControllerBinds m_keyBindings = {
        .onEventType = psyqo::AdvancedPad::Event::ButtonReleased,
        .menuItemNext = psyqo::AdvancedPad::Button::Down,
        .menuItemPrev = psyqo::AdvancedPad::Button::Up,
        .menuItemConfirm = psyqo::AdvancedPad::Button::Cross,
        .menuItemBackCancel = psyqo::AdvancedPad::Button::Triangle};

    eastl::function<void(uint32_t)> m_onFrame;
    eastl::function<void(void)> m_onActivate;
    eastl::function<void(void)> m_onDeactivate;

    void Process(void);
    void ProcessInputs(const psyqo::AdvancedPad::Event &event);

    // these are called when activate/deactivate functions are called
    // deactivate is additionally called when the backcancel button is pressed
    void OnActivate(void)
    {
        if (m_onActivate)
            m_onActivate();
    }

    void OnDeactivate(void)
    {
        if (m_onDeactivate)
            m_onDeactivate();
    }

public:
    Menu() = default;
    Menu(const char *name, psyqo::Rect posSizeRect)
    {
        m_name = name;
        m_rect = posSizeRect;
    }

    ~Menu() = default;

    bool IsEnabled(void) { return m_isEnabled; }

    // activate the menu
    void Activate(void);
    // deactivate the menu and go back to the previous scene/menu/whatever
    void Deactivate();

    // will use defaults if not called
    void SetControllerBindings(const MenuControllerBinds &bindings);

    // Optional: override only the buttons that trigger menu item input callbacks.
    // useful if you want default up/down/confirm/cancel, but custom triggers for certain items.
    // just try not to duplicate buttons already in use by default such as up/down/cross/triangle
    void SetCustomInputCallbackButtons(const eastl::array<psyqo::AdvancedPad::Button, 16> &customBindings);

    // callback each frame
    void SetOnFrame(eastl::function<void(uint32_t)> callback) { m_onFrame = eastl::move(callback); }

    // callback when menu is activated
    void SetOnActivate(eastl::function<void(void)> callback) { m_onActivate = eastl::move(callback); }

    // callback when menu is deactivated
    void SetOnDeactivate(eastl::function<void(void)> callback) { m_onDeactivate = eastl::move(callback); }

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

    uint8_t MoveSelectedMenuItemPrev()
    {
        if (!m_isEnabled)
            return m_currentSelectedMenuItem;

        m_currentSelectedMenuItem = (m_currentSelectedMenuItem == 0) ? m_menuItems.size() - 1 : m_currentSelectedMenuItem - 1;
        return m_currentSelectedMenuItem;
    }

    uint8_t MoveSelectedMenuItemNext()
    {
        if (!m_isEnabled)
            return m_currentSelectedMenuItem;

        m_currentSelectedMenuItem = (m_currentSelectedMenuItem + 1) % m_menuItems.size();
        return m_currentSelectedMenuItem;
    }
};

#endif
