#ifndef _PARTICLE_HH
#define _PARTICLE_HH

#include "../billboard/billboard.hh"
#include "defs.hh"
#include "psyqo/primitives/common.hh"

class Particle final : public Billboard {
public:
    Particle() = default;
    Particle(const psyqo::Vec3 pos, const psyqo::Vec2 size, const psyqo::Color colour, const psyqo::Vec3 velocity, const uint8_t lifetime = 1) : Particle(pos, size, size, colour, colour, velocity, velocity, lifetime) {};

    Particle(const psyqo::Vec3 pos, const psyqo::Vec2 startSize, const psyqo::Vec2 endSize, const psyqo::Color startColour, const psyqo::Color endColour, const psyqo::Vec3 startVelocity, const psyqo::Vec3 endVelocity, const uint8_t lifetime = 1) {
        m_startSize = startSize;
        m_endSize = endSize;

        m_startColour = startColour;
        m_endColour = endColour;

        m_startVelocity = startVelocity;
        m_endVelocity = endVelocity;

        m_lifetime = lifetime;
        m_age = 0;
        m_lifetimeMicroSeconds = MICROSECONDS_IN_A_SECOND * m_lifetime;
        m_lastUpdate = 0;

        // billboard settings
        m_pos = pos;
        m_size = startSize;
        m_colour = m_startColour;
        Billboard("", pos, startSize, 1);
    }

    void Process(const uint32_t &deltaTime);
    const bool IsDead(void) const { return m_age >= m_lifetimeMicroSeconds; }

private:
    psyqo::Color m_startColour;
    psyqo::Color m_endColour;
    psyqo::Vec2 m_startSize;
    psyqo::Vec2 m_endSize;
    psyqo::Vec3 m_startVelocity;
    psyqo::Vec3 m_endVelocity;
    uint8_t m_lifetime = 1;
    uint32_t m_lifetimeMicroSeconds = 0;
    uint32_t m_age = 0;
    uint32_t m_lastUpdate;
};

#endif
