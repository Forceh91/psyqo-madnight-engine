#ifndef _CAMERA_H
#define _CAMERA_H

#include "psyqo/fixed-point.hh"
#include "psyqo/matrix.hh"
#include "psyqo/trigonometry.hh"
#include "psyqo/vector.hh"

using namespace psyqo::fixed_point_literals;
using namespace psyqo::trig_literals;

enum CameraPerspective { FIRST, THIRD };

enum CameraMode {
  FIXED /* camera is fixed, user has no control */,
  FOLLOW /* camera will follow a set pos at a set distance, user can rotate around said pos */,
  FREE /* user has full control over the camera*/
};

struct CameraAngle {
  psyqo::Angle x;
  psyqo::Angle y;
  psyqo::Angle z;
};

struct CameraTracking {
  psyqo::Vec3 *pos;
  psyqo::Angle angle;
  psyqo::FixedPoint<> distance;
};

class Camera final {
public:
  Camera() {
    m_initialPos = m_pos;
    m_initialAngle = m_angle;
    SetRotationMatrix();
  }

  Camera(psyqo::Vec3 pos) {
    m_pos = pos;
    m_initialPos = m_pos;
    m_initialAngle = m_angle;
    SetRotationMatrix();
  }

  Camera(psyqo::FixedPoint<12> x, psyqo::FixedPoint<12> y, psyqo::FixedPoint<12> z) {
    m_pos = {x, y, z};
    m_initialPos = m_pos;
    m_initialAngle = m_angle;
    SetRotationMatrix();
  }

  const psyqo::Vec3 &const_pos(void) { return m_pos; };
  const psyqo::Vec3 *pos(void) { return &m_pos; }
  const CameraAngle *angle(void) { return &m_angle; }
  const psyqo::Vec3 forwardVector(void) {
    return {-m_rotationMatrix.vs[0].z, m_rotationMatrix.vs[1].z, m_rotationMatrix.vs[2].z};
  }
  const psyqo::Matrix33 &rotationMatrix(void) { return m_rotationMatrix; }

  void Process(uint32_t deltaTime);
  void SetPosition(psyqo::FixedPoint<> x, psyqo::FixedPoint<> y, psyqo::FixedPoint<> z);
  void SetPosition(psyqo::Vec3 pos);
  void SetAngle(psyqo::Angle x, psyqo::Angle y, psyqo::Angle z);
  void SetAngle(CameraAngle angle);
  void SetPerspective(CameraPerspective perspective);
  void SetMode(CameraMode mode);

  // pointer to a vec3 (e.g. player position) that you want to track
  // distance is in metres. try to keep this value small. 128px = 1m
  // will only affect camera when set to FOLLOW mode
  void SetFollow(psyqo::Vec3 *pos, psyqo::Angle angle, psyqo::FixedPoint<> distance);
  void ClearFollow(void);

private:
  psyqo::Vec3 m_pos = {0, 0, 0};
  psyqo::Vec3 m_initialPos = {0, 0, 0};
  CameraTracking m_tracking = {nullptr, 0, 0};
  CameraAngle m_angle = {0, 0, 0};
  CameraAngle m_initialAngle = {0, 0, 0};
  psyqo::Matrix33 m_rotationMatrix = {0, 0, 0};
  CameraPerspective m_cameraPerspective = CameraPerspective::FIRST;
  CameraMode m_cameraMode = CameraMode::FREE;

  void SetRotationMatrix(void);
  psyqo::FixedPoint<> m_movementSpeed = 0.001_fp;
  psyqo::Angle m_rotationSpeed = 0.005_pi;
  uint8_t m_stickDeadzone = 16;
};

#endif
