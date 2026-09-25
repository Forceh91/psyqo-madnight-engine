/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "../pool/pool.hh"
#include <EASTL/functional.h>
#include <EASTL/string_view.h>
#include <psyqo/coroutine.hh>
#include <psyqo/primitives/common.hh>

static constexpr uint16_t texturePageWidth = 64;
static constexpr uint16_t texturePageHeight = 256;
static constexpr uint8_t texturePageColumns = 16;
static constexpr uint8_t MAX_TEXTURES = 32; // this will need tweaking later
// 0 is a valid VRAM coordinate, so the 'use whatever the file says' sentinel has to be
// a value VRAM can never hold. VRAM is 1024x512.
static constexpr uint16_t TIM_POSITION_FROM_FILE = 0xFFFF;

struct TimFile {
	uint64_t nameHash = 0;
	int16_t id = INVALID_POOL_ID;
	bool isLoaded = false;						  // is this slot actually holding a texture?
	uint16_t x = 0, y = 0, width = 0, height = 0; // pos in vram + width/height
	psyqo::Prim::TPageAttr::ColorMode colourMode =
		psyqo::Prim::TPageAttr::ColorMode::Tex4Bits; // bits per pixel (4, 8, 16)

	bool hasClut = false;					// does it need/have a clut?
	uint16_t clutX = 0, clutY = 0;			// clut pos in vram
	uint16_t clutWidth = 0, clutHeight = 0; // clut width and height (always 1)
};

class TextureManager final {
	psyqo::Vertex GetTPageIndex(uint16_t x, uint16_t y);
	Pool<TimFile, MAX_TEXTURES> m_pool;

	TimFile* IsTextureLoaded(const eastl::string_view& name);
	TimFile* IsTextureLoaded(uint64_t nameHash);

  public:
	psyqo::Coroutine<> LoadTIM(const eastl::string_view& textureName, uint16_t x, uint16_t y, uint16_t clutX,
							   uint16_t clutY, TimFile** timOut);
	psyqo::PrimPieces::TPageAttr GetTPageAttr(const TimFile* tim);
	psyqo::PrimPieces::TPageAttr GetTPageAttr(const TimFile& tim);
	psyqo::Rect GetTPageUVForTim(const TimFile& tim);
	psyqo::Rect GetTPageUVForTim(const TimFile* tim);

	void GetTextureFromName(const eastl::string_view& textureName, TimFile** timOut);

	// dump all textures in memory and start fresh
	// this is used when switching to a loading screen for instance.
	// this is a dangerous function as it wont check if anything is used
	// this wont remove anything from vram
	void Dump(void);
};
