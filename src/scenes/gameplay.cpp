#include "gameplay.hh"
#include "../render/renderer.hh"
#include "../core/debug/debug_menu.hh"
#include "../render/camera.hh"

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
}
