#include "particle.hh"
#include "../../math/vector.hh"
#include "defs.hh"
#include "psyqo/fixed-point.hh"
#include "psyqo/xprintf.h"

using namespace psyqo::fixed_point_literals;

void Particle::Process(const uint32_t &deltaTime) {
    // guard against a particle having no lifetime somehow
    if (m_lifetime < 0.05_fp || m_age >= m_lifetime)
        return;

    auto fpDeltaTime = (deltaTime * 1.0_fp) / TARGET_FRAME_RATE;
    m_age += fpDeltaTime;

    auto lifetimeLerp = m_age / m_lifetime;
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
