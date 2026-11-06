/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "particle_emitter.hh"
#include "../../madnight.hh"
#include "../../render/renderer.hh"
#include "EASTL/algorithm.h"
#include "particle.hh"

#include <psyqo/gte-math.hh>
#include <psyqo/soft-math.hh>

using namespace psyqo::fixed_point_literals;

void ParticleEmitter::Start(void) { m_isEnabled = true; }

void ParticleEmitter::Stop(void) { m_isEnabled = false; }

void ParticleEmitter::Destroy(void) {
	m_isEnabled = false;
	m_nameHash = 0;
	m_id = INVALID_POOL_ID;
	m_pos = {0, 0, 0};
	m_rotatedPos = {0, 0, 0};
	m_rotation = {0, 0, 0};
	m_rotationMatrix = {0};
	m_radius = 0;
	m_maxParticles = 0;
	m_particlesPerSecond = 0;
	m_spawnedParticles.clear();
	m_spawnRate = 0;
	m_timeSinceLastParticleSpawn = 0;
	m_timeOfLastProcess = 0;

	m_particleStartColour = {0, 0, 0};
	m_particleEndColour = {0, 0, 0};
	m_particleStartSize = {0, 0};
	m_particleEndSize = {0, 0};
	m_particleStartVelocity = {0, 0, 0};
	m_particleRotatedStartVelocity = {0, 0, 0};
	m_particleEndVelocity = {0, 0, 0};
	m_particleRotatedEndVelocity = {0, 0, 0};
	m_particleVelocitySpread = {0, 0, 0};
	m_particleLifeTime = 0;
	m_particleUVCoords = {};
	m_particleTexture = nullptr;
	m_particleIs2D = true;
	m_spawnInsideRadius = false;
}

psyqo::Vec2 ParticleEmitter::GenerateRandomPointOnCircumfrence(void) {
	auto angle = g_madnightEngine.m_rand.rand<360>() * psyqo::Angle(3.14 * 2);
	return {
		g_madnightEngine.m_trig.cos(angle) * m_radius,
		g_madnightEngine.m_trig.sin(angle) * m_radius,
	};
}

psyqo::Vec2 ParticleEmitter::GenerateRandomPointWithinRadius(void) {
	auto& rand = g_madnightEngine.m_rand;
	psyqo::FixedPoint<> radiusScale = 0;
	radiusScale.value = eastl::max(rand.rand<4096>(), rand.rand<4096>());
	auto radius = m_radius * radiusScale;

	psyqo::Angle angle = 0;
	angle.value = rand.rand<2048>();

	return {
		g_madnightEngine.m_trig.cos(angle) * radius,
		g_madnightEngine.m_trig.sin(angle) * radius,
	};
}

void ParticleEmitter::Process(const uint32_t& deltaTime) {
	auto now = Renderer::Instance().GPU().now();
	auto delta = now - m_timeOfLastProcess;

	m_timeSinceLastParticleSpawn += delta;

	// process active particles
	for (auto& particle : m_spawnedParticles) {
		if (!particle.IsDead()) {
			particle.Process(delta);
		}
	}

	// clear out dead ones
	eastl::erase_if(m_spawnedParticles, [](const Particle& p) { return p.IsDead(); });

	// make sure we're enabled
	if (!m_isEnabled) {
		return;
	}

	// make sure its been a second and we dont have too many spawned
	m_timeOfLastProcess = now;
	while (m_timeSinceLastParticleSpawn >= m_spawnRate && m_spawnedParticles.size() < m_maxParticles) {
		// generate a particle at a random point on the circumfrence
		psyqo::Vec2 pos = {0, 0};
		if (m_spawnInsideRadius) {
			pos = GenerateRandomPointWithinRadius();
		} else {
			pos = GenerateRandomPointOnCircumfrence();
		}

		auto spawnPos = m_rotatedPos + psyqo::Vec3{pos.x, 0, pos.y};
		auto spreadStartVelocity = m_particleRotatedStartVelocity, spreadEndVelocity = m_particleRotatedEndVelocity;

		// apply a random spread if we want
		auto startSpreadX =
			g_madnightEngine.m_rand.randInc(-m_particleVelocitySpread.x.value, m_particleVelocitySpread.x.value);
		spreadStartVelocity.x += psyqo::FixedPoint<>(startSpreadX, psyqo::FixedPoint<>::RAW);

		auto startSpreadY =
			g_madnightEngine.m_rand.randInc(-m_particleVelocitySpread.y.value, m_particleVelocitySpread.y.value);
		spreadStartVelocity.y += psyqo::FixedPoint<>(startSpreadY, psyqo::FixedPoint<>::RAW);

		auto startSpreadZ =
			g_madnightEngine.m_rand.randInc(-m_particleVelocitySpread.z.value, m_particleVelocitySpread.z.value);
		spreadStartVelocity.z += psyqo::FixedPoint<>(startSpreadZ, psyqo::FixedPoint<>::RAW);

		auto endSpreadX =
			g_madnightEngine.m_rand.randInc(-m_particleVelocitySpread.x.value, m_particleVelocitySpread.x.value);
		spreadEndVelocity.x += psyqo::FixedPoint<>(endSpreadX, psyqo::FixedPoint<>::RAW);

		auto endSpreadY =
			g_madnightEngine.m_rand.randInc(-m_particleVelocitySpread.y.value, m_particleVelocitySpread.y.value);
		spreadEndVelocity.y += psyqo::FixedPoint<>(endSpreadY, psyqo::FixedPoint<>::RAW);

		auto endSpreadZ =
			g_madnightEngine.m_rand.randInc(-m_particleVelocitySpread.z.value, m_particleVelocitySpread.z.value);
		spreadEndVelocity.z += psyqo::FixedPoint<>(endSpreadZ, psyqo::FixedPoint<>::RAW);

		auto particle = Particle(spawnPos, m_particleStartSize, m_particleEndSize, m_particleStartColour,
								 m_particleEndColour, spreadStartVelocity, spreadEndVelocity, m_particleLifeTime);

		if (m_particleTexture) {
			particle.SetUVCoords(m_particleUVCoords);
		}

		m_spawnedParticles.push_back(particle);
		m_timeSinceLastParticleSpawn -= m_spawnRate;
	}

	m_timeSinceLastParticleSpawn =
		eastl::clamp(m_timeSinceLastParticleSpawn, m_spawnRate, m_timeSinceLastParticleSpawn);
}

