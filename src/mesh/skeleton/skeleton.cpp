#include "skeleton.hh"
#include "../../math/gte-math.hh"
#include "../../math/matrix.hh"
#include "psyqo/fixed-point.hh"
#include "psyqo/gte-registers.hh"
#include "psyqo/matrix.hh"
#include "psyqo/vector.hh"
#include "psyqo/xprintf.h"

void SkeletonController::UpdateSkeletonBoneMatrices(Skeleton *skeleton) {
  if (skeleton == nullptr)
    return;

  // TODO: add dirty check back in
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
    // bone->worldMatrix = bone->localMatrix;

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

      // translatioon
      psyqo::Vec3 worldTrans;
      GTEMath::MultiplyMatrixVec3(parent->worldMatrix.rotationMatrix, bone->localMatrix.translation, &worldTrans);

      // final world matrix (parent + rotated local)
      bone->worldMatrix = {worldRot, parent->worldMatrix.translation + worldTrans};
    }

    if (i == 9 || i == 10) {
      printf("bone %d (parent=%d)=%d,%d,%d\n               %d,%d,%d\n               %d,%d,%d trans=%d,%d,%d\n", i,
             bone->parent, bone->worldMatrix.rotationMatrix.vs[0].x, bone->worldMatrix.rotationMatrix.vs[0].y,
             bone->worldMatrix.rotationMatrix.vs[0].z, bone->worldMatrix.rotationMatrix.vs[1].x,
             bone->worldMatrix.rotationMatrix.vs[1].y, bone->worldMatrix.rotationMatrix.vs[1].z,
             bone->worldMatrix.rotationMatrix.vs[2].x, bone->worldMatrix.rotationMatrix.vs[2].y,
             bone->worldMatrix.rotationMatrix.vs[2].z, bone->worldMatrix.translation.x, bone->worldMatrix.translation.y,
             bone->worldMatrix.translation.z);
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
}

// void SkeletonController::UpdateSkeletonBoneMatrices(Skeleton *skeleton) {
//   if (skeleton == nullptr)
//     return;

//   // for each bone in the skeleton we need to update its wordlMatrix. child bones are done from the parent
//   for (int32_t i = 0; i < skeleton->numBones; i++) {
//     auto &bone = skeleton->bones[i];

//     // if this bone and the parent bone aren't dirty, dont need to do this
//     if ((bone.parent == -1 && !bone.isDirty) ||
//         (bone.parent != -1 && !bone.isDirty && !skeleton->bones[bone.parent].isDirty))
//       continue;

//     // generate its local matrix from rot (quats) + pos?
//     auto normalizedRotation = bone.localRotation;
//     normalizedRotation.Normalize();
//     bone.localMatrix = {normalizedRotation.ToRotationMatrix(), bone.localPos};

//     if (i == 10) {
//       printf("bone %d after=%d,%d,%d\n               %d,%d,%d\n               %d,%d,%d\n", i,
//              bone.localMatrix.rotationMatrix.vs[0].x, bone.localMatrix.rotationMatrix.vs[0].y,
//              bone.localMatrix.rotationMatrix.vs[0].z, bone.localMatrix.rotationMatrix.vs[1].x,
//              bone.localMatrix.rotationMatrix.vs[1].y, bone.localMatrix.rotationMatrix.vs[1].z,
//              bone.localMatrix.rotationMatrix.vs[2].x, bone.localMatrix.rotationMatrix.vs[2].y,
//              bone.localMatrix.rotationMatrix.vs[2].z);
//     }

//     // world matrix
//     // is the local one if its the root
//     if (bone.parent == -1)
//       bone.worldMatrix = bone.localMatrix;
//     else {
//       auto &parentBone = skeleton->bones[bone.parent];
//       if (parentBone.isDirty)
//         bone.isDirty = true;

//       // since the parent has changed, mark this as dirty
//       if (!bone.isDirty)
//         continue;

//       psyqo::Matrix33 boneWorldRot;
//       GTEMath::MultiplyMatrix33(parentBone.worldMatrix.rotationMatrix, bone.localMatrix.rotationMatrix,
//       &boneWorldRot);

//       psyqo::Vec3 boneWorldTranslation;
//       GTEMath::MultiplyMatrixVec3(parentBone.worldMatrix.rotationMatrix, bone.localMatrix.translation,
//                                   &boneWorldTranslation);

//       bone.worldMatrix = SkeletonBoneMatrix{
//           boneWorldRot, boneWorldTranslation}; // parentBone.worldMatrix.translation + boneWorldTranslation};
//     }

//     // if we've not done the initial pose (t-pose) then create the bindpose too
//     if (!bone.hasDoneBindPose) {
//       bone.bindPose = bone.worldMatrix;

//       auto inverseRotationMatrix = TransposeMatrix33(bone.worldMatrix.rotationMatrix);
//       psyqo::Vec3 inverseTranslation;
//       GTEMath::MultiplyMatrixVec3(inverseRotationMatrix, -bone.bindPose.translation, &inverseTranslation);

