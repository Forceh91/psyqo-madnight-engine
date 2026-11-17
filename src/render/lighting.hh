/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "lights/point_light.hh"

#include <EASTL/fixed_vector.h>
#include <EASTL/span.h>

#include <psyqo/primitives/common.hh>

static constexpr psyqo::Color DEFAULT_CLEAR_COLOR = {.r = 0, .g = 0, .b = 0};
static constexpr uint8_t MAX_POINT_LIGHTS = 8; // does this need to be shared across other types of lights? perhaps. we
											   // could do a templated array in the future?

class Lighting {
  public:
	static Lighting& instance() {
		static Lighting s_instance;
		return s_instance;
	}

	const constexpr bool& IsSimpleFogEnabled(void) const { return m_isSimpleFogEnabled; }
	// NOTE: turning this on at the same time as advanced fog has no effect, advanced fog will overpower it
	void EnableSimpleFog(void) { m_isSimpleFogEnabled = true; }
	void DisableSimpleFog(void) { m_isSimpleFogEnabled = false; }

	const constexpr bool& IsAdvancedFogEnabled(void) const { return m_isAdvancedFogEnabled; }
	void EnableAdvancedFog(void) { m_isAdvancedFogEnabled = true; }
	void DisableAdvancedFog(void) { m_isAdvancedFogEnabled = false; }

	const constexpr psyqo::Color& GetAmbientColour(void) const { return m_ambient; }
	void SetAmbientColour(psyqo::Color colour) { m_ambient = colour; }

	const constexpr psyqo::Color& GetFogColour(void) const { return m_fogColour; }
	void SetFogColour(psyqo::Color colour) { m_fogColour = colour; }

	const constexpr eastl::span<PointLight> GetPointLights(void) {
		return {m_pointLights.data(), m_pointLights.size()};
	}

	bool CreatePointLight(const psyqo::Vec3& pos, const psyqo::FixedPoint<>& radius, const psyqo::Color& colour,
						  PointLight* pointLightOut);
	bool DestroyPointLight(const PointLight* pointLight);

  private:
	Lighting() = default;

	bool m_isSimpleFogEnabled = false;
	bool m_isAdvancedFogEnabled = false;

	// clear colour IS fog colour
	psyqo::Color m_fogColour = DEFAULT_CLEAR_COLOR;
	psyqo::Color m_ambient = {128, 128, 128};

	// light types
	eastl::fixed_vector<PointLight, MAX_POINT_LIGHTS> m_pointLights = {};
};
