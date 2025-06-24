#ifndef _GAMEPLAY_SCENE_H
#define _GAMEPLAY_SCENE_H

#include "psyqo/coroutine.hh"
#include "psyqo/scene.hh"

#include "../ui/hud/gameplay_hud.hh"

class GameplayScene final : public psyqo::Scene
{
    void start(StartReason reason) override;
    void frame() override;

    GameplayHUD m_debugHUD = GameplayHUD("Debug HUD", {.pos = {5, 10}, .size = {100, 100}});
    TextHUDElement *m_heapSizeText = nullptr;
    TextHUDElement *m_fpsText = nullptr;

public:
};

#endif
