#include "skeleton.hh"
#include "../../math/gte-math.hh"
#include "psyqo/matrix.hh"

void SkeletonController::UpdateSkeleton(Skeleton *skeleton) {
  if (skeleton == nullptr)
    return;

  // for each bone in the skeleton we need to update its wordlMatrix. child bones are done from the parent
  for (int32_t i = 0; i < skeleton->numBones; i++) {
    SkeletonBone &bone = skeleton->bones[i];

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
      psyqo::Matrix33 boneWorldRot;
      GTEMath::MultiplyMatrix33(parentBone.worldMatrix.rotationMatrix, bone.localMatrix.rotationMatrix, &boneWorldRot);

      psyqo::Vec3 boneWorldTranslation;
      GTEMath::MultiplyMatrixVec3(parentBone.worldMatrix.rotationMatrix, bone.localMatrix.translation,
                                  &boneWorldTranslation);
      bone.worldMatrix = SkeletonBoneMatrix{boneWorldRot, parentBone.worldMatrix.translation + boneWorldTranslation};
    }
  }
}
