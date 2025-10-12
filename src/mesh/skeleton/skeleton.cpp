#include "skeleton.hh"
#include "../../math/gte-math.hh"
#include "psyqo/fixed-point.hh"
#include "psyqo/matrix.hh"

void SkeletonController::UpdateSkeletonBoneMatrices(Skeleton *skeleton) {
  if (skeleton == nullptr)
    return;

  // for each bone in the skeleton we need to update its wordlMatrix. child bones are done from the parent
  for (int32_t i = 0; i < skeleton->numBones; i++) {
    auto &bone = skeleton->bones[i];

    // if this bone and the parent bone aren't dirty, dont need to do this
    if ((bone.parent == -1 && !bone.isDirty) ||
        (bone.parent != -1 && !bone.isDirty && !skeleton->bones[bone.parent].isDirty))
      continue;

    // generate its local matrix from rot (quats) + pos?
    auto normalizedRotation = bone.localRotation;
    normalizedRotation.Normalize();
    bone.localMatrix.rotationMatrix = normalizedRotation.ToRotationMatrix();
    bone.localMatrix.translation = bone.localPos;

    // world matrix
    // is the local one if its the root
    if (bone.parent == -1)
      bone.worldMatrix = bone.localMatrix;
    else {
      auto &parentBone = skeleton->bones[bone.parent];
      if (!parentBone.isDirty)
        continue;

      // since the parent has changed, mark this as dirty
      bone.isDirty = true;

      psyqo::Matrix33 boneWorldRot;
      GTEMath::MultiplyMatrix33(parentBone.worldMatrix.rotationMatrix, bone.localMatrix.rotationMatrix, &boneWorldRot);

      psyqo::Vec3 boneWorldTranslation;
      GTEMath::MultiplyMatrixVec3(parentBone.worldMatrix.rotationMatrix, bone.localMatrix.translation,
                                  &boneWorldTranslation);

      bone.worldMatrix = SkeletonBoneMatrix{boneWorldRot, parentBone.worldMatrix.translation + boneWorldTranslation};
    }

    // if we've not done the initial pose (t-pose) then create the bindpose too
    if (!bone.hasDoneBindPose) {
      bone.bindPose = bone.worldMatrix;
      bone.hasDoneBindPose = true;
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

    // need to sleep
    auto frameDiff = next->frame - prev->frame;
    auto slerpFactor = frameDiff > 0 ? ((skeleton->animationCurrentFrame - prev->frame) / frameDiff) * 1.0_fp : 0;

    auto &bone = skeleton->bones[track.jointId];
    if (next->keyType == KeyType::ROTATION) {
      auto rot = bone.localRotation;
      bone.localRotation = Slerp(prev->rotation, next->rotation, slerpFactor);
    }

    // TODO: translation
    bone.isDirty = true;
  }

  // increase what animation frame we're on
  skeleton->animationCurrentFrame += deltaTime;
}
