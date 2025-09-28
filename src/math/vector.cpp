#include "vector.hh"
#include "psyqo/matrix.hh"
#include "psyqo/soft-math.hh"
#include "psyqo/vector.hh"

psyqo::FixedPoint<> DotProduct(const psyqo::Vec3 &a, const psyqo::Vec3 &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

psyqo::Matrix33 LookAt(psyqo::Vec3 origin, psyqo::Vec3 target) {
  // calculate forward axis direction
  auto forwardVector = target - origin;
  auto forwardVectorNormal = forwardVector;
  psyqo::SoftMath::normalizeVec3(&forwardVectorNormal);

  // calculate right axis direction
  auto up = psyqo::Vec3::UP();
  auto crossProduct = psyqo::SoftMath::crossProductVec3(forwardVectorNormal, up);
  auto rightVectorNormal = crossProduct;
  psyqo::SoftMath::normalizeVec3(&rightVectorNormal);

  // calculate up axis direction
  auto forwardRightCrossProduct = psyqo::SoftMath::crossProductVec3(rightVectorNormal, forwardVectorNormal);
  auto upVectorNormal = forwardRightCrossProduct;
  psyqo::SoftMath::normalizeVec3(&upVectorNormal);

  return {.vs = {{rightVectorNormal.x, rightVectorNormal.y, rightVectorNormal.z},
                 {upVectorNormal.x, upVectorNormal.y, upVectorNormal.z},
                 {forwardVectorNormal.x, forwardVectorNormal.y, forwardVectorNormal.z}}};
}
