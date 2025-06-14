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

    // shows a loading screen and unloads all known meshes and textures
    psyqo::Coroutine<> HardLoadGameplayScene(eastl::vector<LoadQueue> files);

public:
    psyqo::Trig<> m_trig;
    psyqo::AdvancedPad m_input;
};

extern MadnightEngine g_madnightEngine;

#endif
