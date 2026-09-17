/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once
#include <EASTL/array.h>
#include <EASTL/fixed_string.h>
#include <EASTL/fixed_vector.h>
#include <EASTL/functional.h>

#include "EASTL/algorithm.h"
#include "psyqo/primitives/common.hh"
#include "psyqo/scene.hh"

#include "../hud/sprite_hud_element.hh"
#include "../hud/text_hud_element.hh"
#include "menu_controller_binds.hh"
#include "menu_defines.hh"
#include "menu_item.hh"

/*
 * this is the base class for all menus that are created via this engine
 * once activated it will create a scene on top of what was there previously
 * by default it won't clear the frame buffers and the previous render image
 * will remain. so for example using this as a pause menu would be perfectly fine
 */
class Menu : public psyqo::Scene {
	void start(StartReason reason) override;
	void teardown(TearDownReason reason) override;
	void frame() override;

	// this is potentially redundant
	bool m_isEnabled = false;
	bool m_shouldDisable = false;
	eastl::fixed_string<char, MENU_MAX_NAME_LEN> m_name = "";
	psyqo::Rect m_rect = {0};
	psyqo::Font<100>* m_defaultFont = nullptr;

	eastl::fixed_vector<TextHUDElement, MENU_MAX_TEXT_ELEMENTS, false> m_textElements;
	eastl::fixed_vector<SpriteHUDElement, MENU_MAX_SPRITE_ELEMENTS, false> m_spriteElements;
	eastl::fixed_vector<MenuItem, MENU_MAX_MENU_ITEMS, false> m_menuItems;
	uint8_t m_currentSelectedMenuItem = 0;

	MenuControllerBinds m_keyBindings = {.onEventType = psyqo::AdvancedPad::Event::ButtonReleased,
										 .menuItemNext = psyqo::AdvancedPad::Button::Down,
										 .menuItemPrev = psyqo::AdvancedPad::Button::Up,
										 .menuItemConfirm = psyqo::AdvancedPad::Button::Cross,
										 .menuItemBackCancel = psyqo::AdvancedPad::Button::Triangle};

	// callback when `frame` is called, will not callback if delta time is 0
	eastl::function<void(uint32_t)> m_onFrame;
	eastl::function<void(void)> m_onEnable;
	eastl::function<void(void)> m_onDisable;
	eastl::function<void(void)> m_onDestroy;

	void Process(void);
	void ProcessInputs(const psyqo::AdvancedPad::Event& event);

	// these are called when activate/deactivate functions are called
	// deactivate is additionally called when the backcancel button is pressed
	void OnActivate(void) {
		if (m_onEnable) {
			m_onEnable();
		}
	}

	void OnDeactivate(void) {
		if (m_onDisable) {
			m_onDisable();
		}
	}

	void OnDestroy(void) {
		if (m_onDestroy) {
			m_onDestroy();
		}
	}

	uint8_t MoveSelectedMenuItemPrev() {
		if (!m_isEnabled || !m_menuItems.size()) {
			return m_currentSelectedMenuItem;
		}

		m_currentSelectedMenuItem =
			(m_currentSelectedMenuItem == 0) ? m_menuItems.size() - 1 : m_currentSelectedMenuItem - 1;
		return m_currentSelectedMenuItem;
	}

	uint8_t MoveSelectedMenuItemNext() {
		if (!m_isEnabled || !m_menuItems.size()) {
			return m_currentSelectedMenuItem;
		}

		m_currentSelectedMenuItem = (m_currentSelectedMenuItem + 1) % m_menuItems.size();
		return m_currentSelectedMenuItem;
	}

  public:
	Menu() = default;
	Menu(const eastl::string_view& name, psyqo::Rect posSizeRect) : m_name(name.data(), name.length()) {
		m_rect = posSizeRect;
	}

	~Menu() = default;

	constexpr bool IsEnabled(void) { return m_isEnabled; }

	// activate the menu
	void Enable(void);
	// deactivate the menu and go back to the previous scene/menu/whatever
	void Disable(void);

	// deactive the menu and destroy everything it was holding
	void Destroy(void);

	// will use defaults if not called
	void SetControllerBindings(const MenuControllerBinds& bindings);

	// Optional: override only the buttons that trigger menu item input callbacks.
	// useful if you want default up/down/confirm/cancel, but custom triggers for certain items.
	// just try not to duplicate buttons already in use by default such as up/down/cross/triangle
	void SetCustomInputCallbackButtons(const eastl::array<psyqo::AdvancedPad::Button, 16>& customBindings);

	// callback each frame
	void SetOnFrame(eastl::function<void(uint32_t)> callback) { m_onFrame = eastl::move(callback); }

	// callback when menu is activated
	void SetOnEnable(eastl::function<void(void)> callback) { m_onEnable = eastl::move(callback); }

	// callback when menu is deactivated
	void SetOnDisable(eastl::function<void(void)> callback) { m_onDisable = eastl::move(callback); }

	// callback when menu is destroyed
	void SetOnDestroy(eastl::function<void(void)> callback) { m_onDestroy = eastl::move(callback); }

	// dont lose track of the hud element!
	TextHUDElement* AddTextHUDElement(TextHUDElement&& textElement) {
		m_textElements.push_back(eastl::move(textElement));
		return &m_textElements.back();
	}

	void RemoveTextHUDElement(TextHUDElement* element) {
		auto it = eastl::find_if(m_textElements.begin(), m_textElements.end(),
								 [element](TextHUDElement& el) { return &el == element; });
		if (it != m_textElements.end()) {
			m_textElements.erase(it);
		}
	}

	// dont lose track of the hud element!
	SpriteHUDElement* AddSpriteHUDElement(SpriteHUDElement&& spriteElement) {
		m_spriteElements.push_back(eastl::move(spriteElement));
		return &m_spriteElements.back();
	}

	void RemoveSpriteHUDElement(SpriteHUDElement* element) {
		auto it = eastl::find_if(m_spriteElements.begin(), m_spriteElements.end(),
								 [element](SpriteHUDElement& el) { return &el == element; });
		if (it != m_spriteElements.end()) {
			m_spriteElements.erase(it);
		}
	}

	MenuItem* AddMenuItem(const MenuItem& item);
	MenuItem* AddMenuItem(const eastl::string_view& name, const eastl::string_view& displayText,
						  const psyqo::Rect posSize);
	void AddMenuItems(const eastl::span<MenuItem>& items);
	void SetDefaultFont(psyqo::Font<100>* font) { m_defaultFont = font; }
	void SetSelectedMenuItem(uint8_t ix) {
		ix = eastl::clamp<uint8_t>(ix, 0, m_menuItems.size() - 1);
		m_currentSelectedMenuItem = ix;
	}
};
