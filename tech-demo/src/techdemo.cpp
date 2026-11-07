/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "game.hh"
#include "hall_scene.hh"
#include "madnight.hh"
#include "render/renderer.hh"

#include <EASTL/vector.h>
#include <psyqo/xprintf.h>

// the disc image a PCDRV build reads from the host, see tools/make_image.py
const char* g_gameIsoName = "techdemo.bin";

// a custom loading screen: pass it to HardLoadingScreen/SoftLoadingScreen in place of the engine's
// LoadingScene. with PCDRV the load is over in a few frames, from a disc it stays up longer
class TechDemoLoadingScene final : public psyqo::Scene {
	void start(StartReason reason) override { Renderer::Instance().StartScene(); }
	void frame() override {
		auto& renderer = Renderer::Instance();
		if (renderer.Process() == 0) {
			return;
		}

		uint32_t loaded = g_madnightEngine.m_fileLoader.LoadedFiles();
		uint32_t total = g_madnightEngine.m_fileLoader.TotalFiles();
		constexpr uint32_t BAR_WIDTH = 20;
		uint32_t filled = total > 0 ? loaded * BAR_WIDTH / total : 0;

		char bar[BAR_WIDTH + 1];
		for (uint32_t i = 0; i < BAR_WIDTH; i++) {
			bar[i] = i < filled ? '#' : '-';
		}
		bar[BAR_WIDTH] = '\0';

		renderer.Clear({.r = 0, .g = 0, .b = 0});
		renderer.SystemFont()->chainprintf(renderer.GPU(), {88, 100}, COLOUR_WHITE, "MADNIGHT TECH DEMO");
		renderer.SystemFont()->chainprintf(renderer.GPU(), {56, 120}, COLOUR_WHITE, "[%s] %u/%u", bar, loaded, total);
	}
};

class TechDemo final : public MadnightEngineGame {
  public:
	psyqo::Coroutine<> InitialLoad(void) override;
};

TechDemo g_techDemo;
MadnightEngineGame& g_madnightEngineGame = g_techDemo;

psyqo::Coroutine<> TechDemo::InitialLoad(void) {
	// built here, not as a global: Camera's constructor runs GTE maths and reads
	// g_madnightEngine.m_trig, neither of which is ready during static initialisation
	static HallScene hallScene;
	static TechDemoLoadingScene loadingScene;
	printf("TECHDEMO loading\n");
	// one manifest pulls in every texture, mesh and animation the scene uses
	eastl::vector<LoadQueue> files = {{.name = "DEMO.SCN", .type = LoadFileType::SCENE}};
	co_await g_madnightEngine.HardLoadingScreen(eastl::move(files), &loadingScene, &hallScene);
	printf("TECHDEMO loaded\n");
}

int main() { return g_madnightEngine.run(); }
