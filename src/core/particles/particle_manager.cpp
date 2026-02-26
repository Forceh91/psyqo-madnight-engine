#include "particle_manager.hh"
#include "defs.hh"
#include "particle_emitter.hh"
#include "psyqo/fixed-point.hh"


eastl::array<ParticleEmitter, MAX_PARTICLE_EMITTERS> ParticleEmitterManager::m_emitters;
eastl::fixed_vector<ParticleEmitter*, MAX_PARTICLE_EMITTERS> ParticleEmitterManager::m_activeEmitters;

/*
* make sure that after calling this you call the following functions to actually get particles looking good.
* particles default to being 2D so they are sprites, which means they do need a texture
* `SetParticleColour`, `SetParticleVelocity`, `SetParticleSize`, `SetParticleTexture`
* you can make them 3d via `Set2D` however this is less performant.
*/
ParticleEmitter *ParticleEmitterManager::CreateParticleEmitter(const eastl::fixed_string<char, MAX_PARTICLE_EMITTER_NAME_LENGTH> &name, const psyqo::Vec3 &pos, const psyqo::FixedPoint<> &radius, const uint8_t &particlesPerSecond, const uint8_t &particleLifeTimeSecs) {
    auto ix = GetFreeIndex();
    if (ix == -1)
        return nullptr;

    m_emitters[ix] = ParticleEmitter(name, ix, pos, radius, particlesPerSecond, particleLifeTimeSecs);
    return &m_emitters[ix];
}

int8_t ParticleEmitterManager::GetFreeIndex(void) {
    for (auto i = 0; i < MAX_PARTICLE_EMITTERS; i++) {
        if (m_emitters.at(i).name().empty())
            return i;
    }

    return -1;
}

void ParticleEmitterManager::DestroyParticleEmitter(ParticleEmitter *emitter) {
    if (emitter)
        emitter->Destroy();
}

const eastl::fixed_vector<ParticleEmitter*, MAX_PARTICLE_EMITTERS> &ParticleEmitterManager::GetActiveEmitters(void) {
    m_activeEmitters.clear();

    for (auto &emitter : m_emitters) {
        if (emitter.id() != INVALID_PARTICLE_EMITTER_ID)
            m_activeEmitters.push_back(&emitter);
    }

    return m_activeEmitters;
}

ParticleEmitter* ParticleEmitterManager::GetEmitterByName(const eastl::fixed_string<char, MAX_PARTICLE_EMITTER_NAME_LENGTH> name) {
    for (auto i = 0; i < MAX_PARTICLE_EMITTERS; i++) {
        if (m_emitters.at(i).name() == name)
            return &m_emitters.at(i);
    }

    return nullptr;
}
