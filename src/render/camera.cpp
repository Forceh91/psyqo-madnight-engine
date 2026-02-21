#include "camera.hh"
#include "../madnight.hh"
#include "../math/vector.hh"
#include "../math/matrix.hh"
#include "EASTL/algorithm.h"
#include "psyqo/fixed-point.hh"
#include "psyqo/matrix.hh"
#include "psyqo/soft-math.hh"
#include "psyqo/trigonometry.hh"
#include "psyqo/vector.hh"

void Camera::Process(uint32_t deltaTime) {
  // TODO: should first/third person camera perspective affect things?
  psyqo::Trig trig = g_madnightEngine.m_trig;

  switch (m_cameraMode) {
  case CameraMode::FIXED:
    break;

  case CameraMode::FOLLOW:
    // update the position to be an orbit around
    SetPosition(CalculateOrbitPosition());
    // and update the rotation matrix to be a lookat matrix
    LookAt(m_tracking.pos);

    break;

  case CameraMode::FREE_LOOK:
    break;
  }
}

void Camera::SetRotationMatrix(void) {
  auto rotationMatrixX =
      psyqo::SoftMath::generateRotationMatrix33(m_angle.x, psyqo::SoftMath::Axis::X, g_madnightEngine.m_trig);
  auto rotationMatrixY =
      psyqo::SoftMath::generateRotationMatrix33(m_angle.y, psyqo::SoftMath::Axis::Y, g_madnightEngine.m_trig);
  auto rotationMatrixZ =
      psyqo::SoftMath::generateRotationMatrix33(m_angle.z, psyqo::SoftMath::Axis::Z, g_madnightEngine.m_trig);

  // xyz multiplication order
  psyqo::SoftMath::multiplyMatrix33(rotationMatrixY, rotationMatrixX, &rotationMatrixY);
  psyqo::SoftMath::multiplyMatrix33(rotationMatrixY, rotationMatrixZ, &rotationMatrixY);

  // update calculated rotation matrix
  m_rotationMatrix = rotationMatrixY;
}

void Camera::SetPosition(psyqo::FixedPoint<12> x, psyqo::FixedPoint<12> y, psyqo::FixedPoint<12> z) {
  m_pos = {x, y, z};
}

void Camera::SetPosition(psyqo::Vec3 pos) { m_pos = pos; }

void Camera::SetAngle(psyqo::Angle x, psyqo::Angle y, psyqo::Angle z) {
  SetAngle(CameraAngle{x, y, z});
  SetRotationMatrix();
}

void Camera::SetAngle(CameraAngle angle) {
  m_angle = angle;
  SetRotationMatrix();
}

void Camera::SetFixed(void) { SetFixed(m_pos, m_angle); }

void Camera::SetFixed(psyqo::Vec3 pos) { SetFixed(pos, m_angle); }

void Camera::SetFixed(psyqo::Vec3 pos, CameraAngle angle) {
  m_prevCameraMode = m_cameraMode;
  m_cameraMode = CameraMode::FIXED;

  SetPosition(pos);
  SetAngle(angle);
}

void Camera::ClearFixed(void) { m_cameraMode = m_prevCameraMode; }

void Camera::SetFollow(psyqo::Vec3 *pos, psyqo::FixedPoint<> distance) { SetFollow(pos, {0, 0}, distance); }

void Camera::SetFollow(psyqo::Vec3 *pos, psyqo::Vec2 offsetPos, psyqo::FixedPoint<> distance) {
  m_tracking = {pos, offsetPos, distance};
  m_cameraMode = CameraMode::FOLLOW;
}

void Camera::ClearFollow(void) {
  m_tracking = {nullptr, 0, 0};
  m_cameraMode = CameraMode::FIXED;
}

void Camera::SetFreeLook(void) { SetFreeLook(m_pos, m_angle); }

void Camera::SetFreeLook(psyqo::Vec3 pos) { SetFreeLook(pos, m_angle); }

void Camera::SetFreeLook(psyqo::Vec3 pos, CameraAngle initialAngle) {
  m_cameraMode = CameraMode::FREE_LOOK;
  SetPosition(pos);
  SetAngle(initialAngle);
}

void Camera::SetFreeLookMaxAngles(CameraMaxAngle maxAngles) { m_maxFreeLookAngles = maxAngles; }

void Camera::ClearFreeLook(void) { m_cameraMode = CameraMode::FIXED; }

