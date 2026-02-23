#include "particle_emitter.hh"
#include "defs.hh"
#include "particle.hh"
#include "../../madnight.hh"
#include "psyqo/fixed-point.hh"
#include "psyqo/trigonometry.hh"

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
    m_radius = 0;
    m_id = INVALID_BILLBOARD_ID;
    m_isEnabled = false;
    m_spawnedParticles.clear();
}

psyqo::Vec2 ParticleEmitter::GenerateRandomPointOnCircumfrence(void) {
    auto angle = g_madnightEngine.m_rand.rand<360>() * psyqo::Angle(3.14 * 2);
    return {
        g_madnightEngine.m_trig.cos(angle) * m_radius,
        g_madnightEngine.m_trig.sin(angle) * m_radius,
    };
}

void ParticleEmitter::Process(const uint32_t &deltaTime) {
    // process active particles
    for (auto &particle : m_spawnedParticles) {
        if (!particle.IsDead()) particle.Process(deltaTime);
    }
 
    // clear out dead ones
    for (int i = m_spawnedParticles.size() - 1; i >= 0; i--) {
        auto const &particle = m_spawnedParticles.at(i);
        if (particle.IsDead())
            m_spawnedParticles.erase(m_spawnedParticles.begin() + i);
    }

    // make sure we're enabled or we're not out of room
    if (!m_isEnabled || m_spawnedParticles.size() >= m_maxParticles) return;

    // make sure its been a second
    auto fpDeltaTime = (deltaTime * 1.0_fp) / TARGET_FRAME_RATE;
    m_timeSinceLastParticleSpawn += fpDeltaTime;
    if (m_timeSinceLastParticleSpawn < m_spawnRate)
        return;

    // generate a particle at a random point on the circumfrence
    auto pos = GenerateRandomPointOnCircumfrence();
    auto spawnPos = psyqo::Vec3{
        m_pos.x + pos.x,
        m_pos.y,
        m_pos.z + pos.y
    };

    auto particle = Particle(spawnPos, m_particleStartSize, m_particleEndSize, m_particleStartColour, m_particleEndColour, m_particleStartVelocity, m_particleEndVelocity, m_particleLifeTime);
    if (m_particleTexture)
        particle.SetTexture(m_particleTexture, m_particleUVCoords);

    m_spawnedParticles.push_back(particle);
    m_timeSinceLastParticleSpawn = 0;
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

void ParticleEmitter::SetParticleTexture(const eastl::fixed_string<char, MAX_CDROM_FILE_NAME_LEN> &textureName, const eastl::array<psyqo::PrimPieces::UVCoords, 4> &uv) {
    TextureManager::GetTextureFromName(textureName.c_str(), &m_particleTexture);
    m_particleUVCoords = uv;
}

void ParticleEmitter::SetParticleUVCoords(const eastl::array<psyqo::PrimPieces::UVCoords, 4> &uv) {
    m_particleUVCoords = uv;
}