#pragma once

#include "EASTL/fixed_string.h"
#include "animation.hh"
#include "psyqo/coroutine.hh"

class AnimationManager final {
	AnimationBin m_loadedAnimBin = {};

  public:
	psyqo::Coroutine<> LoadAnimation(const eastl::string_view& animationsFile);
	Animation* GetAnimationFromName(const eastl::fixed_string<char, MAX_ANIMATION_NAME_LENGTH>& animationName);
};
