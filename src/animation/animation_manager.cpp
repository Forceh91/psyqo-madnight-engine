#include "animation_manager.hh"
#include "../helpers/cdrom.hh"
#include "EASTL/fixed_string.h"
#include "animation.hh"
#include "psyqo/xprintf.h"

AnimationBin AnimationManager::m_loadedAnimBin = {0, {}};

psyqo::Coroutine<> AnimationManager::LoadAnimationFromCDRom(const char *animationsFile) {
  auto buffer = co_await CDRomHelper::LoadFile(animationsFile);

  void *data = buffer.data();
  size_t size = buffer.size();
  if (data == nullptr || size == 0) {
    buffer.clear();
    printf("ANIMATIONS: Failed to load animations file or it has no file size.\n");
    co_return;
  }

  // basic struct setup
  __builtin_memset(&m_loadedAnimBin, 0, sizeof(AnimationBin));

  // pointer math type
  uint8_t *ptr = (uint8_t *)data;

  // animbin header
  eastl::fixed_string<char, 7> magic(reinterpret_cast<char *>(ptr), 7);
  ptr += 7;

  // verify the magic value
  if (magic.compare("ANIMBIN") != 0) {
    printf("ANIMATIONS: Header is invalid. aborting.\n");
    buffer.clear();
    co_return;
  }

  // version + anim count
  uint8_t version = 0;
  __builtin_memcpy(&version, ptr++, 1);                       // 1 byte
  __builtin_memcpy(&m_loadedAnimBin.numAnimations, ptr++, 1); // 1 byte

  // free the buffer
  buffer.clear();
  printf("ANIMATIONS: Successfully loaded animations file of %d bytes into memory.\n", size);
}
