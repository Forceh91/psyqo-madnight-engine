/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <psyqo/primitives/common.hh>

static constexpr psyqo::Color DEFAULT_CLEAR_COLOR = {.r = 0, .g = 0, .b = 0};

class Lighting {
  public:
	static Lighting& instance() {
		static Lighting s_instance;
		return s_instance;
	}

	const constexpr bool& IsSimpleFogEnabled(void) const { return m_isSimpleFogEnabled; }
	void EnableSimpleFog(void) { m_isSimpleFogEnabled = true; }
	void DisableSimpleFog(void) { m_isSimpleFogEnabled = false; }

	const constexpr psyqo::Color& GetAmbientColour(void) const { return m_ambient; }
	void SetAmbientColour(psyqo::Color colour) { m_ambient = colour; }

	const constexpr psyqo::Color& GetFogColour(void) const { return m_fogColour; }
	void SetFogColour(psyqo::Color colour) { m_fogColour = colour; }

  private:
	Lighting() = default;

	bool m_isSimpleFogEnabled = false;
	// clear colour IS fog colour
	psyqo::Color m_fogColour = DEFAULT_CLEAR_COLOR;
	psyqo::Color m_ambient = {128, 128, 128};
};
