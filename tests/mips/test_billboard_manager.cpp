#include "core/billboard/billboard_manager.hh"
#include "core/billboard/billboard.hh"
#include "snitch_all.hpp"

// NOTE: every test calls DestroyBillboard on anything it creates, or relies
// on pool exhaustion tests being self-contained, since BillboardManager is
// static state shared across every TEST_CASE in this binary. There's no
// Dump()-equivalent bulk reset exposed here (unlike GameObjectManager), so
// tests can't blindly assume a clean pool — each one is written to only
// depend on what it itself creates and destroys.

static psyqo::Vec3 ZeroPos() {
    psyqo::Vec3 pos;
    pos.x = 0;
    pos.y = 0;
    pos.z = 0;
    return pos;
}

static psyqo::Vec2 UnitSize() {
    psyqo::Vec2 size;
    size.x = 1.0;
    size.y = 1.0;
    return size;
}

TEST_CASE("CreateBillboard initializes name, position, size, and id") {
    psyqo::Vec3 pos;
    pos.x = 1.0;
    pos.y = 2.0;
    pos.z = 3.0;

    auto* billboard = BillboardManager::CreateBillboard("test_billboard", pos, UnitSize());
    REQUIRE(billboard != nullptr);

    REQUIRE(billboard->id() != INVALID_POOL_ID);
    REQUIRE(billboard->nameHash() == HashName("test_billboard"));
    REQUIRE(billboard->pos().x.integer() == 1);
    REQUIRE(billboard->pos().y.integer() == 2);
    REQUIRE(billboard->pos().z.integer() == 3);

    BillboardManager::DestroyBillboard(billboard);
}

TEST_CASE("CreateBillboard returns nullptr once the pool is full") {
    eastl::fixed_vector<Billboard*, MAX_BILLBOARDS> created;

    for (int16_t i = 0; i < MAX_BILLBOARDS; i++) {
        auto* billboard = BillboardManager::CreateBillboard("filler", ZeroPos(), UnitSize());
        REQUIRE(billboard != nullptr);
        created.push_back(billboard);
    }

    auto* overflow = BillboardManager::CreateBillboard("overflow", ZeroPos(), UnitSize());
    REQUIRE(overflow == nullptr);

    // clean up so later tests aren't starved of slots
    for (auto* billboard : created)
        BillboardManager::DestroyBillboard(billboard);
}

TEST_CASE("Destroying a billboard frees its pool slot for reuse") {
    auto* first = BillboardManager::CreateBillboard("first", ZeroPos(), UnitSize());
    REQUIRE(first != nullptr);
    auto firstId = first->id();

    BillboardManager::DestroyBillboard(first);

    auto* second = BillboardManager::CreateBillboard("second", ZeroPos(), UnitSize());
    REQUIRE(second != nullptr);
    REQUIRE(second->id() == firstId);

    BillboardManager::DestroyBillboard(second);
}

TEST_CASE("Destroyed billboards are excluded from GetActiveBillboards") {
    auto* billboard = BillboardManager::CreateBillboard("temp", ZeroPos(), UnitSize());
    REQUIRE(billboard != nullptr);

    BillboardManager::DestroyBillboard(billboard);

    // NOTE: assumes Billboard::Destroy() resets m_id back to INVALID_POOL_ID
    const auto& active = BillboardManager::GetActiveBillboards();
    for (const auto* b : active) {
        REQUIRE(b != billboard);
    }
}

TEST_CASE("GetBillboardByName finds a billboard by its exact name") {
    auto* created = BillboardManager::CreateBillboard("torch_billboard", ZeroPos(), UnitSize());
    REQUIRE(created != nullptr);

    auto* found = BillboardManager::GetBillboardByName("torch_billboard");
    REQUIRE(found == created);

    BillboardManager::DestroyBillboard(created);
}

TEST_CASE("GetBillboardByName returns nullptr for a name that doesn't exist") {
    auto* found = BillboardManager::GetBillboardByName("definitely_not_a_real_billboard");
    REQUIRE(found == nullptr);
}

TEST_CASE("GetBillboardByName(hash) finds a billboard by its name hash") {
    auto* created = BillboardManager::CreateBillboard("sign_billboard", ZeroPos(), UnitSize());
    REQUIRE(created != nullptr);

    auto* found = BillboardManager::GetBillboardByName(HashName("sign_billboard"));
    REQUIRE(found == created);

    BillboardManager::DestroyBillboard(created);
}
