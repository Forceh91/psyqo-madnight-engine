#include "gameplay.hh"
#include "../render/renderer.hh"
#include "../render/colour.hh"
#include "../core/debug/debug_menu.hh"
#include "../render/camera.hh"
#include "../core/raycast.hh"
#include "../core/collision.hh"
#include "../core/object/gameobject_manager.hh"
#include "../sound/sound_manager.hh"
#include "psyqo/alloc.h"
#include "psyqo/xprintf.h"

void GameplayScene::start(StartReason reason)
{
    Renderer::Instance().StartScene();

    g_madnightEngine.m_input.setOnEvent([&](auto event)
                                        {
                                            if (event.type != psyqo::AdvancedPad::Event::ButtonReleased)
                                                return;
                                            if (event.button == psyqo::AdvancedPad::Button::Start)
                                                m_menu.Activate();
                                            if (event.button == psyqo::AdvancedPad::Button::Up)
                                                SoundManager::PlaySoundEffect(4, 28, 30, 63);
                                            if (event.button == psyqo::AdvancedPad::Button::L2)
                                                SoundManager::PlayMusic();
                                            if (event.button == psyqo::AdvancedPad::Button::R2)
                                                SoundManager::PauseMusic(); });

    // the below only needs to happen if this was a freshly created scene
    if (reason != StartReason::Create)
        return;

    m_heapSizeText = m_debugHUD.AddTextHUDElement(TextHUDElement("HEAP", {.pos = {5, 0}, .size = {100, 100}}));
    m_fpsText = m_debugHUD.AddTextHUDElement(TextHUDElement("FPS", {.pos = {5, 15}, .size = {100, 100}}));
}

void GameplayScene::teardown(TearDownReason reason)
{
    g_madnightEngine.m_input.setOnEvent(nullptr);
}

void GameplayScene::frame()
{
    auto &renderInstance = Renderer::Instance();
    uint32_t deltaTime = renderInstance.Process();

    // process camera inputs
    CameraManager::process(deltaTime);

    // process debug menu
    DebugMenu::Process();

    auto camPos = CameraManager::get_pos();

    // raycast
    uint8_t raycastDistance = DebugMenu::RaycastDistance();
    Ray ray = {.origin = camPos, .direction = CameraManager::GetForwardVector(), .maxDistance = raycastDistance * ONE_METRE};
    RayHit hit = {0};

    // bool didHit = Raycast::RaycastScene(ray, GameObjectTag::ENVIRONMENT, &hit);
    // printf("did hit=%d\n", didHit);

    // collision detection test...
    // auto objects = GameObjectManager::GetGameObjectsWithTag(GameObjectTag::ENVIRONMENT);
    // bool collision = Collision::IsSATCollision(objects[0]->obb(), objects[1]->obb());
    // printf("collision=%d\n", collision);

    // the central point for rendering gameobjects etc
    renderInstance.Render();

    if (DebugMenu::IsEnabled())
        return;

    if (DebugMenu::DisplayDebugHUD())
    {
        char heapSize[GAMEPLAY_HUD_ELEMENT_MAX_STR_LEN];
        snprintf(heapSize, GAMEPLAY_HUD_ELEMENT_MAX_STR_LEN, "Heap Used: %d", (int)((uint8_t *)psyqo_heap_end() - (uint8_t *)psyqo_heap_start()));
        m_heapSizeText->SetDisplayText(heapSize);

        char fps[GAMEPLAY_HUD_ELEMENT_MAX_STR_LEN];
        snprintf(fps, GAMEPLAY_HUD_ELEMENT_MAX_STR_LEN, "FPS: %d", renderInstance.GPU().getRefreshRate() / deltaTime);
        m_fpsText->SetDisplayText(fps);

        m_debugHUD.Render();
    }
}
