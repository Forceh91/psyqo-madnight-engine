# Getting Started

This folder contains the scaffold you need to start a new game on top of the Madnight Engine: a `Makefile` and a minimal `src/` directory with a game entry point. The engine itself is meant to be pulled in as a git submodule into your own, separate game repo — not built from inside this repo.

## Prerequisites

Before you start, set up the PS1 dev toolchain. This isn't bundled with the engine, so follow the **"The toolchain"** section of the official [psyqo Getting Started guide](https://github.com/grumpycoders/pcsx-redux/blob/main/src/mips/psyqo/GETTING_STARTED.md#the-toolchain) first. At minimum you'll need:

- A MIPS cross-compiler toolchain (`mips-linux-gnu` or similar) on your `PATH`
- `make`
- `git`
- An emulator that can run `.ps-exe` files (e.g. [PCSX-Redux](https://github.com/grumpycoders/pcsx-redux) or [DuckStation](https://github.com/stenzek/duckstation)) for testing builds

## 1. Create your game repo

Make a new folder for your game's code and initialize it as its own git repository:

```bash
mkdir my-game && cd my-game
git init
```

## 2. Add the engine as a submodule

The engine is pulled in as a submodule, which in turn pulls in its own `nugget`/`psyqo` submodule:

```bash
git submodule add https://github.com/Forceh91/psyqo-madnight-engine madnight_engine
git submodule update --init --recursive
```

Your folder should now look like this:

```
my-game/
└── madnight_engine/   ← the engine, as a submodule
```

## 3. Scaffold your game code

Copy this folder's `Makefile` and `src` folder into the root of your game repo:

```bash
cp madnight_engine/getting-started/Makefile .
cp -r madnight_engine/getting-started/src .
```

You should end up with:

```
my-game/
├── madnight_engine/       ← engine submodule
├── src/
│   ├── mygame.cpp         ← your game's entry point
│   └── mygame.hh
└── Makefile
```

`mygame.cpp` is where the engine hands control over to your game. Out of the box it does the bare minimum: it registers itself with the engine and switches to a scene on startup.

```cpp
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
```

`GameplayScene` above is just the engine's built-in placeholder scene (`madnight_engine/src/scenes/gameplay.hh`) — swap it out for your own scene once you're ready to build real content. Add any new `.cpp` files under `src/` and the Makefile will pick them up automatically (it does a recursive find).

## 4. Build

From your game repo's root:

```bash
make
```

If everything's set up correctly, this produces:

```
build/madnight-engine-game.ps-exe
```

along with the matching `.elf` and `.map` files.

## 5. Run it

Drop `build/madnight-engine-game.ps-exe` into your emulator of choice. If it worked, you'll see a grey scene with a debug HUD overlay showing FPS and heap usage — that's the engine's default gameplay scene confirming the whole chain (toolchain → engine → your code) is wired up correctly.

## Loading assets

If you want to dynamically load assets (rather than hardcoding them as header files), use pcsx-redux's [authoring tool](https://github.com/grumpycoders/pcsx-redux/tree/main/tools/authoring) to generate your ISO. The [`../cdrom/toc.json`](../cdrom/toc.json) file in this repo is an example you can use as a starting point.

## Updating the engine

Since the engine is a submodule, pull in upstream changes with:

```bash
cd madnight_engine
git pull origin main
cd ..
git add madnight_engine
git commit -m "Update madnight_engine submodule"
```

## Where to look next

Since docs are sparse beyond this point, the best reference is the engine source itself — particularly `src/scenes`, `src/core`, and `src/render` — for how scenes, game objects, and the camera fit together.
