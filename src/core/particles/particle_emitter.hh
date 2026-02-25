#ifndef _PARTICLE_EMITTER_H
#define _PARTICLE_EMITTER_H

#include "EASTL/vector.h"
#include "defs.hh"
#include "particle.hh"
#include "psyqo/fixed-point.hh"
#include "psyqo/primitives/common.hh"
#include "psyqo/vector.hh"

using namespace psyqo::fixed_point_literals;

class ParticleEmitter final {
public:
    ParticleEmitter() = default;
    ParticleEmitter(const eastl::fixed_string<char, MAX_PARTICLE_EMITTER_NAME_LENGTH> &name, const uint8_t &id, const psyqo::Vec3 &pos, const psyqo::FixedPoint<> radius, const uint8_t &particlesPerSecond, const psyqo::FixedPoint<> &particleLifeTime) {
        m_id = id;
        m_name = name;
        m_pos = pos;
        m_radius = radius;
        m_particlesPerSecond = particlesPerSecond;
        m_particleLifeTime = particleLifeTime;
        m_maxParticles = m_particlesPerSecond * m_particleLifeTime.integer();
        m_spawnRate = 1.0_fp / m_particlesPerSecond;
    };

    const eastl::fixed_string<char, MAX_PARTICLE_EMITTER_NAME_LENGTH> &name() const { return m_name; }
    const uint8_t &id() const { return m_id; }    

    void Start(void);
    void Stop(void);
    void Destroy(void);

    void Process(const uint32_t &deltaTime);
    const eastl::vector<Particle> &particles() const { return m_spawnedParticles; };

    void SetParticles2D(const bool &is2D);

    void SetParticleVelocity(const psyqo::Vec3 &particleVelocity);
    void SetParticleVelocity(const psyqo::Vec3 &particleVelocity, const psyqo::Vec3 &particleEndVelocity);    

    void SetParticleSize(const psyqo::Vec2 &particleSize);
    void SetParticleSize(const psyqo::Vec2 &particleSize, const psyqo::Vec2 &particleEndSize);

    void SetParticleColour(const psyqo::Color &particleColour);
    void SetParticleColour(const psyqo::Color &particleColour, const psyqo::Color &particleEndColour);

    void SetParticleTexture(const eastl::fixed_string<char, MAX_CDROM_FILE_NAME_LEN> &textureName, const eastl::array<psyqo::PrimPieces::UVCoords, 4> &uv);
    void SetParticleUVCoords(const eastl::array<psyqo::PrimPieces::UVCoords, 4> &uv);

    const TimFile *pParticleTexture() const { return m_particleTexture; }
    const bool &AreParticles2D() const { return m_particleIs2D; }
private:
    bool m_isEnabled = false;
    eastl::fixed_string<char, MAX_PARTICLE_EMITTER_NAME_LENGTH> m_name;
    uint8_t m_id = INVALID_PARTICLE_EMITTER_ID;
    psyqo::Vec3 m_pos = {0,0,0};
    psyqo::FixedPoint<> m_radius = 0;
    uint16_t m_maxParticles = 0;
    uint8_t m_particlesPerSecond = 0;
    eastl::vector<Particle> m_spawnedParticles;
    psyqo::FixedPoint<> m_spawnRate = 0;
    psyqo::FixedPoint<> m_timeSinceLastParticleSpawn = 0;

    psyqo::Color m_particleStartColour;
    psyqo::Color m_particleEndColour;
    psyqo::Vec2 m_particleStartSize;
    psyqo::Vec2 m_particleEndSize;
    psyqo::Vec3 m_particleStartVelocity;
    psyqo::Vec3 m_particleEndVelocity;
    psyqo::FixedPoint<> m_particleLifeTime;
    eastl::array<psyqo::PrimPieces::UVCoords, 4> m_particleUVCoords;
    TimFile *m_particleTexture = nullptr;
    bool m_particleIs2D = true;

    psyqo::Vec2 GenerateRandomPointOnCircumfrence(void);
};

#endif
