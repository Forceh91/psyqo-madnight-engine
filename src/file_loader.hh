/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "helpers/load_queue.hh"
#include "scenes/scene_loader.hh"
#include <psyqo/coroutine.hh>

enum LOAD_STATE : uint8_t { UNKNOWN, LOADING, COMPLETE };

class FileLoader final {
  public:
	// will load the provided queue, `clearPools` will dump all existing game objects, textures,
	// meshes, colbins, and sfx, from memory. it will not check they are in-use before it does this.
	// this is a FIFO worklist and the queue will be loaded backwards.
	psyqo::Coroutine<> LoadFiles(eastl::vector<LoadQueue>&& files, bool clearPools = true);
	// how many files in total *will* be loaded. this number will increase when a scene file is reached
	constexpr uint16_t TotalFiles(void) { return m_totalFiles; }
	// how many files in total *have* been loaded.
	constexpr uint16_t LoadedFiles(void) { return m_loadedFiles; }

	constexpr LOAD_STATE LoadState(void) {
		if (!m_totalFiles) {
			return UNKNOWN;
		}
		if (m_totalFiles > 0 && m_loadedFiles != m_totalFiles) {
			return LOADING;
		}

		return COMPLETE;
	}

  private:
	// the file(s) that will be loaded
	eastl::vector<LoadQueue> m_queue;
	// how many files in total will be loaded
	uint16_t m_totalFiles = 0;
	// how many have been loaded
	uint16_t m_loadedFiles = 0;

	SceneLoader m_sceneLoader;
};
