#include "gameplay.hh"
#include "../render/renderer.hh"
#include "../core/debug/debug_menu.hh"
#include "../render/camera.hh"
#include "../core/raycast.hh"
#include "../core/collision.hh"
#include "../core/object/gameobject_manager.hh"
#include "psyqo/xprintf.h"

void GameplayScene::start(StartReason reason)
{
    Renderer::Instance().StartScene();
}

void GameplayScene::frame()
{
    auto &renderInstance = Renderer::Instance();
    uint32_t deltaTime = renderInstance.Process();

    // process camera inputs
    CameraManager::process(deltaTime);

    // process debug menu
    DebugMenu::Process();

    // the central point for rendering gameobjects etc
    renderInstance.Render();

    // raycast
    uint8_t raycastDistance = DebugMenu::RaycastDistance();
    Ray ray = {.origin = CameraManager::get_pos(), .direction = CameraManager::GetForwardVector(), .maxDistance = raycastDistance * ONE_METRE};
    RayHit hit = {0};

    // bool didHit = Raycast::RaycastScene(ray, GameObjectTag::ENVIRONMENT, &hit);
    // printf("did hit=%d\n", didHit);

    // collision detection test...
    // auto objects = GameObjectManager::GetGameObjectsWithTag(GameObjectTag::ENVIRONMENT);
    // bool collision = Collision::IsSATCollision(objects[0]->obb(), objects[1]->obb());
    // printf("collision=%d\n", collision);
}
