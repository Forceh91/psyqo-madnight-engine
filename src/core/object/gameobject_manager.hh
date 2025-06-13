#ifndef _GAMEOBJECT_MANAGER_H
#define _GAMEOBJECT_MANAGER_H

#include "gameobject.hh"
#include "gameobject_defs.hh"

#include "EASTL/array.h"
#include "EASTL/vector.h"
#include "psyqo/vector.hh"

static constexpr uint8_t MAX_GAME_OBJECTS = 200;

class GameObjectManager final
{
    static eastl::array<GameObject, MAX_GAME_OBJECTS> m_gameObjects;

    static int8_t GetFreeIndex(void);

public:
    static GameObject *CreateGameObject(const char *name, psyqo::Vec3 pos, GameObjectRotation rotation, GameObjectTag tag = GameObjectTag::NONE);
    static void DestroyGameObject(GameObject *gameObject);
    static eastl::vector<GameObject *> GetGameObjects(void);
    static eastl::vector<GameObject *> GetGameObjectsWithTag(GameObjectTag tag);
};

#endif
