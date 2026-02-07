#ifndef _SKELETON_H
#define _SKELETON_H

#include "../../animation/animation.hh"
#include "../../quaternion.hh"
#include "psyqo/matrix.hh"
#include "psyqo/vector.hh"

static constexpr uint8_t MAX_BONES = 50;

struct SkeletonBoneMatrix {
  psyqo::Matrix33 rotationMatrix = {{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}};
  psyqo::Vec3 translation = {0, 0, 0};
};

struct SkeletonBone {
  int8_t id;
  int8_t parent;                           // -1 = root
  psyqo::Vec3 localPos = {0, 0, 0};        // relative to parent
  Quaternion localRotation = {0, 0, 0, 0}; // relative to parent
  Quaternion initialLocalRotation = {0, 0, 0, 0};
  SkeletonBoneMatrix localMatrix;     // computed itself. to generate this see `GameObject::GenerateRotationMatrix`
  SkeletonBoneMatrix worldMatrix;     // computed from parent. to generate this see `GameObject::GenerateRotationMatrix`
  SkeletonBoneMatrix bindPose;        // initial pose when the skeleton is loaded in
  SkeletonBoneMatrix bindPoseInverse; // inverse bind pose matrix
  bool isDirty = true;                // set by the animation to determine if we need to regenrate the above
  bool hasDoneBindPose = false;
  psyqo::Vec3 startPos = {0,0,0};
  psyqo::Vec3 endPos = {0,0,0};
};

struct Skeleton {
  uint8_t numBones;
  SkeletonBone bones[MAX_BONES];
  Animation *animation;
  uint16_t animationCurrentFrame = 0;
};

class SkeletonController {
public:
  static void UpdateSkeletonBoneMatrices(Skeleton *skeleton);
  static void MarkBonesClean(Skeleton *skeleton);
  static void SetAnimation(Skeleton *skeleton, Animation *animation);
  static void PlayAnimation(Skeleton *skeleton, uint32_t deltaTime);
};

#endif
