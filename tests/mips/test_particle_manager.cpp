#include "core/particles/particle_manager.hh"
#include "core/particles/particle_emitter.hh"
#include "helpers/archive.hh"
#include "snitch_all.hpp"

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
static ParticleEmitter* MakeEmitter(ParticleEmitterManager& mgr, const eastl::string_view& name) {
    psyqo::FixedPoint<> radius = 5.0;
    return mgr.CreateParticleEmitter(name, ZeroPos(), radius, 10, 2);
}

TEST_CASE("CreateParticleEmitter initializes name and id") {
    ParticleEmitterManager mgr;
    auto* emitter = MakeEmitter(mgr, "test_emitter");
    REQUIRE(emitter != nullptr);

    REQUIRE(emitter->id() != INVALID_POOL_ID);
    REQUIRE(emitter->nameHash() == HashName("test_emitter"));

    mgr.DestroyParticleEmitter(emitter);
}

TEST_CASE("CreateParticleEmitter returns nullptr once the pool is full") {
    ParticleEmitterManager mgr;
    eastl::fixed_vector<ParticleEmitter*, MAX_PARTICLE_EMITTERS> created;

    for (int16_t i = 0; i < MAX_PARTICLE_EMITTERS; i++) {
        auto* emitter = MakeEmitter(mgr, "filler");
        REQUIRE(emitter != nullptr);
        created.push_back(emitter);
    }

    auto* overflow = MakeEmitter(mgr, "overflow");
    REQUIRE(overflow == nullptr);
}

TEST_CASE("Destroying a particle emitter frees its pool slot for reuse") {
    ParticleEmitterManager mgr;
    auto* first = MakeEmitter(mgr, "first");
    REQUIRE(first != nullptr);
    auto firstId = first->id();

    mgr.DestroyParticleEmitter(first);

    auto* second = MakeEmitter(mgr, "second");
    REQUIRE(second != nullptr);
    REQUIRE(second->id() == firstId);

    mgr.DestroyParticleEmitter(second);
}

TEST_CASE("Destroyed emitters are excluded from GetActiveEmitters") {
    ParticleEmitterManager mgr;
    auto* emitter = MakeEmitter(mgr, "temp");
    REQUIRE(emitter != nullptr);

    mgr.DestroyParticleEmitter(emitter);

    // NOTE: assumes ParticleEmitter::Destroy() resets m_id back to INVALID_POOL_ID
    const auto& active = mgr.GetActiveEmitters();
    for (const auto* e : active) {
        REQUIRE(e != emitter);
    }
}

TEST_CASE("GetEmitterByName finds an emitter by its exact name") {
    ParticleEmitterManager mgr;
    auto* created = MakeEmitter(mgr, "fire_emitter");
    REQUIRE(created != nullptr);

    auto* found = mgr.GetEmitterByName("fire_emitter");
    REQUIRE(found == created);

    mgr.DestroyParticleEmitter(created);
}

TEST_CASE("GetEmitterByName returns nullptr for a name that doesn't exist") {
    ParticleEmitterManager mgr;
    auto* found = mgr.GetEmitterByName("definitely_not_a_real_emitter");
    REQUIRE(found == nullptr);
}

TEST_CASE("GetEmitterByName(hash) finds an emitter by its name hash") {
    ParticleEmitterManager mgr;
    auto* created = MakeEmitter(mgr, "smoke_emitter");
    REQUIRE(created != nullptr);

    auto* found = mgr.GetEmitterByName(HashName("smoke_emitter"));
    REQUIRE(found == created);

    mgr.DestroyParticleEmitter(created);
}