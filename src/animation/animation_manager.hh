#pragma once

#include "EASTL/fixed_string.h"
#include "animation.hh"
#include "psyqo/coroutine.hh"

class AnimationManager final {
	static AnimationBin m_loadedAnimBin;

  public:
	static psyqo::Coroutine<> LoadAnimation(const eastl::string_view& animationsFile);
	static Animation* GetAnimationFromName(const eastl::fixed_string<char, MAX_ANIMATION_NAME_LENGTH>& animationName);
};
