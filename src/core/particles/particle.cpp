/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "particle.hh"
#include "../../math/vector.hh"
#include "psyqo/fixed-point.hh"

using namespace psyqo::fixed_point_literals;

void Particle::Process(const uint32_t& deltaTime) {
	// guard against a particle having no lifetime somehow
	if (m_lifetime < 1 || m_age >= m_lifetimeMicroSeconds) {
		return;
	}

	m_age += deltaTime;

	auto ageMS = static_cast<int32_t>(m_age / 1000);
	auto lifetimeMS = m_lifetimeMicroSeconds / 1000;

	auto lifetimeLerp =
		lifetimeMS > 0 ? psyqo::FixedPoint<>(int32_t(ageMS * 4096 / lifetimeMS), psyqo::FixedPoint<>::RAW) : 0.0_fp;
	auto r = (m_startColour.r * (1.0_fp - lifetimeLerp) + m_endColour.r * lifetimeLerp).value >> 12;
	auto g = (m_startColour.g * (1.0_fp - lifetimeLerp) + m_endColour.g * lifetimeLerp).value >> 12;
	auto b = (m_startColour.b * (1.0_fp - lifetimeLerp) + m_endColour.b * lifetimeLerp).value >> 12;

	psyqo::Color lerpedColour = {static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b)};
	SetColour(lerpedColour);

	auto lerpedSize = Lerp(m_startSize, m_endSize, lifetimeLerp);
	setSize(lerpedSize);

	// position from age, not velocity * deltaTime per frame (that truncates to 0 for slow particles at short frames).
	// distance = average of start and current velocity * age: / 2 for the average, / 1000 for ms -> s
	// fits an int32 for velocities under ~32_ws per second at the max 255s lifetime
	auto lerpedVelocity = Lerp(m_startVelocity, m_endVelocity, lifetimeLerp);
	auto displacement = [ageMS](const psyqo::FixedPoint<>& start, const psyqo::FixedPoint<>& current) {
		return psyqo::FixedPoint<>((start.value + current.value) * ageMS / 2000, psyqo::FixedPoint<>::RAW);
	};

	SetPosition(m_spawnPos + psyqo::Vec3{displacement(m_startVelocity.x, lerpedVelocity.x),
										 displacement(m_startVelocity.y, lerpedVelocity.y),
										 displacement(m_startVelocity.z, lerpedVelocity.z)});
}
