#include "camera.hh"
#include "../madnight.hh"
#include "../math/vector.hh"
#include "psyqo/advancedpad.hh"
#include "psyqo/soft-math.hh"
#include "psyqo/trigonometry.hh"
#include "psyqo/vector.hh"

void Camera::Process(uint32_t deltaTime) {
  // TODO: should first/third person camera perspective affect things?
  psyqo::Trig trig = g_madnightEngine.m_trig;

  switch (m_cameraMode) {
  case CameraMode::FIXED:
    // used for looking at a set point, not much processing to do really
    break;

  case CameraMode::FOLLOW:
    // update position, allow camera rotation in an orbit style
    // L2 will orbit the camera left
    if (g_madnightEngine.m_input.isButtonPressed(psyqo::AdvancedPad::Pad::Pad1a, psyqo::AdvancedPad::L2)) {
      m_orbitAngle.y -= (128 >> 5) * deltaTime * m_rotationSpeed;
    }

    // R2 will orbit the camera right
    if (g_madnightEngine.m_input.isButtonPressed(psyqo::AdvancedPad::Pad::Pad1a, psyqo::AdvancedPad::R2)) {
      m_orbitAngle.y -= (-128 >> 5) * deltaTime * m_rotationSpeed;
    }

    // update the position to be an orbit around
    SetPosition(CalculateOrbitPosition());
    // and update the rotation matrix to be a lookat matrix
    m_rotationMatrix = LookAt(m_pos, *m_tracking.pos);

    // watch for rotation inputs
    break;

  case CameraMode::FREE:
    // ignore follow position, allow camera rotation
    break;
  }

  // once everything is processed, update rotation matrix
  // SetRotationMatrix();
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

void Camera::SetAngle(psyqo::Angle x, psyqo::Angle y, psyqo::Angle z) { m_angle = {x, y, z}; }

void Camera::SetAngle(CameraAngle angle) { m_angle = angle; }

void Camera::SetPerspective(CameraPerspective perspective) {
  // TODO: reset position when this happens?
  m_cameraPerspective = perspective;
}

void Camera::SetMode(CameraMode mode) { m_cameraMode = mode; }

void Camera::SetFollow(psyqo::Vec3 *pos, psyqo::FixedPoint<> distance) { SetFollow(pos, {0, 0}, distance); }

void Camera::SetFollow(psyqo::Vec3 *pos, psyqo::Vec2 offsetPos, psyqo::FixedPoint<> distance) {
  m_tracking = {pos, offsetPos, distance};
  m_cameraMode = CameraMode::FOLLOW;
}

void Camera::ClearFollow(void) { m_tracking = {nullptr, 0, 0}; }

// TODO: watch for stuff getting in the way, force the distance to be lower if needed
psyqo::Vec3 Camera::CalculateOrbitPosition(void) {
  psyqo::Trig trig = g_madnightEngine.m_trig;
  psyqo::Vec3 forward = {trig.cos(m_orbitAngle.x) * trig.sin(m_orbitAngle.y), trig.sin(m_orbitAngle.x),
                         trig.cos(m_orbitAngle.x) * trig.cos(m_orbitAngle.y)};

  auto worldUp = psyqo::Vec3::UP();
  auto forwardUpCross = psyqo::SoftMath::crossProductVec3(forward, worldUp);
  auto right = forwardUpCross;
  psyqo::SoftMath::normalizeVec3(&right);

  auto rightForwardCross = psyqo::SoftMath::crossProductVec3(right, forward);
  auto up = rightForwardCross;
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