#ifndef _SKELETON_H
#define _SKELETON_H

#include "psyqo/matrix.hh"
#include "psyqo/trigonometry.hh"
#include "psyqo/vector.hh"

static constexpr uint8_t maxBones = 20;

struct SkeletonBoneMatrix {
  psyqo::Matrix33 rotationMatrix;
  psyqo::Vec3 translation;
};

struct SkeletonBoneRotation {
  psyqo::Angle x;
  psyqo::Angle y;
  psyqo::Angle z;
};

struct SkeletonBone {
  uint8_t parent;                     // -1 = root
  psyqo::Vec3 localPos;               // relative to parent
  SkeletonBoneRotation localRotation; // relative to parent. TODO: make euler
  SkeletonBoneMatrix worldMatrix;     // computed from parent
};

struct Skeleton {
  uint8_t numBones;
  SkeletonBone bones[maxBones];
};

#endif
