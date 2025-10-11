#ifndef _ANIMATION_HH
#define _ANIMATION_HH

#include "../quaternion.hh"
#include "EASTL/fixed_string.h"
#include "psyqo/vector.hh"

static constexpr uint8_t MAX_ANIMATIONS = 5;
static constexpr uint8_t MAX_TRACKS = 5;
static constexpr uint8_t MAX_MARKERS = 5;
static constexpr uint8_t MAX_KEYS = 5;

// what the frame should do. rotate or translate the bone/joint
struct Key {
  union {
    Quaternion rotation;
    psyqo::Vec3 translation;
  };

  uint32_t frame;
  uint8_t keyType; // 0 = quat, 1 = rotation
};

// a track is basically "for this bone. do this"
struct Track {
  uint8_t type;
  uint8_t jointId;
  uint16_t numKeys;
  Key keys[MAX_KEYS];
};

// useful for marking a certain frame, can be used for say "play sound at this marker"
struct Marker {
  eastl::fixed_string<char, 32> name;
  uint32_t frame;
};

// the actual animation
struct Animation {
  eastl::fixed_string<char, 32> name;
  uint32_t flags;              // bitfields. looped = 1
  uint16_t length;             // how many frames?
  uint16_t numTracks;          // how many tracks it contains
  Track tracks[MAX_TRACKS];    // the data in the tracks
  uint16_t numMarkers;         // numbers of markers
  Marker markers[MAX_MARKERS]; // the markers
};

struct AnimationBin {
  uint32_t numAnimations;
  Animation animations[MAX_ANIMATIONS];
};

#endif
