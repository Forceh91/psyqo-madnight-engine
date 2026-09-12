#pragma once
#include "psyqo/coroutine.hh"

class MadnightEngineGame
{
public:
    virtual psyqo::Coroutine<> InitialLoad(void) = 0;
};

extern MadnightEngineGame &g_madnightEngineGame;
