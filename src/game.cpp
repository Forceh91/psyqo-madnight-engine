#include "game.hh"

#include <EASTL/vector.h>
#include "core/object/gameobject_manager.hh"
#include "scenes/gameplay.hh"
#include "madnight.hh"

using namespace psyqo::fixed_point_literals;
using namespace psyqo::trig_literals;

// you can either use this or replace it with your own
static GameplayScene gameplayScene;

// once the engine is ready it will call this function
// as an entry point into your own game code
// it has been created as a coroutine so you can await file loading if needed
psyqo::Coroutine<> MadnightEngineGame::InitialLoad(void)
{
    eastl::vector<LoadQueue> queue = {
        {.name = "TEXTURES/STREET.TIM", .type = LoadFileType::TEXTURE, .x = 320, .y = 0, .clutX = 0, .clutY = 240},
        {.name = "MODELS/STREET.MB", .type = LoadFileType::OBJECT},
    };

    // show loading screen
    co_await g_madnightEngine.HardLoadingScreen(eastl::move(queue), &gameplayScene);

    // create a game object
    auto gameObject = GameObjectManager::CreateGameObject("STREET", {0, 0, 0}, {0, 0, 0}, GameObjectTag::ENVIRONMENT);
    if (gameObject != nullptr)
    {
        gameObject->SetQuadType(GameObjectQuadType::GouraudTextureQuad);
        gameObject->SetMesh("MODELS/STREET.MB");
        gameObject->SetTexture("TEXTURES/STREET.TIM");
    }

    auto gameObject2 = GameObjectManager::CreateGameObject("STREET2", {0.05_fp, 0, 0.5_fp}, {0, 1.0_pi, 0.25_pi}, GameObjectTag::ENVIRONMENT);
    if (gameObject2 != nullptr)
    {
        gameObject2->SetQuadType(GameObjectQuadType::GouraudTextureQuad);
        gameObject2->SetMesh("MODELS/STREET.MB");
        gameObject2->SetTexture("TEXTURES/STREET.TIM");
    }
}
