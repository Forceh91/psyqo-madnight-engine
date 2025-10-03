#include "skeleton.hh"
#include "../../madnight.hh"
#include "../../math/gte-math.hh"
#include "psyqo/matrix.hh"
#include "psyqo/soft-math.hh"

void SkeletonController::GenerateBoneMatrix(const SkeletonBone &bone, psyqo::Matrix33 *rotationMatrix) {
  auto roll = psyqo::SoftMath::generateRotationMatrix33(bone.localRotation.x, psyqo::SoftMath::Axis::X,
                                                        g_madnightEngine.m_trig);
  auto pitch = psyqo::SoftMath::generateRotationMatrix33(bone.localRotation.y, psyqo::SoftMath::Axis::Y,
                                                         g_madnightEngine.m_trig);
  auto yaw = psyqo::SoftMath::generateRotationMatrix33(bone.localRotation.z, psyqo::SoftMath::Axis::Z,
                                                       g_madnightEngine.m_trig);

  // create complete x/y/z rotation. this is done ROLL then YAW then PITCH
  psyqo::Matrix33 tempMatrix = {0};
  GTEMath::MultiplyMatrix33(yaw, pitch, &tempMatrix);
  GTEMath::MultiplyMatrix33(tempMatrix, roll, rotationMatrix);
}

void SkeletonController::UpdateSkeleton(Skeleton *skeleton) {
  if (skeleton == nullptr)
    return;

  // for each bone in the skeleton we need to update its wordlMatrix. child bones are done from the parent
  for (int32_t i = 0; i < skeleton->numBones; i++) {
    SkeletonBone &bone = skeleton->bones[i];

    // generate its local matrix from rot (euler) + pos?
    SkeletonController::GenerateBoneMatrix(bone, &bone.localMatrix.rotationMatrix);
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
      psyqo::SoftMath::matrixVecMul3(parentBone.worldMatrix.rotationMatrix, bone.localMatrix.translation,
                                     &boneWorldTranslation);
      bone.worldMatrix = SkeletonBoneMatrix{boneWorldRot, parentBone.worldMatrix.translation + boneWorldTranslation};
    }
  }
}
