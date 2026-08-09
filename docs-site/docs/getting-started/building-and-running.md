---
title: Building & Running
sidebar_position: 4
---

# Building & Running

## Build

From your game repo's root:

```bash
make
```

If everything's set up correctly, this produces:

```
build/madnight-engine-game.ps-exe
```

along with the matching `.elf` and `.map` files.

## Run it

Drop `build/madnight-engine-game.ps-exe` into your emulator of choice. If it worked, you'll see a grey scene with a debug HUD overlay showing FPS and heap usage — that's the engine's default gameplay scene confirming the whole chain (toolchain → engine → your code) is wired up correctly.

Next up: [loading assets dynamically](./loading-assets), or jump straight to [updating the engine](./updating-the-engine) when you need to pull in upstream changes later.
