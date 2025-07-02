#ifndef _GAME_HH
#define _GAME_HH

#include "psyqo/coroutine.hh"

class MadnightEngineGame final
{
public:
    static psyqo::Coroutine<> InitialLoad(void);
};

#endif