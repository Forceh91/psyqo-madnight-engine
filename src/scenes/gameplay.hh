#ifndef _GAMEPLAY_SCENE_H
#define _GAMEPLAY_SCENE_H

#include "psyqo/coroutine.hh"
#include "psyqo/scene.hh"

#include "../ui/hud/gameplay_hud.hh"
#include "../ui/menu/menu.hh"

/*
 * the gameplay scene is an example scene that shows
 * how to use various aspects of the renderer, debug menu,
 * input events etc. etc.
 * it will render all your gameobjects and textures via `Renderer::Render`
 * you can build off of this scene or create your own
 */
class GameplayScene final : public psyqo::Scene
{
    void start(StartReason reason) override;
    void teardown(TearDownReason reason) override;
    void frame() override;

    GameplayHUD m_debugHUD = GameplayHUD("Debug HUD", {.pos = {5, 10}, .size = {100, 100}});
    TextHUDElement *m_heapSizeText = nullptr;
    TextHUDElement *m_fpsText = nullptr;

    Menu m_menu;

public:
};

#endif
