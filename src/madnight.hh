#ifndef _HELLO3D_H_
#define _HELLO3D_H_

#include "helpers/load_queue.hh"
#include "rand.hh"

#include "psyqo/coroutine.hh"
#include "psyqo/application.hh"
#include "psyqo/trigonometry.hh"
#include "psyqo/advancedpad.hh"
#include "EASTL/vector.h"

class MadnightEngine final : public psyqo::Application
{
    void prepare() override;
    void createScene() override;

    psyqo::Coroutine<> m_initialLoadRoutine;

public:
    psyqo::Trig<> m_trig;
    psyqo::AdvancedPad m_input;

    // make sure you seed this with a nice number when starting your first scene
    // using `g_madnightEngine.gpu().now()` for example.
    Rand m_rand;

    // you probably want to use hardloadingscreen below, but if you have no files to load then no need, just use this instead
    // this will pop the existing scene unless you specify `true` for keepPreviousscene
    void SwitchScene(psyqo::Scene *scene, bool keepPrevious = false);

    // shows a loading screen and unloads all known meshes and textures
    // this will also unload the current scene, and start the requested scene fresh, so if you have
    // stuff that occurs in startscene, you need to make sure you don't do it again
    // as this will trigger a `Start` reason, and not `Resume`
    psyqo::Coroutine<> HardLoadingScreen(eastl::vector<LoadQueue> &&files, psyqo::Scene *postLoadScene);

    /*
     * this is the very first thing that is called once the engine is initialized and ready
     * for use. you can do whatever you want with this here. for example you can make use of `co_await HardLoadingScreen` in
     * this function before you do whatever you need/want to do in your actual game code.
     */
    psyqo::Coroutine<> InitialLoad(void);
};

extern MadnightEngine g_madnightEngine;

#endif
