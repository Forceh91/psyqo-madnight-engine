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
	static ParticleEmitter* CreateParticleEmitter(const eastl::string_view& name, const psyqo::Vec3& pos,
												  const psyqo::FixedPoint<>& radius, const uint8_t& particlesPerSecond,
												  const uint8_t& particleLifeTimeSecs);
	static void DestroyParticleEmitter(ParticleEmitter* emitter);

	static const eastl::fixed_vector<ParticleEmitter*, MAX_PARTICLE_EMITTERS>& GetActiveEmitters(void);
	static const eastl::array<ParticleEmitter, MAX_PARTICLE_EMITTERS>& GetEmitters(void) { return m_emitters; }
	static ParticleEmitter* GetEmitterByName(const eastl::string_view& name);
	static ParticleEmitter* GetEmitterByName(uint64_t nameHash);

  private:
	static eastl::array<ParticleEmitter, MAX_PARTICLE_EMITTERS> m_emitters;
	static eastl::fixed_vector<ParticleEmitter*, MAX_PARTICLE_EMITTERS> m_activeEmitters;

	static int16_t GetFreeIndex(void);
};
