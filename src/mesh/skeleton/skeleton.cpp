#include "skeleton.hh"
#include "../../math/gte-math.hh"
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
    auto existingMatrix = bone.worldMatrix;
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
