#include "particle_emitter.hh"
#include "defs.hh"
#include "particle.hh"
#include "psyqo/xprintf.h"

using namespace psyqo::fixed_point_literals;

void ParticleEmitter::Start(void) {
    m_isEnabled = true;
}

void ParticleEmitter::Stop(void) {
    m_isEnabled = false;
}

void ParticleEmitter::Destroy(void) {
    m_name.clear();
    m_pos = {0,0,0};
    m_size = {0,0};
    m_id = INVALID_BILLBOARD_ID;
    m_isEnabled = false;
    m_spawnedParticles.clear();
}

void ParticleEmitter::Process(const uint32_t &deltaTime) {
    // process active particles
    for (auto &particle : m_spawnedParticles) {
        if (!particle.IsDead()) particle.Process(deltaTime);
    }
 
    // clear out dead ones
    for (int i = m_spawnedParticles.size() - 1; i >= 0; i--) {
        auto const &particle = m_spawnedParticles.at(i);
        if (particle.IsDead()) m_spawnedParticles.erase(m_spawnedParticles.begin() + i);
    }

    // make sure we're enabled or we're not out of room
    if (!m_isEnabled || m_spawnedParticles.size() >= m_maxParticles) return;

    // make sure its been a second
    auto fpDeltaTime = (deltaTime * 1.0_fp) / TARGET_FRAME_RATE;
    m_timeSinceLastSpawn += fpDeltaTime;
    if (m_timeSinceLastSpawn < 1.0_fp)
        return;

    // it has so spam particles whilst we can
    for (int i = 0; i < m_particlesPerSecond; i++) {
        // TODO: pick a random point in the emitter area to spawn it at
        m_spawnedParticles.push_back(Particle(m_pos, m_particleStartSize, m_particleEndSize, m_particleStartColour, m_particleEndColour, m_particleStartVelocity, m_particleEndVelocity, m_particleLifeTime));
    }

    // reset how long its been since last spawn
    m_timeSinceLastSpawn = 0;
}

void ParticleEmitter::SetParticleVelocity(const psyqo::Vec3 &particleVelocity) {
    SetParticleVelocity(particleVelocity, particleVelocity);
}

void ParticleEmitter::SetParticleVelocity(const psyqo::Vec3 &particleVelocity, const psyqo::Vec3 &particleEndVelocity) {
    m_particleStartVelocity = particleVelocity;
    m_particleEndVelocity = particleEndVelocity;
}

void ParticleEmitter::SetParticleSize(const psyqo::Vec2 &particleSize) {
    SetParticleSize(particleSize, particleSize);
}

void ParticleEmitter::SetParticleSize(const psyqo::Vec2 &particleSize, const psyqo::Vec2 &particleEndSize) {
    m_particleStartSize = particleSize;
    m_particleEndSize = particleEndSize;
}

void ParticleEmitter::SetParticleColour(const psyqo::Color &particleColour) {
    SetParticleColour(particleColour, particleColour);
}

void ParticleEmitter::SetParticleColour(const psyqo::Color &particleColour, const psyqo::Color &particleEndColour) {
    m_particleStartColour = particleColour;
    m_particleEndColour = particleEndColour;
}
