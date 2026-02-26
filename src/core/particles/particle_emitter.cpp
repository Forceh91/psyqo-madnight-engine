#include "particle_emitter.hh"
#include "defs.hh"
#include "particle.hh"
#include "../../render/renderer.hh"
#include "../../madnight.hh"
#include "../../math/gte-math.hh"
#include "psyqo/fixed-point.hh"
#include "psyqo/soft-math.hh"
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
    m_id = INVALID_PARTICLE_EMITTER_ID;
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
    auto now = Renderer::Instance().GPU().now();
    auto delta = now - m_timeOfLastProcess;;

    m_timeSinceLastParticleSpawn += delta;

    // process active particles
    for (auto &particle : m_spawnedParticles) {
        if (!particle.IsDead()) particle.Process(delta);
    }
 
    // clear out dead ones
    for (int i = m_spawnedParticles.size() - 1; i >= 0; i--) {
        auto const &particle = m_spawnedParticles.at(i);
        if (particle.IsDead())
            m_spawnedParticles.erase(m_spawnedParticles.begin() + i);
    }

    // make sure we're enabled
    if (!m_isEnabled) return;
        
    // make sure its been a second and we dont have too many spawned
    m_timeOfLastProcess = now;
    if (m_timeSinceLastParticleSpawn < m_spawnRate || m_spawnedParticles.size() >= m_maxParticles)
        return;

    // generate a particle at a random point on the circumfrence
    auto pos = GenerateRandomPointOnCircumfrence();
    psyqo::Vec3 rotatedPos = {0, 0, 0};
    GTEMath::MultiplyMatrixVec3(m_rotationMatrix, {pos.x, 0, pos.y}, &rotatedPos);

    psyqo::Vec3 rotatedStartVelocity = {0, 0, 0};
    psyqo::Vec3 rotatedEndVelocity = {0, 0, 0};
    GTEMath::MultiplyMatrixVec3(m_rotationMatrix, m_particleStartVelocity, &rotatedStartVelocity);
    GTEMath::MultiplyMatrixVec3(m_rotationMatrix, m_particleEndVelocity, &rotatedEndVelocity);    

    auto spawnPos = m_pos + rotatedPos;
    auto particle = Particle(spawnPos, m_particleStartSize, m_particleEndSize, m_particleStartColour, m_particleEndColour, rotatedStartVelocity, rotatedEndVelocity, m_particleLifeTime);

    if (m_particleTexture)
        particle.SetUVCoords(m_particleUVCoords);

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

void ParticleEmitter::SetParticles2D(const bool &is2D) {
    m_particleIs2D = is2D;
}

void ParticleEmitter::SetRotation(const EmitterRotation &rotation) {
    m_rotation = rotation;
    GenerateRotationMatrix();
}

void ParticleEmitter::GenerateRotationMatrix(void) {
    auto roll = psyqo::SoftMath::generateRotationMatrix33(m_rotation.x, psyqo::SoftMath::Axis::X, g_madnightEngine.m_trig);
    auto pitch = psyqo::SoftMath::generateRotationMatrix33(m_rotation.y, psyqo::SoftMath::Axis::Y, g_madnightEngine.m_trig);
    auto yaw = psyqo::SoftMath::generateRotationMatrix33(m_rotation.z, psyqo::SoftMath::Axis::Z, g_madnightEngine.m_trig);

    // create complete x/y/z rotation. this is done ROLL then YAW then PITCH
    psyqo::Matrix33 tempMatrix = {0};
    psyqo::SoftMath::multiplyMatrix33(yaw, pitch, &tempMatrix);
    psyqo::SoftMath::multiplyMatrix33(tempMatrix, roll, &m_rotationMatrix);
}
