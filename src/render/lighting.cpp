/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "lighting.hh"
#include "../math/vector.hh"
#include "lights/point_light.hh"

#include <EASTL/fixed_vector.h>

#include "psyqo/fixed-point.hh"

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

eastl::fixed_vector<PointLight, MAX_POINT_LIGHTS_PER_OBJECT, false>
Lighting::FindPointLightsForPos(const psyqo::Vec3& pos) {
	eastl::fixed_vector<PointLight, MAX_POINT_LIGHTS_PER_OBJECT, false> out;
	eastl::fixed_vector<psyqo::FixedPoint<>, MAX_POINT_LIGHTS_PER_OBJECT, false> closestLights;

	for (const auto& light : m_pointLights) {
		// no point going further if the diff is bigger than the lights radius
		auto posDiff = light.pos - pos;
		if (posDiff.x.abs() > light.radius || posDiff.y.abs() > light.radius || posDiff.z.abs() > light.radius) {
			continue;
		}

		// now for the final check to see if its within radius
		auto posSq = DotProduct(posDiff, posDiff);
		auto radiusSq = light.radius * light.radius;
		if (posSq > radiusSq) {
			continue;
		}

		// not filled the list yet so we can push straight in
		if (!out.full()) {
			out.push_back(light);
			closestLights.push_back(posSq);
		} else {
			// otherwise we need to remove something once we know which the furthest away element is
			auto biggestPosSq = eastl::max_element(closestLights.begin(), closestLights.end());
			auto ix = biggestPosSq - closestLights.begin();

			// if this new possq is less than the largest known possq, replace it
			if (posSq < *biggestPosSq) {
				closestLights[ix] = posSq;
				out[ix] = light;
			}
		}
	}

	return out;
}
