---
title: Getting Started
sidebar_position: 1
---

# Getting Started

The engine is a library that links into your game code — you don't build a game *inside* this repo. Instead you scaffold a **separate game repository**, pull the engine in as a git submodule, and build from there.

This section walks through that flow end to end:

1. [Set up the toolchain](./toolchain)
2. [Scaffold a game repo on top of the engine](./scaffolding-a-game)
3. [Build and run it](./building-and-running)
4. [Load assets dynamically](./loading-assets) (optional, once hardcoded headers stop cutting it)
5. [Keep the engine up to date](./updating-the-engine)

If you get through all of that and want to go deeper, the best next stop is the [API Reference](../api/overview) — particularly `src/scenes`, `src/core`, and `src/render`, which is where scenes, game objects, and the camera all connect.
