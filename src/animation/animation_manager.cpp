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

  // for the number of animations...
  for (int32_t i = 0; i < m_loadedAnimBin.numAnimations; i++) {
    auto *anim = &m_loadedAnimBin.animations[i];
    // load the name
    anim->name.assign(reinterpret_cast<char *>(ptr), MAX_ANIMATION_NAME_LENGTH);
    ptr += MAX_ANIMATION_NAME_LENGTH;

    // load any flags it has
    __builtin_memcpy(&anim->flags, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    // length of animation
    __builtin_memcpy(&anim->length, ptr, sizeof(uint16_t));
    ptr += sizeof(uint16_t);

    // number of tracks
    __builtin_memcpy(&anim->numTracks, ptr, sizeof(uint16_t));
    ptr += sizeof(uint16_t);

    // numer of frame markers
    __builtin_memcpy(&anim->numMarkers, ptr, sizeof(uint16_t));
    ptr += sizeof(uint16_t);

    // load the tracks (should be one per bone)
    for (int32_t j = 0; j < anim->numTracks; j++) {
      auto *track = &anim->tracks[j];

      // track type (rotation or translation)
      __builtin_memcpy(&track->type, ptr++, sizeof(uint8_t));

      // which bone
      __builtin_memcpy(&track->jointId, ptr++, sizeof(uint8_t));

      // keyframes
      __builtin_memcpy(&track->numKeys, ptr, sizeof(uint16_t));
      ptr += sizeof(uint16_t);

      // for each frame (key)
      for (int32_t k = 0; k < track->numKeys; k++) {
        auto *key = &track->keys[k];

        // frame number
        __builtin_memcpy(&key->frame, ptr, sizeof(uint16_t));
        ptr += sizeof(uint16_t);

        // key type
        __builtin_memcpy(&key->keyType, ptr++, sizeof(uint8_t));

        // keytype data
        if (key->keyType == KeyType::ROTATION) {
          // rotation data
          __builtin_memcpy(&key->rotation.w.value, ptr, sizeof(int16_t)); // 2 bytes
          ptr += sizeof(int16_t);

          __builtin_memcpy(&key->rotation.x.value, ptr, sizeof(int16_t)); // 2 bytes
          ptr += sizeof(int16_t);

          __builtin_memcpy(&key->rotation.y.value, ptr, sizeof(int16_t)); // 2 bytes
          ptr += sizeof(int16_t);

          __builtin_memcpy(&key->rotation.z.value, ptr, sizeof(int16_t)); // 2 bytes
          ptr += sizeof(int16_t);
        } else if (key->keyType == KeyType::TRANSLATION) {
          // translation data
          __builtin_memcpy(&key->translation.x.value, ptr, sizeof(int32_t)); // 4 bytes
          ptr += sizeof(int32_t);

          __builtin_memcpy(&key->translation.y.value, ptr, sizeof(int32_t)); // 4 bytes
          ptr += sizeof(int32_t);

          __builtin_memcpy(&key->translation.z.value, ptr, sizeof(int32_t)); // 4 bytes
          ptr += sizeof(int32_t);
        }
      }
    }

    // if they have markers
    if (anim->numMarkers) {
      for (int32_t i = 0; i < anim->numMarkers; i++) {
        auto *marker = &anim->markers[i];
        // load the marker name
        marker->name.assign(reinterpret_cast<char *>(ptr), MAX_ANIMATION_NAME_LENGTH);
        ptr += MAX_ANIMATION_NAME_LENGTH;

        // load the frame
        __builtin_memcpy(&marker->frame, ptr, sizeof(uint16_t));
        ptr += sizeof(uint16_t);
      }
    }
  }

  // free the buffer
  buffer.clear();
  printf("ANIMATIONS: Successfully loaded animations file of %d bytes into memory.\n", size);
}

Animation * AnimationManager::GetAnimationFromName(const eastl::fixed_string<char, MAX_ANIMATION_NAME_LENGTH> &animationName) {
  for (int32_t i = 0; i < m_loadedAnimBin.numAnimations; i++) {
    auto *anim = &m_loadedAnimBin.animations[i];
    if (anim == nullptr)
      continue;

    printf("name=%s. to find=%s. comparison=%d\n", anim->name.c_str(), animationName.c_str(),
           anim->name.compare(animationName));

    // if (anim->name.compare(animationName) == 0)
    return anim;
  }

  return nullptr;
}
