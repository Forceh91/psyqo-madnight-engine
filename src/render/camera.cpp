#include "camera.hh"
#include "../madnight.hh"
#include "psyqo/soft-math.hh"
#include "psyqo/trigonometry.hh"

void Camera::Process(uint32_t deltaTime) {
  // TODO: should first/third person camera perspective affect things?
  psyqo::Trig trig = g_madnightEngine.m_trig;
  psyqo::Vec3 offsetPosition = {0, 0, 0};

  switch (m_cameraMode) {
  case CameraMode::FIXED:
    // used for looking at a set point, not much processing to do really
    break;

  case CameraMode::FOLLOW:
    // update position, allow camera rotation
    // TODO: watch for stuff getting in the way, force the distance to be lower if needed
    offsetPosition = *m_tracking.pos;
    offsetPosition.x += trig.sin(m_angle.y) * -m_tracking.distance;
    offsetPosition.z += trig.cos(m_angle.y) * -m_tracking.distance;

    SetPosition(offsetPosition);
    SetAngle(m_angle.x, m_tracking.angle, m_angle.z);

    // watch for rotation inputs
    break;

  case CameraMode::FREE:
    // ignore follow position, allow camera rotation
    break;
  }

  // once everything is processed, update rotation matrix
  SetRotationMatrix();
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

// NOTE: will force the camera into follow mode
void Camera::SetFollow(psyqo::Vec3 *pos, psyqo::Angle angle, psyqo::FixedPoint<> distance) {
  m_tracking = {pos, angle, distance};
  m_cameraMode = CameraMode::FOLLOW;
}
void Camera::ClearFollow(void) { m_tracking = {nullptr, 0, 0}; }
