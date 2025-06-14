#ifndef _GAMEPLAY_SCENE_H
#define _GAMEPLAY_SCENE_H

#include "psyqo/coroutine.hh"
#include "psyqo/scene.hh"

class GameplayScene final : public psyqo::Scene
{
    void start(StartReason reason) override;
    void frame() override;

public:
};

#endif
