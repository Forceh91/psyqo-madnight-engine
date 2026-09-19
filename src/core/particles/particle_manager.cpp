/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "particle_manager.hh"
#include "../../helpers/archive.hh"
#include "particle_emitter.hh"
#include <psyqo/fixed-point.hh>

/*
 * make sure that after calling this you call the following functions to actually get particles looking good.
 * particles default to being 2D so they are sprites, which means they do need a texture
 * `SetParticleColour`, `SetParticleVelocity`, `SetParticleSize`, `SetParticleTexture`
 * you can make them 3d via `Set2D` however this is less performant.
 */
ParticleEmitter* ParticleEmitterManager::CreateParticleEmitter(const eastl::string_view& name, const psyqo::Vec3& pos,
															   const psyqo::FixedPoint<>& radius,
															   const uint8_t& particlesPerSecond,
															   const uint8_t& particleLifeTimeSecs) {
	auto id = m_pool.Acquire();
	if (id == INVALID_POOL_ID) {
		return nullptr;
	}

	auto* emitter = m_pool.Get(id);
	emitter->Init(HashName(name), id, pos, radius, particlesPerSecond, particleLifeTimeSecs);
	return emitter;
}

void ParticleEmitterManager::DestroyParticleEmitter(ParticleEmitter* emitter) {
	if (emitter) {
		emitter->Destroy();
	}
}

const eastl::fixed_vector<ParticleEmitter*, MAX_PARTICLE_EMITTERS>& ParticleEmitterManager::GetActiveEmitters(void) {
	m_activeEmitters.clear();

	auto count = m_pool.size();
	for (auto i = 0; i < count; i++) {
		auto* emitter = m_pool.Get(i);
		if (emitter && emitter->id() != INVALID_POOL_ID) {
			m_activeEmitters.push_back(emitter);
		}
	}

	return m_activeEmitters;
}

ParticleEmitter* ParticleEmitterManager::GetEmitterByName(const eastl::string_view& name) {
	return GetEmitterByName(HashName(name));
}

ParticleEmitter* ParticleEmitterManager::GetEmitterByName(uint64_t nameHash) {
	auto count = m_pool.size();
	for (auto i = 0; i < count; i++) {
		auto* emitter = m_pool.Get(i);
		if (emitter && emitter->id() != INVALID_POOL_ID && emitter->nameHash() == nameHash) {
			return emitter;
		}
	}

	return nullptr;
}
