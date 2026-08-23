---
title: Scaffolding a Game
sidebar_position: 3
---

# Scaffolding a Game

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

Copy the engine's `getting-started/Makefile` and `getting-started/src` folder into the root of your game repo:

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

`mygame.cpp` is where the engine hands control over to your game. Out of the box it does the bare minimum — it registers itself with the engine and switches to a scene on startup:

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

`SwitchScene` returns a pointer to the scene it popped (or `nullptr` if `keepPrevious` was `true`) — useful if that scene was heap-allocated and you want to `delete` it once you're done with it, rather than leaking it.

`GameplayScene` above is just the engine's built-in placeholder scene (`madnight_engine/src/scenes/gameplay.hh`) — swap it out for your own scene once you're ready to build real content. Add any new `.cpp` files under `src/` and the Makefile will pick them up automatically (it does a recursive find).

Next: [build and run it](./building-and-running).
