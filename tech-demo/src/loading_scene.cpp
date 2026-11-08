/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "loading_scene.hh"
#include "file_loader.hh"
#include "madnight.hh"
#include "render/renderer.hh"

#include <EASTL/array.h>

// the hall's fog for the background, and the embers' start and end colours for the bar
static constexpr psyqo::Color BACKGROUND = {.r = 46, .g = 48, .b = 50};
static constexpr psyqo::Color TRACK = {.r = 24, .g = 24, .b = 26};
static constexpr psyqo::Color BAR_START = {.r = 120, .g = 30, .b = 16};
static constexpr psyqo::Color BAR_END = {.r = 255, .g = 170, .b = 60};
static constexpr psyqo::Color TEXT = {.r = 220, .g = 210, .b = 190};

static constexpr psyqo::Rect BAR = {.pos = {40, 128}, .size = {240, 12}};
static constexpr eastl::array<char, 4> SPINNER = {'|', '/', '-', '\\'};
static constexpr uint32_t SPINNER_STEP_US = 100000;

void TechDemoLoadingScene::start(StartReason reason) {
	Renderer::Instance().StartScene();
	m_startTime = Renderer::Instance().GPU().now();
}

void TechDemoLoadingScene::frame(void) {
	auto& renderer = Renderer::Instance();
	if (renderer.Process() == 0) {
		return;
	}

	auto& gpu = renderer.GPU();
	renderer.Clear(BACKGROUND);

	auto* font = renderer.SystemFont();
	font->chainprint(gpu, "MADNIGHT ENGINE", {100, 92}, TEXT);
	font->chainprint(gpu, "tech demo", {124, 106}, BAR_END);

	// bar width from the share of files loaded so far
	uint32_t loaded = g_madnightEngine.m_fileLoader.LoadedFiles();
	uint32_t total = g_madnightEngine.m_fileLoader.TotalFiles();
	int16_t width = total > 0 ? int16_t(BAR.size.w * loaded / total) : 0;

	auto parity = gpu.getParity();
	auto& track = m_track[parity];
	track.primitive.setColor(TRACK);
	track.primitive.position = {int16_t(BAR.pos.x - 2), int16_t(BAR.pos.y - 2)};
	track.primitive.size = {int16_t(BAR.size.w + 4), int16_t(BAR.size.h + 4)};
	gpu.chain(track);

	if (width > 0) {
		// the gradient runs across the whole track, so a part-filled bar shows the cool end only
		uint8_t t = uint8_t(255 * width / BAR.size.w);
		auto mix = [t](uint8_t a, uint8_t b) { return uint8_t(a + (b - a) * t / 255); };
		psyqo::Color tip = {
			.r = mix(BAR_START.r, BAR_END.r), .g = mix(BAR_START.g, BAR_END.g), .b = mix(BAR_START.b, BAR_END.b)};

		auto& bar = m_bar[parity];
		bar.primitive.setPointA({BAR.pos.x, BAR.pos.y});
		bar.primitive.setPointB({int16_t(BAR.pos.x + width), BAR.pos.y});
		bar.primitive.setPointC({BAR.pos.x, int16_t(BAR.pos.y + BAR.size.h)});
		bar.primitive.setPointD({int16_t(BAR.pos.x + width), int16_t(BAR.pos.y + BAR.size.h)});
		bar.primitive.setColorA(BAR_START);
		bar.primitive.setColorB(tip);
		bar.primitive.setColorC(BAR_START);
		bar.primitive.setColorD(tip);
		gpu.chain(bar);

		// sparks rise off the filled part of the bar and cool as they go, like the hall's embers.
		// each one loops on its own period, so no state is kept between frames
		uint32_t elapsedMs = (gpu.now() - m_startTime) / 1000;
		for (unsigned i = 0; i < EMBER_COUNT; i++) {
			uint32_t period = 900 + i * 137;
			uint32_t age = (elapsedMs + i * 311) % period;
			int16_t rise = int16_t(age * 40 / period);
			int16_t x = int16_t(BAR.pos.x + (i * 53 + age / 90) % width);
			uint8_t cool = uint8_t(255 * age / period);
			auto fade = [cool](uint8_t hot, uint8_t cold) { return uint8_t(hot + (cold - hot) * cool / 255); };

			auto& ember = m_embers[parity][i];
			ember.primitive.setColor({.r = fade(BAR_END.r, BACKGROUND.r),
									  .g = fade(BAR_END.g, BACKGROUND.g),
									  .b = fade(BAR_END.b, BACKGROUND.b)});
			ember.primitive.position = {x, int16_t(BAR.pos.y - 4 - rise)};
			ember.primitive.size = {2, 2};
			gpu.chain(ember);
		}
	}

	char spinner = SPINNER[((gpu.now() - m_startTime) / SPINNER_STEP_US) % SPINNER.size()];
	font->chainprintf(gpu, {BAR.pos.x, int16_t(BAR.pos.y + 20)}, TEXT, "Loading %c  %u/%u", spinner, loaded, total);
}
