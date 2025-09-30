#include "gameplay.hh"
#include "../core/collision.hh"
#include "../core/debug//perf_monitor.hh"
#include "../core/debug/debug_menu.hh"
#include "../core/object/gameobject_manager.hh"
#include "../core/raycast.hh"
#include "../render/colour.hh"
#include "../render/renderer.hh"
#include "../sound/sound_manager.hh"
#include "psyqo/alloc.h"
#include "psyqo/xprintf.h"

void GameplayScene::start(StartReason reason) {
  Renderer::Instance().StartScene();

  g_madnightEngine.m_input.setOnEvent([&](auto event) {
    if (event.type != psyqo::AdvancedPad::Event::ButtonReleased)
      return;
    if (event.button == psyqo::AdvancedPad::Button::Start)
      m_menu.Activate();
  });

  // the below only needs to happen if this was a freshly created scene
  if (reason != StartReason::Create)
    return;

  m_heapSizeText = m_debugHUD.AddTextHUDElement(TextHUDElement("HEAP", {.pos = {5, 0}, .size = {100, 100}}));
  m_fpsText = m_debugHUD.AddTextHUDElement(TextHUDElement("FPS", {.pos = {5, 15}, .size = {100, 100}}));

  m_camera = new Camera();
}

void GameplayScene::teardown(TearDownReason reason) { g_madnightEngine.m_input.setOnEvent(nullptr); }

void GameplayScene::frame() {
  auto &renderInstance = Renderer::Instance();
  auto &gpu = Renderer::Instance().GPU();
  uint32_t deltaTime = renderInstance.Process();
  if (deltaTime == 0)
    return;

  // process camera inputs
  m_camera->Process(deltaTime);

  // process debug menu
  DebugMenu::Process();

  // raycast
  const auto &raycastDistance = DebugMenu::RaycastDistance();
  Ray ray = {
      .origin = m_camera->pos(), .direction = m_camera->forwardVector(), .maxDistance = raycastDistance * ONE_METRE};
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
    PerfMonitor::Render(deltaTime);
}
