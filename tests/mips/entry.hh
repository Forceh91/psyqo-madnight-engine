#pragma once

#include "game.hh"
#include "psyqo/coroutine.hh"

class TestHarness final : public MadnightEngineGame {
    psyqo::Coroutine<> InitialLoad(void) override { co_return; };
};

extern TestHarness g_testHarness;

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

