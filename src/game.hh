#ifndef _GAME_HH
#define _GAME_HH

#include "psyqo/coroutine.hh"

class MadnightEngineGame
{
public:
    virtual psyqo::Coroutine<> InitialLoad(void);
};

#endif