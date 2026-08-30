#pragma once
#include "helpers/load_queue.hh"
#include "psyqo/scene.hh"
#include "rand.hh"

#include "psyqo/coroutine.hh"
#include "psyqo/application.hh"
#include "psyqo/trigonometry.hh"
#include "psyqo/advancedpad.hh"
#include "EASTL/vector.h"

class MadnightEngine final : public psyqo::Application
{
    void prepare() override;

    psyqo::Coroutine<> m_initialLoadRoutine;
public:
    psyqo::Trig<> m_trig;
    psyqo::AdvancedPad m_input;

    // make sure you seed this with a nice number when starting your first scene
    // using `g_madnightEngine.gpu().now()` for example.
    Rand m_rand;

    // you probably want to use hardloadingscreen below, but if you have no files to load then no need, just use this instead
    // this will pop the existing scene unless you specify `true` for keepPreviousscene
    psyqo::Scene* SwitchScene(psyqo::Scene *scene, bool keepPrevious = false);

    // shows a loading screen and unloads all known meshes and textures
    // this will also unload the current scene, and start the requested scene fresh, so if you have
    // stuff that occurs in startscene, you need to make sure you don't do it again
    // as this will trigger a `Start` reason, and not `Resume`
    psyqo::Coroutine<> HardLoadingScreen(eastl::vector<LoadQueue>&& files, psyqo::Scene* postLoadScene);

    // show your custom loading screen and unloads all known meshes, textures, etc.
    // on top of that the current scene is unloaded and replaced with what is specified in `loadingScene`
    psyqo::Coroutine<> HardLoadingScreen(eastl::vector<LoadQueue>&& files, psyqo::Scene* loadingScene, psyqo::Scene* postLoadScene);

    // shows a loading screen but keeps your current scene on the stack, which will be returned to once loading is done
    // this will not unload any data from pools such as meshes, textures, game objects, etc.
    psyqo::Coroutine<> SoftLoadingScreen(eastl::vector<LoadQueue>&& files);
    // shows your custom loading screen but keeps your current scene on the stack, which will be returned to once loading is done
    // this will not unload any data from pools such as meshes, textures, game objects, etc.
    psyqo::Coroutine<> SoftLoadingScreen(eastl::vector<LoadQueue>&& files, psyqo::Scene* loadingScene);

    /*
     * this is the very first thing that is called once the engine is initialized and ready
     * for use. you can do whatever you want with this here. for example you can make use of `co_await HardLoadingScreen` in
     * this function before you do whatever you need/want to do in your actual game code.
     */
    psyqo::Coroutine<> InitialLoad(void);
};

// Trap into the resident debugger with the exit code in $a0 via break
// category 4. Categories 0/6/7/14 are taken (pcdrv / compiler overflow /
// compiler divide-by-zero / psyqo), so 4 is free. On hardware this halts
// the program (Unirom reports HLTD) and leaves the exit code readable in
// $a0, giving the host a deterministic end-of-binary signal instead of a
// printed sentinel string. On the emulator pcsx_exit() has already exited,
// so this is never reached there.    
static inline void exitBreak(int code) {
    register int a0 asm("$4") = code;
    __asm__ volatile("break 4, 0\n" : : "r"(a0) : "memory");
}

extern MadnightEngine g_madnightEngine;

// PCDRV debug builds (-pcdrv) mount an ISO from your host filesystem instead
// of real CD-ROM hardware. Each game must define this exactly once,
// somewhere in its own source (not the engine's) — e.g.:
//     const char* g_gameIsoName = "ssweep.iso";
extern const char* g_gameIsoName;
