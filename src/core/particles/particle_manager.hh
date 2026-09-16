#pragma once
#include "defs.hh"
#include "particle_emitter.hh"

#include "EASTL/array.h"
#include "EASTL/fixed_string.h"
#include "EASTL/fixed_vector.h"
#include "psyqo/fixed-point.hh"
#include "psyqo/vector.hh"

class ParticleEmitterManager final {
  public:
	ParticleEmitter* CreateParticleEmitter(const eastl::string_view& name, const psyqo::Vec3& pos,
										   const psyqo::FixedPoint<>& radius, const uint8_t& particlesPerSecond,
										   const uint8_t& particleLifeTimeSecs);
	void DestroyParticleEmitter(ParticleEmitter* emitter);

	const eastl::fixed_vector<ParticleEmitter*, MAX_PARTICLE_EMITTERS>& GetActiveEmitters(void);
	const eastl::array<ParticleEmitter, MAX_PARTICLE_EMITTERS>& GetEmitters(void) { return m_emitters; }
	ParticleEmitter* GetEmitterByName(const eastl::string_view& name);
	ParticleEmitter* GetEmitterByName(uint64_t nameHash);

  private:
	eastl::array<ParticleEmitter, MAX_PARTICLE_EMITTERS> m_emitters;
	eastl::fixed_vector<ParticleEmitter*, MAX_PARTICLE_EMITTERS> m_activeEmitters;

	int16_t GetFreeIndex(void);
};
