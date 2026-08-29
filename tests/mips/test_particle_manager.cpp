#include "core/particles/particle_manager.hh"
#include "core/particles/particle_emitter.hh"
#include "snitch_all.hpp"

// NOTE: same caveat as the billboard tests — ParticleEmitterManager is
// static state with no bulk reset exposed, so each test only relies on
// what it itself creates and destroys, and cleans up after itself.

static psyqo::Vec3 ZeroPos() {
    psyqo::Vec3 pos;
    pos.x = 0;
    pos.y = 0;
    pos.z = 0;
    return pos;
}

// keeping particlesPerSecond/particleLifeTimeSecs small and non-zero:
// Init divides by particlesPerSecond with no zero-check, so 0 would be a
// divide-by-zero — not something to exercise here, just avoiding it
static ParticleEmitter* MakeEmitter(const eastl::fixed_string<char, MAX_PARTICLE_EMITTER_NAME_LENGTH>& name) {
    psyqo::FixedPoint<> radius = 5.0;
    return ParticleEmitterManager::CreateParticleEmitter(name, ZeroPos(), radius, 10, 2);
}

TEST_CASE("CreateParticleEmitter initializes name and id") {
    auto* emitter = MakeEmitter("test_emitter");
    REQUIRE(emitter != nullptr);

    REQUIRE(emitter->id() != INVALID_POOL_ID);
    REQUIRE(emitter->nameHash() == HashName("test_emitter"));

    ParticleEmitterManager::DestroyParticleEmitter(emitter);
}

TEST_CASE("CreateParticleEmitter returns nullptr once the pool is full") {
    eastl::fixed_vector<ParticleEmitter*, MAX_PARTICLE_EMITTERS> created;

    for (int16_t i = 0; i < MAX_PARTICLE_EMITTERS; i++) {
        auto* emitter = MakeEmitter("filler");
        REQUIRE(emitter != nullptr);
        created.push_back(emitter);
    }

    auto* overflow = MakeEmitter("overflow");
    REQUIRE(overflow == nullptr);

    for (auto* emitter : created)
        ParticleEmitterManager::DestroyParticleEmitter(emitter);
}

TEST_CASE("Destroying a particle emitter frees its pool slot for reuse") {
    auto* first = MakeEmitter("first");
    REQUIRE(first != nullptr);
    auto firstId = first->id();

    ParticleEmitterManager::DestroyParticleEmitter(first);

    auto* second = MakeEmitter("second");
    REQUIRE(second != nullptr);
    REQUIRE(second->id() == firstId);

    ParticleEmitterManager::DestroyParticleEmitter(second);
}

TEST_CASE("Destroyed emitters are excluded from GetActiveEmitters") {
    auto* emitter = MakeEmitter("temp");
    REQUIRE(emitter != nullptr);

    ParticleEmitterManager::DestroyParticleEmitter(emitter);

    // NOTE: assumes ParticleEmitter::Destroy() resets m_id back to INVALID_POOL_ID
    const auto& active = ParticleEmitterManager::GetActiveEmitters();
    for (const auto* e : active) {
        REQUIRE(e != emitter);
    }
}

TEST_CASE("GetEmitterByName finds an emitter by its exact name") {
    auto* created = MakeEmitter("fire_emitter");
    REQUIRE(created != nullptr);

    auto* found = ParticleEmitterManager::GetEmitterByName("fire_emitter");
    REQUIRE(found == created);

    ParticleEmitterManager::DestroyParticleEmitter(created);
}

TEST_CASE("GetEmitterByName returns nullptr for a name that doesn't exist") {
    auto* found = ParticleEmitterManager::GetEmitterByName("definitely_not_a_real_emitter");
    REQUIRE(found == nullptr);
}

TEST_CASE("GetEmitterByName(hash) finds an emitter by its name hash") {
    auto* created = MakeEmitter("smoke_emitter");
    REQUIRE(created != nullptr);

    auto* found = ParticleEmitterManager::GetEmitterByName(HashName("smoke_emitter"));
    REQUIRE(found == created);

    ParticleEmitterManager::DestroyParticleEmitter(created);
}