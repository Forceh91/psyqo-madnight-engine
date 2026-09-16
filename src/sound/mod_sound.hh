/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "../helpers/archive.hh"
#include <EASTL/fixed_string.h>

extern "C" {
#include "modplayer/modplayer.h"
}

struct ModSoundFile {
	eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN> name = "";
	uint32_t size = 0;
	bool isLoaded = false;
};
