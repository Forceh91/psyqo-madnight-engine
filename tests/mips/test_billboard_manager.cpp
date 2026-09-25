#include "core/billboard/billboard_manager.hh"
#include "core/billboard/billboard.hh"
#include "helpers/archive.hh"
#include "snitch_all.hpp"

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
    BillboardManager mgr;
    psyqo::Vec3 pos;
    pos.x = 1.0;
    pos.y = 2.0;
    pos.z = 3.0;

    auto* billboard = mgr.CreateBillboard("test_billboard", pos, UnitSize());
    REQUIRE(billboard != nullptr);

    REQUIRE(billboard->id() != INVALID_POOL_ID);
    REQUIRE(billboard->nameHash() == HashName("test_billboard"));
    REQUIRE(billboard->pos().x.integer() == 1);
    REQUIRE(billboard->pos().y.integer() == 2);
    REQUIRE(billboard->pos().z.integer() == 3);

    mgr.DestroyBillboard(billboard);
}

TEST_CASE("CreateBillboard returns nullptr once the pool is full") {
    BillboardManager mgr;
    eastl::fixed_vector<Billboard*, MAX_BILLBOARDS> created;

    for (int16_t i = 0; i < MAX_BILLBOARDS; i++) {
        auto* billboard = mgr.CreateBillboard("filler", ZeroPos(), UnitSize());
        REQUIRE(billboard != nullptr);
        created.push_back(billboard);
    }

    auto* overflow = mgr.CreateBillboard("overflow", ZeroPos(), UnitSize());
    REQUIRE(overflow == nullptr);
}

TEST_CASE("Destroying a billboard frees its pool slot for reuse") {
    BillboardManager mgr;
    auto* first = mgr.CreateBillboard("first", ZeroPos(), UnitSize());
    REQUIRE(first != nullptr);
    auto firstId = first->id();

    mgr.DestroyBillboard(first);

    auto* second = mgr.CreateBillboard("second", ZeroPos(), UnitSize());
    REQUIRE(second != nullptr);
    REQUIRE(second->id() == firstId);

    mgr.DestroyBillboard(second);
}

TEST_CASE("Destroyed billboards are excluded from GetActiveBillboards") {
    BillboardManager mgr;
    auto* billboard = mgr.CreateBillboard("temp", ZeroPos(), UnitSize());
    REQUIRE(billboard != nullptr);

    mgr.DestroyBillboard(billboard);

    // NOTE: assumes Billboard::Destroy() resets m_id back to INVALID_POOL_ID
    const auto& active = mgr.GetActiveBillboards();
    for (const auto* b : active) {
        REQUIRE(b != billboard);
    }
}

TEST_CASE("GetBillboardByName finds a billboard by its exact name") {
    BillboardManager mgr;
    auto* created = mgr.CreateBillboard("torch_billboard", ZeroPos(), UnitSize());
    REQUIRE(created != nullptr);

    auto* found = mgr.GetBillboardByName("torch_billboard");
    REQUIRE(found == created);

    mgr.DestroyBillboard(created);
}

TEST_CASE("GetBillboardByName returns nullptr for a name that doesn't exist") {
    BillboardManager mgr;
    auto* found = mgr.GetBillboardByName("definitely_not_a_real_billboard");
    REQUIRE(found == nullptr);
}

TEST_CASE("GetBillboardByName(hash) finds a billboard by its name hash") {
    BillboardManager mgr;
    auto* created = mgr.CreateBillboard("sign_billboard", ZeroPos(), UnitSize());
    REQUIRE(created != nullptr);

    auto* found = mgr.GetBillboardByName(HashName("sign_billboard"));
    REQUIRE(found == created);

    mgr.DestroyBillboard(created);
}
