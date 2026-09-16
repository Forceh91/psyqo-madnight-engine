/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once
#include "psyqo/coroutine.hh"

class MadnightEngineGame {
  public:
	virtual psyqo::Coroutine<> InitialLoad(void) = 0;
};

extern MadnightEngineGame& g_madnightEngineGame;
