#include "core/object/gameobject_manager.hh"
#include "core/object/gameobject.hh"

#include "helpers/archive.hh"
#include "snitch_all.hpp"
#include <EASTL/span.h>

static GameObjectRotation ZeroRotation() {
    return GameObjectRotation{0, 0, 0};
}

static psyqo::Vec3 ZeroPos() {
    psyqo::Vec3 pos;
    pos.x = 0;
    pos.y = 0;
    pos.z = 0;
    return pos;
}

// --- CreateGameObject ---

TEST_CASE("CreateGameObject initializes name, position, tag, and id") {
    GameObjectManager mgr;

    psyqo::Vec3 pos;
    pos.x = 1.0;
    pos.y = 2.0;
    pos.z = 3.0;

    auto* obj = mgr.CreateGameObject("test_object", pos, ZeroRotation(), GameObjectTag::ENVIRONMENT);
    REQUIRE(obj != nullptr);

    REQUIRE(obj->id() == 0);
    REQUIRE(obj->name() == "test_object");
    REQUIRE(obj->nameHash() == HashName("test_object"));
    REQUIRE(obj->pos().x.integer() == 1);
    REQUIRE(obj->pos().y.integer() == 2);
    REQUIRE(obj->pos().z.integer() == 3);
    REQUIRE(obj->tag() == GameObjectTag::ENVIRONMENT);
}

TEST_CASE("CreateGameObject returns nullptr once the pool is full") {
    GameObjectManager mgr;

    for (int16_t i = 0; i < MAX_GAME_OBJECTS; i++) {
        auto* obj = mgr.CreateGameObject("filler", ZeroPos(), ZeroRotation(), GameObjectTag::NONE);
        REQUIRE(obj != nullptr);
    }

    auto* overflow = mgr.CreateGameObject("overflow", ZeroPos(), ZeroRotation(), GameObjectTag::NONE);
    REQUIRE(overflow == nullptr);
}

// --- DestroyGameObject ---

TEST_CASE("Destroying a game object frees its pool slot for reuse") {
    GameObjectManager mgr;

    auto* first = mgr.CreateGameObject("first", ZeroPos(), ZeroRotation(), GameObjectTag::NONE);
    REQUIRE(first != nullptr);
    auto firstId = first->id();

    mgr.DestroyGameObject(first);

    auto* second = mgr.CreateGameObject("second", ZeroPos(), ZeroRotation(), GameObjectTag::NONE);
    REQUIRE(second != nullptr);
    REQUIRE(second->id() == firstId);
}

TEST_CASE("Destroyed game objects are excluded from GetActiveGameObjects") {
    GameObjectManager mgr;

    auto* obj = mgr.CreateGameObject("temp", ZeroPos(), ZeroRotation(), GameObjectTag::NONE);
    REQUIRE(obj != nullptr);

    mgr.DestroyGameObject(obj);

    // NOTE: this assumes GameObject::Destroy() resets m_id back to
    // INVALID_POOL_ID, matching the convention used elsewhere in the
    // engine (Billboard, TimFile). If Destroy() doesn't do that, this
    // fails for a different reason than the slot-reuse bug above.
    const auto& active = mgr.GetActiveGameObjects();
    for (const auto* o : active) {
        REQUIRE(o != obj);
    }
}

// --- GetGameObjectByName ---

TEST_CASE("GetGameObjectByName finds an object by its exact name") {
    GameObjectManager mgr;

    auto* created = mgr.CreateGameObject("torch_01", ZeroPos(), ZeroRotation(), GameObjectTag::INTERACTABLE);
    REQUIRE(created != nullptr);

    auto* found = mgr.GetGameObjectByName("torch_01");

    REQUIRE(found == created);
}

TEST_CASE("GetGameObjectByName returns nullptr for a name that doesn't exist") {
    GameObjectManager mgr;

    auto* found = mgr.GetGameObjectByName("does_not_exist");
    REQUIRE(found == nullptr);
}

TEST_CASE("GetGameObjectByName(hash) finds an object by its name hash") {
    GameObjectManager mgr;

    auto* created = mgr.CreateGameObject("crate_03", ZeroPos(), ZeroRotation(), GameObjectTag::ENVIRONMENT);
    REQUIRE(created != nullptr);

    auto* found = mgr.GetGameObjectByName(HashName("crate_03"));
    REQUIRE(found == created);
}

// --- GetActiveGameObjects / renderable list ---

