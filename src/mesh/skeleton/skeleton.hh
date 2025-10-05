#ifndef _SKELETON_H
#define _SKELETON_H

#include "../../quaternion.hh"
#include "psyqo/matrix.hh"
#include "psyqo/vector.hh"

static constexpr uint8_t maxBones = 20;

struct SkeletonBoneMatrix {
  psyqo::Matrix33 rotationMatrix = {{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}};
  psyqo::Vec3 translation = {0, 0, 0};
};

struct SkeletonBone {
  int8_t parent;                           // -1 = root
  psyqo::Vec3 localPos = {0, 0, 0};        // relative to parent
  Quaternion localRotation = {0, 0, 0, 0}; // relative to parent
  SkeletonBoneMatrix localMatrix;          // computed itself. to generate this see `GameObject::GenerateRotationMatrix`
  SkeletonBoneMatrix worldMatrix; // computed from parent. to generate this see `GameObject::GenerateRotationMatrix`
};

struct Skeleton {
  uint8_t numBones;
  SkeletonBone bones[maxBones];
};

class SkeletonController {
public:
  static void UpdateSkeleton(Skeleton *skeleton);
};

#endif
