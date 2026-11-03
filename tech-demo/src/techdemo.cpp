/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "game.hh"
#include "hall_scene.hh"
#include "madnight.hh"

#include <EASTL/vector.h>
#include <psyqo/xprintf.h>

// the disc image a PCDRV build reads from the host, see tools/make_image.py
const char* g_gameIsoName = "techdemo.bin";

class TechDemo final : public MadnightEngineGame {
  public:
	psyqo::Coroutine<> InitialLoad(void) override;
};

TechDemo g_techDemo;
MadnightEngineGame& g_madnightEngineGame = g_techDemo;
static HallScene hallScene;

psyqo::Coroutine<> TechDemo::InitialLoad(void) {
	printf("TECHDEMO loading\n");
	// one manifest pulls in every texture, mesh and animation the scene uses
	eastl::vector<LoadQueue> files = {{.name = "DEMO.SCN", .type = LoadFileType::SCENE}};
	co_await g_madnightEngine.HardLoadingScreen(eastl::move(files), &hallScene);
	printf("TECHDEMO loaded\n");
}

int main() { return g_madnightEngine.run(); }
