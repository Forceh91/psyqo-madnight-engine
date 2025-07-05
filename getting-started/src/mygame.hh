#ifndef _MADNIGHT_GAME_H
#define _MADNIGHT_GAME_H

#include "game.hh"
#include "psyqo/coroutine.hh"

class MadnightGame final : public MadnightEngineGame
{
public:
    psyqo::Coroutine<> InitialLoad(void) override;
};

#endif
