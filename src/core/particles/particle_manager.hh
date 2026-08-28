#pragma once

#include "defs.hh"
#include "particle_emitter.hh"
#include "../../pool/pool.hh"

#include <EASTL/array.h>
#include <EASTL/fixed_string.h>
#include <EASTL/fixed_vector.h>
#include <psyqo/fixed-point.hh>
#include <psyqo/vector.hh>

class ParticleEmitterManager final {
public:
    static ParticleEmitter* CreateParticleEmitter(const eastl::fixed_string<char, MAX_PARTICLE_EMITTER_NAME_LENGTH>& name, const psyqo::Vec3& pos, const psyqo::FixedPoint<>& radius, const uint8_t& particlesPerSecond, const uint8_t& particleLifeTimeSecs);
    static void DestroyParticleEmitter(ParticleEmitter* emitter);

    static const eastl::fixed_vector<ParticleEmitter*, MAX_PARTICLE_EMITTERS>& GetActiveEmitters(void);
    static const ParticleEmitter* GetEmitters(void) { return m_pool.Entries(); }
    static ParticleEmitter* GetEmitterByName(const eastl::fixed_string<char, MAX_PARTICLE_EMITTER_NAME_LENGTH>& name);
    static ParticleEmitter* GetEmitterByName(uint64_t nameHash);

private:
    static Pool<ParticleEmitter, MAX_PARTICLE_EMITTERS> m_pool;
    static eastl::fixed_vector<ParticleEmitter*, MAX_PARTICLE_EMITTERS> m_activeEmitters;
};
