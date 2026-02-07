#include "skeleton.hh"
#include "../../math/gte-math.hh"
#include "../../math/matrix.hh"
#include "psyqo/fixed-point.hh"
#include "psyqo/matrix.hh"
#include "psyqo/vector.hh"
#include "psyqo/xprintf.h"

void SkeletonController::UpdateSkeletonBoneMatrices(Skeleton *skeleton) {
  if (skeleton == nullptr)
    return;

  for (int32_t i = 0; i < skeleton->numBones; i++) {
    // for each bone in the skeleton, we need to do a recursive world matrix update
    // but we need to know its local matrix first
    auto *bone = &skeleton->bones[i];
    if (bone == nullptr)
      continue;

    // not got a parent and its not dirty, continue
    if (bone->parent == -1 && !bone->isDirty)
      continue;

    // parent isnt dirty, and this bone isn't dirty, continue
    if (bone->parent != -1 && !skeleton->bones[bone->parent].isDirty && !bone->isDirty)
      continue;

    // normalize quat rotation
    auto localRot = bone->localRotation;
    localRot.Normalize();

    // generate its rotation matrix
    auto localRotMatrix = localRot.ToRotationMatrix();
    auto localTrans = bone->localPos;

    // store local translation matrix
    bone->localMatrix = {localRotMatrix, localTrans};

    // next we need to compute its world matrix.
    // if we have no parent then just use the local matrix for this
    if (bone->parent == -1)
      bone->worldMatrix = bone->localMatrix;
    else {
      auto *parent = &skeleton->bones[bone->parent];
      if (parent == nullptr)
        continue;

      // if the parent is dirty, then this one is
      if (parent->isDirty)
        bone->isDirty = true;

      if (!bone->isDirty)
        continue;

      // rotation
      psyqo::Matrix33 worldRot;
      GTEMath::MultiplyMatrix33(parent->worldMatrix.rotationMatrix, bone->localMatrix.rotationMatrix, &worldRot);

      // translation
      psyqo::Vec3 worldTrans;
      GTEMath::MultiplyMatrixVec3(parent->worldMatrix.rotationMatrix, bone->localMatrix.translation, &worldTrans);

      // final world matrix (parent + rotated local)
      bone->worldMatrix = {worldRot, parent->worldMatrix.translation + worldTrans};
    }

    // if we dont have a bindpose + bindpose inverse stored. then do that
    if (!bone->hasDoneBindPose) {
      // bind pose
      bone->bindPose = bone->worldMatrix;
      bone->initialLocalRotation = bone->localRotation;

      // inverse of bind pose
      auto inverseRotation = TransposeMatrix33(bone->bindPose.rotationMatrix);
      psyqo::Vec3 inverseTranslation;
      GTEMath::MultiplyMatrixVec3(inverseRotation, -bone->bindPose.translation, &inverseTranslation);
      bone->bindPoseInverse = {inverseRotation, inverseTranslation};

      // mark it as having done this so we dont lose t-pose data
      bone->hasDoneBindPose = true;
    }
  }

  for (int j = 0; j < skeleton->numBones; j++) {
    auto *bone = &skeleton->bones[j];
    
    // find first child of this bone
    int childIndex = -1;
    for (int k = 0; k < skeleton->numBones; k++) {
      if (skeleton->bones[k].parent == j) {
        childIndex = k;
        break;
      }
    }

    bone->startPos = bone->worldMatrix.translation;

    if (childIndex != -1) {
      bone->endPos = skeleton->bones[childIndex].worldMatrix.translation;
    } else {
      // fallback stub for leaf bones - point along bone's local Z axis
      psyqo::Vec3 stubDir = {0, -0.0001_fp, 0}; // adjust length as needed
      psyqo::Vec3 worldDir;
      GTEMath::MultiplyMatrixVec3(bone->worldMatrix.rotationMatrix, stubDir, &worldDir);
      
      bone->endPos = bone->startPos + worldDir;
    }
  }
}

void SkeletonController::MarkBonesClean(Skeleton *skeleton) {
  for (int32_t i = 0; i < skeleton->numBones; i++) {
    skeleton->bones[i].isDirty = false;
  }
}

// right now this will just overwrite the animation. no blending
void SkeletonController::SetAnimation(Skeleton *skeleton, Animation *animation) {
  if (skeleton == nullptr || animation == nullptr)
    return;

  // set the animation and reset its frame
  skeleton->animation = animation;
  skeleton->animationCurrentFrame = 0;
}

void SkeletonController::PlayAnimation(Skeleton *skeleton, uint32_t deltaTime) {
  if (skeleton == nullptr)
    return;

  // if theres no animation then stop
  if (skeleton->animation == nullptr)
    return;

  const auto &animation = skeleton->animation;
  if (skeleton->animationCurrentFrame >= animation->length) {
    // restart if looping, otherwise set to the last frame
    if (animation->flags & 1)
      skeleton->animationCurrentFrame = 0;
    else
      skeleton->animationCurrentFrame = animation->length - 1;
  }

  // for each track
  for (int32_t i = 0; i < animation->numTracks; i++) {
    const auto &track = animation->tracks[i];

    // placeholder prev/next key
    const Key *prev = &track.keys[0];
    const Key *next = &track.keys[0];

    // find the two keyframes around the current frame
    auto &currentFrame = skeleton->animationCurrentFrame;
    for (int32_t j = 0; j < track.numKeys - 1; j++) {
      if (currentFrame >= track.keys[j].frame && currentFrame < track.keys[j + 1].frame) {
        prev = &track.keys[j];
        next = &track.keys[j + 1];
        break;
      }
    }

    // if we never broke out of loop (we’re past last key)
    if (currentFrame >= track.keys[track.numKeys - 1].frame) {
      prev = &track.keys[track.numKeys - 1];
      next = prev; // stay fixed on last key, no interpolation
    } 

    // need to slerp
    auto frameDiff = next->frame - prev->frame;
    auto slerpFactor = frameDiff > 0 ? ((skeleton->animationCurrentFrame - prev->frame) / frameDiff * 1.0_fp) : 0;

    auto &bone = skeleton->bones[track.jointId];
    if (next->keyType == KeyType::ROTATION) {
      bone.localRotation = -bone.initialLocalRotation * Slerp(prev->rotation, next->rotation, slerpFactor);
    }
    // TODO: translation

    bone.isDirty = true;
  }

  // increase what animation frame we're on
  skeleton->animationCurrentFrame += deltaTime;
}
