#include "mygame.hh"
#include "madnight.hh"
#include "game.hh"
#include "scenes/gameplay.hh"
#include "psyqo/xprintf.h"

MadnightGame g_myGame;
MadnightEngineGame &g_madnightEngineGame = g_myGame;
static GameplayScene gameplayScene;

psyqo::Coroutine<> MadnightGame::InitialLoad(void)
{
    printf("welcome to your game code!\n");
    g_madnightEngine.SwitchScene(&gameplayScene); // show the default gameplay scene so something happens visually
    co_return;                                    // let the engine know that we're done with our initial loading
}

int main() { return g_madnightEngine.run(); }
