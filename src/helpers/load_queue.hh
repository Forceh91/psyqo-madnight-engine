#pragma once

#include "archive.hh"
#include <EASTL/fixed_string.h>

enum LoadFileType { OBJECT, TEXTURE, MOD_FILE, ANIMATION, COLBIN, VAG, SCENE = 255 };

struct LoadQueue {
	eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN> name = "";
	LoadFileType type = SCENE;
	// used for textures (type == TEXTURE)
	union {
		struct {
			uint16_t x = 0, y = 0, clutX = 0, clutY = 0;
		};
	};
};
