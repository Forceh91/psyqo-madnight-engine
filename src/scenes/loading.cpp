#include "loading.hh"
#include "../core/debug/perf_monitor.hh"
#include "../file_loader.hh"
#include "../madnight.hh"
#include "../render/renderer.hh"

void LoadingScene::start(StartReason reason) { Renderer::Instance().StartScene(); }

void LoadingScene::frame() {
	auto& instance = Renderer::Instance();
	uint32_t deltaTime = instance.Process();
	if (deltaTime == 0) {
		return;
	}

	auto loaded = psyqo::FixedPoint<>(int32_t(g_madnightEngine.m_fileLoader.LoadedFiles()), int32_t(0));
	auto total = psyqo::FixedPoint<>(int32_t(g_madnightEngine.m_fileLoader.TotalFiles()), int32_t(0));
	uint8_t percent = loaded > 0 ? (loaded / total * 100).integer() : 0;

	Renderer::Instance().Clear();
	instance.SystemFont()->chainprintf(instance.GPU(), {10, 200}, COLOUR_WHITE, "Loading... (%d%%)", percent);
	g_madnightEngine.m_perfMonitor.Render(deltaTime);
}
