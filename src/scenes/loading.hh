#ifndef _LOADING_SCENE_H
#define _LOADING_SCENE_H

#include "psyqo/coroutine.hh"
#include "psyqo/scene.hh"

class LoadingScene final : public psyqo::Scene
{
    void start(StartReason reason) override;
    void frame() override;

public:
    psyqo::Coroutine<> InitializeLoading(void); // todo: add list of files to load
};

#endif
