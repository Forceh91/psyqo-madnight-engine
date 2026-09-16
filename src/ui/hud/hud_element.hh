/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "hud_defines.hh"
#include <EASTL/fixed_string.h>
#include <psyqo/vector.hh>

class HUDElement {
  protected:
	bool m_isEnabled = false;
	eastl::fixed_string<char, GAMEPLAY_HUD_MAX_NAME_LEN> m_name;
	psyqo::Rect m_rect = {0, 0, 0, 0};

  public:
	HUDElement(const eastl::string_view& name, psyqo::Rect rect) : m_name(name.data(), name.length()) {
		m_rect = rect;
		m_isEnabled = true;
	}

	~HUDElement() = default;

	void Enable() { m_isEnabled = true; }
	void Disable() { m_isEnabled = false; }
	eastl::fixed_string<char, GAMEPLAY_HUD_MAX_NAME_LEN>& name() { return m_name; }
};
