#ifndef _ANIMATION_MANAGER_H
#define _ANIMATION_MANAGER_H

#include "EASTL/fixed_string.h"
#include "animation.hh"
#include "psyqo/coroutine.hh"

class AnimationManager final {
  static AnimationBin m_loadedAnimBin;

public:
  static psyqo::Coroutine<> LoadAnimationFromCDRom(const char *animationsFile);
  static Animation *GetAnimationFromName(const eastl::fixed_string<char, MAX_ANIMATION_NAME_LENGTH> &animationName);
};

#endif
