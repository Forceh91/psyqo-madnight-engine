#pragma once

#include "gameobject.hh"
#include "gameobject_defs.hh"

#include <EASTL/span.h>
#include <EASTL/array.h>
#include <EASTL/fixed_vector.h>
#include <psyqo/vector.hh>

static constexpr uint8_t MAX_GAME_OBJECTS = 250;

class GameObjectManager final
{
    static eastl::array<GameObject, MAX_GAME_OBJECTS> m_gameObjects;
    static eastl::fixed_vector<GameObject *, MAX_GAME_OBJECTS> m_activeGameObjects;
    static eastl::fixed_vector<GameObject *, MAX_GAME_OBJECTS> m_renderableGameObjects;

    static int16_t GetFreeIndex(void);

public:
    static GameObject* CreateGameObject(const eastl::string_view& name, const uint64_t& nameHash, const psyqo::Vec3& pos, const GameObjectRotation& rotation, const GameObjectTag& tag = GameObjectTag::NONE);
    static GameObject* CreateGameObject(const eastl::string_view& name, const psyqo::Vec3& pos, const GameObjectRotation& rotation, const GameObjectTag& tag = GameObjectTag::NONE) {
        return CreateGameObject(name, HashName(name), pos, rotation, tag);
    }
    static void DestroyGameObject(GameObject* gameObject);
    static const eastl::fixed_vector<GameObject*, MAX_GAME_OBJECTS> &GetActiveGameObjects(void);
    static void ClearRenderableGameObjects(void);
    static void SetRenderableGameObjects(const eastl::span<GameObject*> renderList);
    static const eastl::fixed_vector<GameObject*, MAX_GAME_OBJECTS>& GetGameObjectsWithTag(GameObjectTag tag);
    static const eastl::array<GameObject, MAX_GAME_OBJECTS>& GetGameObjects(void) { return m_gameObjects; }
    static GameObject* GetGameObjectByName(const char* name);
    static GameObject* GetGameObjectByName(uint64_t nameHash);
    static void Dump(void);
};
