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
