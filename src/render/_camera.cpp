#include "_camera.hh"
#include "../controller/controller.hh"
#include "../madnight.hh"
#include "psyqo/soft-math.hh"

psyqo::Vec3 CameraManager::m_pos = {0, 0, 0};
CAMERA_ANGLE CameraManager::m_angle = {0, 0, 0};
psyqo::Matrix33 CameraManager::m_rotation_matrix = {0};

void CameraManager::init(void) {
  set_position(static_cast<psyqo::FixedPoint<12>>(0), -0.02_fp, -0.075_fp);
  set_rotation_matrix();
}

void CameraManager::set_position(psyqo::FixedPoint<12> x, psyqo::FixedPoint<12> y, psyqo::FixedPoint<12> z) {
  m_pos.x = x;
  m_pos.y = y;
  m_pos.z = z;
}

void CameraManager::process(uint32_t delta_time) {
  psyqo::Trig trig = g_madnightEngine.m_trig;

  // move camera forward
  if (g_madnightEngine.m_input.isButtonPressed(psyqo::AdvancedPad::Pad::Pad1a, psyqo::AdvancedPad::Button::Triangle)) {
    psyqo::FixedPoint<12> forward = m_movement_speed * delta_time;
    m_pos.x += trig.sin(m_angle.y) * forward;
    m_pos.z += trig.cos(m_angle.y) * forward;
  }

  // move camera back
  else if (g_madnightEngine.m_input.isButtonPressed(psyqo::AdvancedPad::Pad::Pad1a,
                                                    psyqo::AdvancedPad::Button::Cross)) {
    psyqo::FixedPoint<12> backwards = -m_movement_speed * delta_time;
    m_pos.x += trig.sin(m_angle.y) * backwards;
    m_pos.z += trig.cos(m_angle.y) * backwards;
  }

  // move camera left
  if (g_madnightEngine.m_input.isButtonPressed(psyqo::AdvancedPad::Pad::Pad1a, psyqo::AdvancedPad::Button::Square)) {
    psyqo::FixedPoint<12> left = m_movement_speed * delta_time;
    m_pos.x += -trig.cos(m_angle.y) * left;
    m_pos.z += trig.sin(m_angle.y) * left;
  }

  // move camera right
  else if (g_madnightEngine.m_input.isButtonPressed(psyqo::AdvancedPad::Pad::Pad1a,
                                                    psyqo::AdvancedPad::Button::Circle)) {
    psyqo::FixedPoint<12> right = -m_movement_speed * delta_time;
    m_pos.x += -trig.cos(m_angle.y) * right;
    m_pos.z += trig.sin(m_angle.y) * right;
  }

  // move camera up
  if (g_madnightEngine.m_input.isButtonPressed(psyqo::AdvancedPad::Pad::Pad1a, psyqo::AdvancedPad::Button::L1)) {
    psyqo::FixedPoint<12> up = -m_movement_speed * delta_time;
    m_pos.y += up;
  }

  // move camera down
  else if (g_madnightEngine.m_input.isButtonPressed(psyqo::AdvancedPad::Pad::Pad1a, psyqo::AdvancedPad::Button::R1)) {
    psyqo::FixedPoint<12> down = m_movement_speed * delta_time;
    m_pos.y += down;
  }

  // make sure we've actually got an analog pad connected
  if (g_madnightEngine.m_input.getPadType(psyqo::AdvancedPad::Pad::Pad1a) != psyqo::AdvancedPad::PadType::AnalogPad)
    return;

  // analog stick movement for fps cam (remember 0x80 is centre, so -128 to 127)
  int leftStickX = ControllerHelper::GetNormalizedAnalogStickInput(psyqo::AdvancedPad::Pad::Pad1a,
                                                                   ControllerHelper::AnalogStickIndex::LeftStickX);
  int leftStickY = ControllerHelper::GetNormalizedAnalogStickInput(psyqo::AdvancedPad::Pad::Pad1a,
                                                                   ControllerHelper::AnalogStickIndex::LeftStickY);

  int rightStickX = ControllerHelper::GetNormalizedAnalogStickInput(psyqo::AdvancedPad::Pad::Pad1a,
                                                                    ControllerHelper::AnalogStickIndex::RightStickX);
  int rightStickY = ControllerHelper::GetNormalizedAnalogStickInput(psyqo::AdvancedPad::Pad::Pad1a,
                                                                    ControllerHelper::AnalogStickIndex::RightStickY);

  // use left stick y to move forward/backwards
  if (leftStickY < -m_stickDeadzone || leftStickY > m_stickDeadzone) {
    psyqo::FixedPoint<12> forward = m_movement_speed * -leftStickY >> 5 * delta_time;
    m_pos.x += trig.sin(m_angle.y) * forward;
    m_pos.z += trig.cos(m_angle.y) * forward;
  }

  // use left stick x to strafe left/right
  if (leftStickX < -m_stickDeadzone || leftStickX > m_stickDeadzone) {
    psyqo::FixedPoint<12> left = m_movement_speed * -leftStickX >> 5 * delta_time;
    m_pos.x += -trig.cos(m_angle.y) * left;
    m_pos.z += trig.sin(m_angle.y) * left;
  }

  // use right stick y to look up/down
  if (rightStickY < -m_stickDeadzone || rightStickY > m_stickDeadzone) {
    m_angle.x += (-rightStickY >> 5) * delta_time * m_rotation_speed;
    set_rotation_matrix();
  }

  // use right stick x to look left/right
  if (rightStickX < -m_stickDeadzone || rightStickX > m_stickDeadzone) {
    m_angle.y -= (-rightStickX >> 5) * delta_time * m_rotation_speed;
    set_rotation_matrix();
  }
}

void CameraManager::set_rotation_matrix(void) {
  auto rm_x = psyqo::SoftMath::generateRotationMatrix33(m_angle.x, psyqo::SoftMath::Axis::X, g_madnightEngine.m_trig);
  auto rm_y = psyqo::SoftMath::generateRotationMatrix33(m_angle.y, psyqo::SoftMath::Axis::Y, g_madnightEngine.m_trig);
  auto rm_z = psyqo::SoftMath::generateRotationMatrix33(m_angle.z, psyqo::SoftMath::Axis::Z, g_madnightEngine.m_trig);

  // xyz multiplication order
  psyqo::SoftMath::multiplyMatrix33(rm_y, rm_x, &rm_y);
  psyqo::SoftMath::multiplyMatrix33(rm_y, rm_z, &rm_y);

  // update rotation matrix
  m_rotation_matrix = rm_y;
}

psyqo::Vec3 CameraManager::GetForwardVector(void) {
  return {-m_rotation_matrix.vs[0].z, m_rotation_matrix.vs[1].z, m_rotation_matrix.vs[2].z};
}

psyqo::Vec3 &CameraManager::get_pos(void) { return m_pos; }
CAMERA_ANGLE *CameraManager::get_angle(void) { return &m_angle; }
psyqo::Matrix33 &CameraManager::get_rotation_matrix(void) { return m_rotation_matrix; };
