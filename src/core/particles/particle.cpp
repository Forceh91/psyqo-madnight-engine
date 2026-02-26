#include "particle.hh"
#include "../../math/vector.hh"
#include "defs.hh"
#include "psyqo/fixed-point.hh"
#include "psyqo/xprintf.h"

using namespace psyqo::fixed_point_literals;

void Particle::Process(const uint32_t &deltaTime) {
    // guard against a particle having no lifetime somehow
    if (m_lifetime < 1 || m_age >= m_lifetimeMicroSeconds)
        return;

    m_age += deltaTime;
    
    // need to convert delta time into seconds to get fp first
    auto fpDeltaTime = 1.0_fp * (deltaTime / 1000) / 1000;   
    auto ageMS = m_age / 1000;
    auto lifetimeMS = m_lifetimeMicroSeconds / 1000;

    auto lifetimeLerp = (1.0_fp * ageMS) / (1.0_fp * lifetimeMS);
    auto r = (m_startColour.r * (1.0_fp - lifetimeLerp) + m_endColour.r * lifetimeLerp).value >> 12;
    auto g = (m_startColour.g * (1.0_fp - lifetimeLerp) + m_endColour.g * lifetimeLerp).value >> 12;
    auto b = (m_startColour.b * (1.0_fp - lifetimeLerp) + m_endColour.b * lifetimeLerp).value >> 12;

    psyqo::Color lerpedColour = {
        static_cast<uint8_t>(r),
        static_cast<uint8_t>(g),
        static_cast<uint8_t>(b)
    };
    SetColour(lerpedColour);
    
    auto lerpedSize = Lerp(m_startSize, m_endSize, lifetimeLerp);
    setSize(lerpedSize);
    
    auto lerpedVelocity = Lerp(m_startVelocity, m_endVelocity, lifetimeLerp);
    auto deltaTimeVelocity= lerpedVelocity * fpDeltaTime;

    SetPosition(m_pos + deltaTimeVelocity);
}
