#include "gameobject_manager.hh"

eastl::array<GameObject, MAX_GAME_OBJECTS> GameObjectManager::m_gameObjects;

GameObject *GameObjectManager::CreateGameObject(const char *name, psyqo::Vec3 pos, GameObjectRotation rotation, GameObjectTag tag)
{
    // do we have space in the game objects for this?
    int8_t freeIx = GetFreeIndex();
    if (freeIx == -1)
        return nullptr;

    // we do, lets create a new instance and add it
    m_gameObjects[freeIx] = GameObject(name, pos, rotation, tag);
    return &m_gameObjects[freeIx];
}

int8_t GameObjectManager::GetFreeIndex(void)
{
    for (uint8_t i = 0; i < MAX_GAME_OBJECTS; i++)
    {
        if (m_gameObjects.at(i).name().empty())
            return i;
    };

    return -1;
}

void GameObjectManager::DestroyGameObject(GameObject *object)
{
    if (object != nullptr)
        object->Destroy();
}

eastl::vector<GameObject *> GameObjectManager::GetGameObjects(void)
{
    // get all game objects that are actually initialized
    eastl::vector<GameObject *> activeGameObjects;

    for (uint8_t i = 0; i < MAX_GAME_OBJECTS; i++)
    {
        if (!m_gameObjects.at(i).name().empty())
            activeGameObjects.push_back(&m_gameObjects.at(i));
    };

    return eastl::move(activeGameObjects);
}

eastl::vector<GameObject *> GameObjectManager::GetGameObjectsWithTag(GameObjectTag tag)
{
    // get all game objects that are actually initialized
    eastl::vector<GameObject *> gameObjectsWithTag;

    for (uint8_t i = 0; i < MAX_GAME_OBJECTS; i++)
    {
        if (!m_gameObjects.at(i).name().empty() && m_gameObjects.at(i).tag() == tag)
            gameObjectsWithTag.push_back(&m_gameObjects.at(i));
    };

    return eastl::move(gameObjectsWithTag);
}

GameObject *GameObjectManager::GetGameObjectByName(const char *name)
{
    // find the first game object that matches this name
    for (uint8_t i = 0; i < MAX_GAME_OBJECTS; i++)
    {
        if (m_gameObjects.at(i).name() == name)
            return &m_gameObjects.at(i);
    };

    return nullptr;
}