//       bone.initialLocalRotation = bone.localRotation;
//       bone.bindPoseInverse = {inverseRotationMatrix, inverseTranslation};
//       bone.hasDoneBindPose = true;
//     }
//     /*
//     printf("bone=%i, bindPose.rot=%f,%f,%f/%f,%f,%f/%f,%f,%f, bindPose.trans=%f,%f,%f. inverseBindPose.rot=%f,%f,%f.
//     "
//            "inverseBindPose.trans=%f,%f,%f. world rot=%f,%f,%f/%f,%f,%f/%f,%f,%f, world trans=%f,%f,%f\n",
//            i, bone.bindPose.rotationMatrix.vs[0].x, bone.bindPose.rotationMatrix.vs[0].y,
//            bone.bindPose.rotationMatrix.vs[0].z, bone.bindPose.rotationMatrix.vs[1].x,
//            bone.bindPose.rotationMatrix.vs[1].y, bone.bindPose.rotationMatrix.vs[1].z,
//            bone.bindPose.rotationMatrix.vs[2].x, bone.bindPose.rotationMatrix.vs[2].y,
//            bone.bindPose.rotationMatrix.vs[2].z, bone.bindPose.translation.x, bone.bindPose.translation.y,
//            bone.bindPose.translation.z, bone.bindPoseInverse.rotationMatrix.vs[0].x,
//            bone.bindPoseInverse.rotationMatrix.vs[0].y, bone.bindPoseInverse.rotationMatrix.vs[0].z,
//            bone.bindPoseInverse.rotationMatrix.vs[1].x, bone.bindPoseInverse.rotationMatrix.vs[1].y,
//            bone.bindPoseInverse.rotationMatrix.vs[1].z, bone.bindPoseInverse.rotationMatrix.vs[2].x,
//            bone.bindPoseInverse.rotationMatrix.vs[2].y, bone.bindPoseInverse.rotationMatrix.vs[2].z,
//            bone.bindPoseInverse.translation.x, bone.bindPoseInverse.translation.y,
//            bone.bindPoseInverse.translation.z, bone.worldMatrix.rotationMatrix.vs[0].x,
//            bone.worldMatrix.rotationMatrix.vs[0].y, bone.worldMatrix.rotationMatrix.vs[0].z,
//            bone.worldMatrix.rotationMatrix.vs[1].x, bone.worldMatrix.rotationMatrix.vs[1].y,
//            bone.worldMatrix.rotationMatrix.vs[1].z, bone.worldMatrix.rotationMatrix.vs[2].x,
//            bone.worldMatrix.rotationMatrix.vs[2].y, bone.worldMatrix.rotationMatrix.vs[2].z,
//            bone.worldMatrix.translation.x, bone.worldMatrix.translation.y, bone.worldMatrix.translation.z);
//     */
//   }
// }

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

    // need to sleep
    auto frameDiff = next->frame - prev->frame;
    auto slerpFactor = frameDiff > 0 ? ((skeleton->animationCurrentFrame - prev->frame) / frameDiff) * 1.0_fp : 0;

    auto &bone = skeleton->bones[track.jointId];
    if (next->keyType == KeyType::ROTATION) {
      if (track.jointId == 9 || track.jointId == 10) {
        printf("bone %d before=%d,%d,%d\n               %d,%d,%d\n               %d,%d,%d trans=%d,%d,%d\n",
               track.jointId, bone.localMatrix.rotationMatrix.vs[0].x, bone.localMatrix.rotationMatrix.vs[0].y,
               bone.localMatrix.rotationMatrix.vs[0].z, bone.localMatrix.rotationMatrix.vs[1].x,
               bone.localMatrix.rotationMatrix.vs[1].y, bone.localMatrix.rotationMatrix.vs[1].z,
               bone.localMatrix.rotationMatrix.vs[2].x, bone.localMatrix.rotationMatrix.vs[2].y,
               bone.localMatrix.rotationMatrix.vs[2].z, bone.localPos.x, bone.localPos.y, bone.localPos.z);
        // bone.localRotation = Slerp(prev->rotation, next->rotation, slerpFactor);
        // if (track.jointId == 9)
        // bone.localRotation.x = bone.initialLocalRotation.x + psyqo::GTE::Short(0.1_fp);
      }

      if (track.jointId == 5) {
        // bone.localRotation.x = psyqo::GTE::Short(0.5_fp);
        // bone.localRotation = Slerp(prev->rotation, next->rotation, slerpFactor);
      }
      bone.localRotation = Slerp(prev->rotation, next->rotation, slerpFactor);
    }

    // TODO: translation
    bone.isDirty = true;
  }

  // increase what animation frame we're on
  skeleton->animationCurrentFrame += deltaTime;
}
