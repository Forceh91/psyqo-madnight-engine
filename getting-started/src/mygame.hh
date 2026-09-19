#pragma once

#include "game.hh"
#include "psyqo/coroutine.hh"

class MadnightGame final : public MadnightEngineGame
{
public:
    psyqo::Coroutine<> InitialLoad(void) override;
};
