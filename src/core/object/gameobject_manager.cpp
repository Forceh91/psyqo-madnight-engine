#include "gameobject_manager.hh"

#include <EASTL/span.h>

Pool<GameObject, MAX_GAME_OBJECTS> GameObjectManager::m_pool;
eastl::fixed_vector<GameObject*, MAX_GAME_OBJECTS> GameObjectManager::m_activeGameObjects;
eastl::fixed_vector<GameObject*, MAX_GAME_OBJECTS> GameObjectManager::m_renderableGameObjects;

GameObject *GameObjectManager::CreateGameObject(const char* name, const psyqo::Vec3& pos, const GameObjectRotation& rotation, GameObjectTag tag)
{
    // do we have space in the game objects for this?
    auto id = m_pool.Acquire();
    if (id == INVALID_POOL_ID)
        return nullptr;

    // we do, lets create a new instance and add it
    auto* gameObject = m_pool.Get(id);
    gameObject->Init(name, pos, rotation, tag, id);
    return gameObject;
}

void GameObjectManager::DestroyGameObject(GameObject* object)
{
    if (object != nullptr)
        object->Destroy();
}

const eastl::fixed_vector<GameObject*, MAX_GAME_OBJECTS>& GameObjectManager::GetActiveGameObjects(void)
{
    // take renderable game objects first if we have them
    if (m_renderableGameObjects.size() > 0)
        return m_renderableGameObjects;

    m_activeGameObjects.clear();

    // get all game objects that are actually initialized
    auto size = m_pool.size();
    for (int i = 0; i < size; i++) {
        auto* gameObject = m_pool.Get(i);
        if (gameObject && gameObject->id() != INVALID_POOL_ID)
            m_activeGameObjects.push_back(gameObject);
    }

    return m_activeGameObjects;
}

void GameObjectManager::ClearRenderableGameObjects(void) {
    m_renderableGameObjects.clear();
}

void GameObjectManager::SetRenderableGameObjects(const eastl::span<GameObject*> renderList) {
    m_renderableGameObjects.clear();

    for (const auto &object : renderList) {
        if (object->id() != INVALID_POOL_ID)
            m_renderableGameObjects.push_back(object);
    }
}

// TODO: should this be its own list outside of `m_activeGameObjects`
const eastl::fixed_vector<GameObject*, MAX_GAME_OBJECTS>& GameObjectManager::GetGameObjectsWithTag(GameObjectTag tag)
{
    m_activeGameObjects.clear();

    auto size = m_pool.size();
    for (int i = 0; i < size; i++) {
        auto* gameObject = m_pool.Get(i);
        if (gameObject && gameObject->id() != INVALID_POOL_ID && gameObject->tag() == tag)
            m_activeGameObjects.push_back(gameObject);
    }

    return m_activeGameObjects;
}

GameObject* GameObjectManager::GetGameObjectByName(const char* name)
{
    return GetGameObjectByName(HashName(name));
}

GameObject* GameObjectManager::GetGameObjectByName(uint64_t nameHash)
{
    // find the first game object that matches this name
    auto size = m_pool.size();
    for (int i = 0; i < size; i++) {
        auto* gameObject = m_pool.Get(i);
        if (gameObject && gameObject->id() != INVALID_POOL_ID && gameObject->nameHash() == nameHash)
            m_activeGameObjects.push_back(gameObject);
    }
    return nullptr;
}

void GameObjectManager::Dump(void) {
    auto size = m_pool.size();
    for (int i = 0; i < size; i++) {
        auto* gameObject = m_pool.Get(i);
        if (gameObject && gameObject->id() != INVALID_POOL_ID)
            gameObject->Destroy();
    }
    m_pool.Dump();
}
