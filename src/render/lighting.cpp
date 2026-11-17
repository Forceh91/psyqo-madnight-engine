/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "lighting.hh"
#include "lights/point_light.hh"

bool Lighting::CreatePointLight(const psyqo::Vec3& pos, const psyqo::FixedPoint<>& radius, const psyqo::Color& colour,
								PointLight* pointLightOut) {

	pointLightOut = nullptr;

	if (m_pointLights.full()) {
		return false;
	}

	pointLightOut = &m_pointLights.emplace_back(PointLight{pos, radius, colour});
	return true;
}

bool Lighting::DestroyPointLight(const PointLight* light) {
	if (m_pointLights.empty()) {
		return false;
	}

	auto sizeBefore = m_pointLights.size();
	eastl::erase_if(m_pointLights, [light](const PointLight& l) { return &l == light; });

	return m_pointLights.size() < sizeBefore;
}
