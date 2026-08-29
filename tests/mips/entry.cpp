#include "entry.hh"
#include "common/hardware/pcsxhw.h"
#include "common/syscalls/syscalls.h"

#include "snitch_all.hpp"

TestHarness g_testHarness;
MadnightEngineGame &g_madnightEngineGame = g_testHarness;

static void psyqo_console_print(std::string_view message) noexcept {
    for (char c : message) {
        syscall_putchar(c);
    }
}

int main() {
    snitch::cli::console_print = &psyqo_console_print;
    snitch::tests.print_callback = &psyqo_console_print;

    bool success = snitch::tests.run_tests("psyqo");

    if (success) {
        ramsyscall_printf("All tests passed!\n");
    } else {
        ramsyscall_printf("Some tests FAILED!\n");
    }

    // Signal to the emulator via exit code.
    pcsx_exit(success ? 0 : 1);
    return success ? 0 : 1;
}
