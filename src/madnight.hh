#ifndef _HELLO3D_H_
#define _HELLO3D_H_

#include "helpers/load_queue.hh"

#include "psyqo/coroutine.hh"
#include "psyqo/application.hh"
#include "psyqo/trigonometry.hh"
#include "psyqo/advancedpad.hh"
#include "EASTL/vector.h"

class MadnightEngine final : public psyqo::Application
{
    void prepare() override;
    void createScene() override;

    psyqo::Coroutine<> InitialLoad(void);

    // shows a loading screen and unloads all known meshes and textures
    psyqo::Coroutine<> HardLoadingScreen(eastl::vector<LoadQueue> files);
    void SwitchToGameplay(void);

public:
    psyqo::Trig<> m_trig;
    psyqo::AdvancedPad m_input;
};

extern MadnightEngine g_madnightEngine;

#endif
