#ifndef _CAMERA_H
#define _CAMERA_H

#include "psyqo/fixed-point.hh"
#include "psyqo/matrix.hh"
#include "psyqo/trigonometry.hh"
#include "psyqo/vector.hh"

using namespace psyqo::fixed_point_literals;
using namespace psyqo::trig_literals;

enum CameraMode {
  FIXED /* camera is fixed, `LookAt` can still be used. good for cutscenes etc. responds to `SetPosition`, `SetAngle`,
           and `UpdateAngles` for convenience but recommended to take control away from the user when in this mode */
  ,
  FOLLOW /* camera will follow a set pos at a set distance, user can orbit around said pos (see: `UpdateOrbitAngles`),
            automatically calls `LookAt` on the followed position. essentially a third person camera */
  ,
  FREE_LOOK /* user has full control over the camera rotation (see: `UpdateAngles`, `SetAngle`), `LookAt` will also
               work, but will override user input. essentially a first person cam. up to you to update its position
               (see: `SetPosition`) when you move your character if you want it to be FPS style */
};

struct CameraAngle {
  psyqo::Angle x;
  psyqo::Angle y;
  psyqo::Angle z;
};

struct CameraMaxAngle {
  psyqo::Angle maxX;
  psyqo::Angle maxY;
  psyqo::Angle maxZ;
};

struct CameraTracking {
  psyqo::Vec3 *pos;
  psyqo::Vec2 offsetPos;
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

  ~Camera(){};

  const psyqo::Vec3 &pos(void) const { return m_pos; };
  const psyqo::Vec3 *posPtr(void) { return &m_pos; }
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

  void SetFixed(void);
  void SetFixed(psyqo::Vec3 pos);
  void SetFixed(psyqo::Vec3 pos, CameraAngle angle);
  // returns the camera back to the previously set mode
  void ClearFixed(void);

  // pointer to a vec3 (e.g. player position) that you want to track
  // distance is in metres. try to keep this value small. 128px = 1m
  // NOTE: will force the camera into follow mode, and will only take affect when in that mode
  void SetFollow(psyqo::Vec3 *pos, psyqo::FixedPoint<> distance);
  void SetFollow(psyqo::Vec3 *pos, psyqo::Vec2 offsetPos, psyqo::FixedPoint<> distance);
  // stops following, turns the camera into a fixed camera
  void ClearFollow(void);

  void SetFreeLook(void);
  void SetFreeLook(psyqo::Vec3 pos);
  void SetFreeLook(psyqo::Vec3 pos, CameraAngle initialAngle);
  // defaults to 0.5_pi (90deg/-90deg) if not set.
  void SetFreeLookMaxAngles(CameraMaxAngle maxAngles);
  // turns the camera into a fixed camera
  void ClearFreeLook(void);

  void LookAt(const psyqo::Vec3 *target);

  // deltaTime in terms of frames
  void UpdateOrbitAngles(psyqo::Angle xDeltaAmount, psyqo::Angle yDeltaAmount);
  void UpdateOrbitAngles(psyqo::Angle xAmount, psyqo::Angle yAmount, uint32_t deltaTime);

  // deltatime in terms of frames
  void UpdateAngles(psyqo::Angle xDeltaAmount, psyqo::Angle yDeltaAmount, psyqo::Angle zDeltaAmount);
  void UpdateAngles(psyqo::Angle xDeltaAmount, psyqo::Angle yDeltaAmount, psyqo::Angle zDeltaAmount,
                    uint32_t deltaTime);

private:
  psyqo::Vec3 m_pos = {0, 0, 0};
  psyqo::Vec3 m_initialPos = {0, 0, 0};
  CameraTracking m_tracking = {nullptr, 0, 0};
  CameraAngle m_angle = {0, 0, 0};
  CameraAngle m_orbitAngle = {0, 0, 0}; // only used when camera is in FOLLOW mode
  CameraAngle m_initialAngle = {0, 0, 0};
  psyqo::Matrix33 m_rotationMatrix = {0, 0, 0};
  CameraMode m_cameraMode = CameraMode::FIXED;
  CameraMode m_prevCameraMode = m_cameraMode;
  psyqo::FixedPoint<> m_movementSpeed = 0.001_fp;
  psyqo::Angle m_rotationSpeed = 0.005_pi;
  CameraMaxAngle m_maxFreeLookAngles = {0.5_pi, 0.5_pi, 0.5_pi};

  void SetRotationMatrix(void);
  psyqo::Vec3 CalculateOrbitPosition(void);
};

#endif
