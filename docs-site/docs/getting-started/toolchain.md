---
title: Toolchain Setup
sidebar_position: 2
---

# Toolchain Setup

Before touching the engine, set up the PS1 dev toolchain. It isn't bundled with the engine — follow the **"The toolchain"** section of the official [psyqo Getting Started guide](https://github.com/grumpycoders/pcsx-redux/blob/main/src/mips/psyqo/GETTING_STARTED.md#the-toolchain) first.

At minimum you'll need:

- A MIPS cross-compiler toolchain (`mips-linux-gnu` or similar) on your `PATH`
- `make`
- `git`
- An emulator that can run `.ps-exe` files, e.g. [PCSX-Redux](https://github.com/grumpycoders/pcsx-redux) or [DuckStation](https://github.com/stenzek/duckstation), for testing builds

Once that's in place, move on to [scaffolding a game](./scaffolding-a-game).
