/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once
#include "../helpers/load_queue.hh"
#include <EASTL/string_view.h>
#include <EASTL/vector.h>
#include <psyqo/coroutine.hh>

class SceneLoader final {
  public:
	psyqo::Coroutine<> LoadScene(const eastl::string_view& sceneFile, eastl::vector<LoadQueue>& queue);

  private:
};