void ParticleEmitter::SetParticleVelocity(const psyqo::Vec3& particleVelocity) {
	SetParticleVelocity(particleVelocity, particleVelocity);
}

void ParticleEmitter::SetParticleVelocity(const psyqo::Vec3& particleVelocity, const psyqo::Vec3& particleEndVelocity) {
	m_particleStartVelocity = particleVelocity;
	m_particleEndVelocity = particleEndVelocity;

	GenerateRotatedVelocity();
}

void ParticleEmitter::SetParticleVelocitySpread(const psyqo::Vec3& spread) { m_particleVelocitySpread = spread; }

void ParticleEmitter::SetParticleSize(const psyqo::Vec2& particleSize) { SetParticleSize(particleSize, particleSize); }

void ParticleEmitter::SetParticleSize(const psyqo::Vec2& particleSize, const psyqo::Vec2& particleEndSize) {
	m_particleStartSize = particleSize;
	m_particleEndSize = particleEndSize;
}

void ParticleEmitter::SetParticleColour(const psyqo::Color& particleColour) {
	SetParticleColour(particleColour, particleColour);
}

void ParticleEmitter::SetParticleColour(const psyqo::Color& particleColour, const psyqo::Color& particleEndColour) {
	m_particleStartColour = particleColour;
	m_particleEndColour = particleEndColour;
}

void ParticleEmitter::SetParticleTexture(const eastl::string_view& textureName,
										 const eastl::array<psyqo::PrimPieces::UVCoords, 4>& uv) {
	g_madnightEngine.m_textureManager.GetTextureFromName(textureName, &m_particleTexture);
	m_particleUVCoords = uv;
}

void ParticleEmitter::SetParticleUVCoords(const eastl::array<psyqo::PrimPieces::UVCoords, 4>& uv) {
	m_particleUVCoords = uv;
}

void ParticleEmitter::SetParticles2D(const bool& is2D) { m_particleIs2D = is2D; }

void ParticleEmitter::SetSpawnInsideRadius(const bool& withinRadius) { m_spawnInsideRadius = withinRadius; }

void ParticleEmitter::SetRotation(const EmitterRotation& rotation) {
	m_rotation = rotation;
	GenerateRotationMatrix();
}

void ParticleEmitter::GenerateRotationMatrix(void) {
	auto roll =
		psyqo::SoftMath::generateRotationMatrix33(m_rotation.x, psyqo::SoftMath::Axis::X, g_madnightEngine.m_trig);
	auto pitch =
		psyqo::SoftMath::generateRotationMatrix33(m_rotation.y, psyqo::SoftMath::Axis::Y, g_madnightEngine.m_trig);
	auto yaw =
		psyqo::SoftMath::generateRotationMatrix33(m_rotation.z, psyqo::SoftMath::Axis::Z, g_madnightEngine.m_trig);

	// create complete x/y/z rotation. this is done ROLL then YAW then PITCH
	psyqo::Matrix33 tempMatrix = {0};
	psyqo::GteMath::multiplyMatrix33(yaw, pitch, &tempMatrix);
	psyqo::GteMath::multiplyMatrix33(tempMatrix, roll, &m_rotationMatrix);

	// do both of these now so we dont have to do it every frame
	// generate the rotated pos
	psyqo::Vec3 rotatedPos = {0, 0, 0};
	psyqo::GteMath::matrixVecMul3(m_rotationMatrix, m_pos, &m_rotatedPos);

	// and generate the rotated velocity
	GenerateRotatedVelocity();
}

void ParticleEmitter::GenerateRotatedVelocity(void) {
	psyqo::GteMath::matrixVecMul3(m_rotationMatrix, m_particleStartVelocity, &m_particleRotatedStartVelocity);
	psyqo::GteMath::matrixVecMul3(m_rotationMatrix, m_particleEndVelocity, &m_particleRotatedEndVelocity);
}
