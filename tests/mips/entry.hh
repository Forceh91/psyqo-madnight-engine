#pragma once

#include "game.hh"
#include "psyqo/coroutine.hh"

class TestHarness final : public MadnightEngineGame {
    psyqo::Coroutine<> InitialLoad(void) override { co_return; };
};

extern TestHarness g_testHarness;
