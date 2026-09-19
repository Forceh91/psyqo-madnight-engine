/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once
#include "../../pool/pool.hh"
#include "defs.hh"
#include "particle_emitter.hh"

#include <EASTL/fixed_vector.h>
#include <psyqo/fixed-point.hh>
#include <psyqo/vector.hh>

class ParticleEmitterManager final {
  public:
	ParticleEmitter* CreateParticleEmitter(const eastl::string_view& name, const psyqo::Vec3& pos,
										   const psyqo::FixedPoint<>& radius, const uint8_t& particlesPerSecond,
										   const uint8_t& particleLifeTimeSecs);
	void DestroyParticleEmitter(ParticleEmitter* emitter);

	const eastl::fixed_vector<ParticleEmitter*, MAX_PARTICLE_EMITTERS>& GetActiveEmitters(void);
	const constexpr ParticleEmitter* GetEmitters(void) const { return m_pool.Entries(); }
	ParticleEmitter* GetEmitterByName(const eastl::string_view& name);
	ParticleEmitter* GetEmitterByName(uint64_t nameHash);

  private:
	Pool<ParticleEmitter, MAX_PARTICLE_EMITTERS> m_pool;
	eastl::fixed_vector<ParticleEmitter*, MAX_PARTICLE_EMITTERS> m_activeEmitters;

	int16_t GetFreeIndex(void);
};