TEST_CASE("GetActiveGameObjects prefers the renderable list when one is set") {
    GameObjectManager mgr;

    auto* a = mgr.CreateGameObject("a", ZeroPos(), ZeroRotation(), GameObjectTag::NONE);
    auto* b = mgr.CreateGameObject("b", ZeroPos(), ZeroRotation(), GameObjectTag::NONE);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);

    GameObject* renderList[] = {a};
    mgr.SetRenderableGameObjects(eastl::span<GameObject*>(renderList, 1));

    // documents existing behaviour rather than asserting it's necessarily
    // correct — GetActiveGameObjects() silently returns the renderable
    // list instead of the full active list whenever one has been set
    const auto& active = mgr.GetActiveGameObjects();
    REQUIRE(active.size() == 1);
    REQUIRE(active[0] == a);

    mgr.ClearRenderableGameObjects();
    const auto& activeAfterClear = mgr.GetActiveGameObjects();
    REQUIRE(activeAfterClear.size() == 2);
}

TEST_CASE("SetRenderableGameObjects filters out objects with an invalid id") {
    GameObjectManager mgr;

    auto* valid = mgr.CreateGameObject("valid", ZeroPos(), ZeroRotation(), GameObjectTag::NONE);
    REQUIRE(valid != nullptr);

    auto* gone = mgr.CreateGameObject("gone", ZeroPos(), ZeroRotation(), GameObjectTag::NONE);
    REQUIRE(gone != nullptr);
    mgr.DestroyGameObject(gone);

    GameObject* renderList[] = {valid, gone};
    mgr.SetRenderableGameObjects(eastl::span<GameObject*>(renderList, 2));

    const auto& renderable = mgr.GetActiveGameObjects();
    REQUIRE(renderable.size() == 1);
    REQUIRE(renderable[0] == valid);

    mgr.ClearRenderableGameObjects();
}

// --- GetGameObjectsWithTag ---

TEST_CASE("GetGameObjectsWithTag returns only objects matching that tag") {
    GameObjectManager mgr;

    auto* portalA = mgr.CreateGameObject("portal_a", ZeroPos(), ZeroRotation(), GameObjectTag::PORTAL);
    auto* env = mgr.CreateGameObject("rock", ZeroPos(), ZeroRotation(), GameObjectTag::ENVIRONMENT);
    auto* portalB = mgr.CreateGameObject("portal_b", ZeroPos(), ZeroRotation(), GameObjectTag::PORTAL);
    REQUIRE(portalA != nullptr);
    REQUIRE(env != nullptr);
    REQUIRE(portalB != nullptr);

    const auto& portals = mgr.GetGameObjectsWithTag(GameObjectTag::PORTAL);

    REQUIRE(portals.size() == 2);

    bool foundA = false, foundB = false;
    for (const auto* o : portals) {
        if (o == portalA) foundA = true;
        if (o == portalB) foundB = true;
    }
    REQUIRE(foundA);
    REQUIRE(foundB);
}

// --- Dump ---

TEST_CASE("Dump resets the pool so ids are handed out from scratch") {
    GameObjectManager mgr;

    auto* a = mgr.CreateGameObject("a", ZeroPos(), ZeroRotation(), GameObjectTag::NONE);
    auto* b = mgr.CreateGameObject("b", ZeroPos(), ZeroRotation(), GameObjectTag::NONE);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(a->id() == 0);
    REQUIRE(b->id() == 1);

    mgr.Dump();

    auto* c = mgr.CreateGameObject("c", ZeroPos(), ZeroRotation(), GameObjectTag::NONE);
    REQUIRE(c != nullptr);
    REQUIRE(c->id() == 0);
}

TEST_CASE("Dump clears a previously-set renderable list, not just the pool") {
    GameObjectManager mgr;

    auto* a = mgr.CreateGameObject("a", ZeroPos(), ZeroRotation(), GameObjectTag::NONE);
    REQUIRE(a != nullptr);

    GameObject* renderList[] = {a};
    mgr.SetRenderableGameObjects(eastl::span<GameObject*>(renderList, 1));

    // sanity check: renderable list is actually in effect before Dump
    REQUIRE(mgr.GetActiveGameObjects().size() == 1);

    mgr.Dump();

    auto* b = mgr.CreateGameObject("b", ZeroPos(), ZeroRotation(), GameObjectTag::NONE);
    REQUIRE(b != nullptr);

    // must reflect the fresh post-Dump state (just `b`), not the stale
    // renderable list pointing at a since-recycled slot
    const auto& active = mgr.GetActiveGameObjects();
    REQUIRE(active.size() == 1);
    REQUIRE(active[0] == b);
}