// TODO: watch for stuff getting in the way, force the distance to be lower if needed
psyqo::Vec3 Camera::CalculateOrbitPosition(void) {
  psyqo::Trig trig = g_madnightEngine.m_trig;
  auto const currentPos = *m_tracking.pos;

  psyqo::Vec3 forward = {trig.cos(m_orbitAngle.x) * trig.sin(m_orbitAngle.y), trig.sin(m_orbitAngle.x),
                         trig.cos(m_orbitAngle.x) * trig.cos(m_orbitAngle.y)};
  if (IsVector3Zero(forward))
    return currentPos;

  psyqo::SoftMath::normalizeVec3(&forward);

  auto worldUp = psyqo::Vec3::UP();
  auto forwardUpCross = psyqo::SoftMath::crossProductVec3(forward, worldUp);
  if (IsVector3Zero(forwardUpCross))
    return currentPos;

  auto right = forwardUpCross;
  psyqo::SoftMath::normalizeVec3(&right);

  auto rightForwardCross = psyqo::SoftMath::crossProductVec3(right, forward);
  auto up = rightForwardCross;
  if (IsVector3Zero(up))
    return currentPos;

  psyqo::SoftMath::normalizeVec3(&up);

  if (m_orbitAngle.z < 0 || m_orbitAngle.z > 0) {
    auto cosRight = trig.cos(m_orbitAngle.z);
    auto sinRight = trig.sin(m_orbitAngle.z);

    auto orbitRight = right * cosRight + up * sinRight;
    up = up * cosRight - right * sinRight;
    right = orbitRight;
  }

  return *m_tracking.pos - (forward * m_tracking.distance) + (right * m_tracking.offsetPos.x) +
         (up * m_tracking.offsetPos.y);
}

void Camera::LookAt(const psyqo::Vec3 *target) {
  // calculate forward axis direction
  auto forwardVector = *target - m_pos;
  auto forwardVectorNormal = forwardVector;
  if (IsVector3Zero(forwardVector))
    return;

  psyqo::SoftMath::normalizeVec3(&forwardVectorNormal);

  // calculate right axis direction
  auto up = psyqo::Vec3::UP();
  auto crossProduct = psyqo::SoftMath::crossProductVec3(forwardVectorNormal, up);
  if (IsVector3Zero(crossProduct))
    return;

  auto rightVectorNormal = crossProduct;
  if (IsVector3Zero(rightVectorNormal))
    return;

  psyqo::SoftMath::normalizeVec3(&rightVectorNormal);

  // calculate up axis direction
  auto rightForwardCrossProduct = psyqo::SoftMath::crossProductVec3(rightVectorNormal, forwardVectorNormal);

  if (IsVector3Zero(rightForwardCrossProduct))
    return;

  auto upVectorNormal = rightForwardCrossProduct;
  if (IsVector3Zero(upVectorNormal))
    return;
  
  psyqo::SoftMath::normalizeVec3(&upVectorNormal);

  m_rotationMatrix = {{{rightVectorNormal.x, rightVectorNormal.y, rightVectorNormal.z},
                       {upVectorNormal.x, upVectorNormal.y, upVectorNormal.z},
                       {forwardVectorNormal.x, forwardVectorNormal.y, forwardVectorNormal.z}}};
}

void Camera::UpdateOrbitAngles(psyqo::Angle xDeltaAmount, psyqo::Angle yDeltaAmount) {
  UpdateOrbitAngles(xDeltaAmount, yDeltaAmount, 1);
}

void Camera::UpdateOrbitAngles(psyqo::Angle xAmount, psyqo::Angle yAmount, uint32_t deltaTime) {
  if (m_cameraMode != CameraMode::FOLLOW)
    return;

  m_orbitAngle.x = eastl::clamp(m_orbitAngle.x - xAmount * deltaTime, -0.21_pi, 0.21_pi);
  m_orbitAngle.y = eastl::clamp(m_orbitAngle.y + yAmount * deltaTime, -1.0_pi, 1.0_pi);

  // allow a full 360 view when going left->right or vice versa
  if (m_orbitAngle.y == 1.0_pi || m_orbitAngle.y == -1.0_pi)
    m_orbitAngle.y = -m_orbitAngle.y;
}

void Camera::UpdateAngles(psyqo::Angle xDeltaAmount, psyqo::Angle yDeltaAmount, psyqo::Angle zDeltaAmount) {
  UpdateAngles(xDeltaAmount, yDeltaAmount, zDeltaAmount, 1);
}

void Camera::UpdateAngles(psyqo::Angle xAmount, psyqo::Angle yAmount, psyqo::Angle zAmount, uint32_t deltaTime) {
  m_angle.x = eastl::clamp(m_angle.x - xAmount * deltaTime, -m_maxFreeLookAngles.maxX, m_maxFreeLookAngles.maxX);
  m_angle.y = eastl::clamp(m_angle.y + yAmount * deltaTime, -m_maxFreeLookAngles.maxY, m_maxFreeLookAngles.maxY);
  m_angle.z = eastl::clamp(m_angle.z + zAmount * deltaTime, -m_maxFreeLookAngles.maxZ, m_maxFreeLookAngles.maxZ);
  SetRotationMatrix();
}

psyqo::Matrix33 Camera::inverseRotationMatrix(void) {
  return InverseMatrix33(m_rotationMatrix);
}
