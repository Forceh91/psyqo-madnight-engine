#include "camera.hh"
#include "psyqo/soft-math.hh"
#include "../hello3d.hh"

psyqo::Vec3 CameraManager::m_pos;
CAMERA_ANGLE CameraManager::m_angle = {0, 0, 0};
psyqo::Matrix33 CameraManager::m_rotation_matrix = {0};

void CameraManager::init(void)
{
    set_position(static_cast<psyqo::FixedPoint<12>>(0), static_cast<psyqo::FixedPoint<12>>(0), static_cast<psyqo::FixedPoint<12>>(-0.01));
    set_rotation_matrix();
}

void CameraManager::set_position(psyqo::FixedPoint<12> x, psyqo::FixedPoint<12> y, psyqo::FixedPoint<12> z)
{
    m_pos.x = x;
    m_pos.y = y;
    m_pos.z = z;
}

void CameraManager::process(void)
{
    psyqo::Trig trig = g_madnightEngine.m_trig;

    // move camera forward
    if (g_madnightEngine.m_input.isButtonPressed(psyqo::AdvancedPad::Pad::Pad1a, psyqo::AdvancedPad::Button::Triangle))
    {
        psyqo::FixedPoint<12> forward = m_movement_speed;
        m_pos.x += trig.sin(m_angle.y) * forward;
        m_pos.z += trig.cos(m_angle.y) * forward;
    }

    // move camera back
    else if (g_madnightEngine.m_input.isButtonPressed(psyqo::AdvancedPad::Pad::Pad1a, psyqo::AdvancedPad::Button::Cross))
    {
        psyqo::FixedPoint<12> backwards = -m_movement_speed;
        m_pos.x += trig.sin(m_angle.y) * backwards;
        m_pos.z += trig.cos(m_angle.y) * backwards;
    }

    // move camera left
    if (g_madnightEngine.m_input.isButtonPressed(psyqo::AdvancedPad::Pad::Pad1a, psyqo::AdvancedPad::Button::Square))
    {
        psyqo::FixedPoint<12> left = m_movement_speed;
        m_pos.x += -trig.cos(m_angle.y) * left;
        m_pos.z += trig.sin(m_angle.y) * left;
    }

    // move camera right
    else if (g_madnightEngine.m_input.isButtonPressed(psyqo::AdvancedPad::Pad::Pad1a, psyqo::AdvancedPad::Button::Circle))
    {
        psyqo::FixedPoint<12> right = -m_movement_speed;
        m_pos.x += -trig.cos(m_angle.y) * right;
        m_pos.z += trig.sin(m_angle.y) * right;
    }

    // move camera up
    if (g_madnightEngine.m_input.isButtonPressed(psyqo::AdvancedPad::Pad::Pad1a, psyqo::AdvancedPad::Button::L1))
    {
        psyqo::FixedPoint<12> up = -m_movement_speed;
        m_pos.y += up;
    }

    // move camera down
    else if (g_madnightEngine.m_input.isButtonPressed(psyqo::AdvancedPad::Pad::Pad1a, psyqo::AdvancedPad::Button::R1))
    {
        psyqo::FixedPoint<12> down = m_movement_speed;
        m_pos.y += down;
    }
}

void CameraManager::set_rotation_matrix(void)
{
    auto rm_x = psyqo::SoftMath::generateRotationMatrix33(m_angle.x, psyqo::SoftMath::Axis::X, g_madnightEngine.m_trig);
    auto rm_y = psyqo::SoftMath::generateRotationMatrix33(m_angle.y, psyqo::SoftMath::Axis::Y, g_madnightEngine.m_trig);
    auto rm_z = psyqo::SoftMath::generateRotationMatrix33(m_angle.z, psyqo::SoftMath::Axis::Z, g_madnightEngine.m_trig);

    // xyz multiplication order
    psyqo::SoftMath::multiplyMatrix33(rm_y, rm_x, &rm_y);
    psyqo::SoftMath::multiplyMatrix33(rm_y, rm_z, &rm_y);

    // update rotation matrix
    m_rotation_matrix = rm_y;
}

psyqo::Vec3 &CameraManager::get_pos(void) { return m_pos; }
CAMERA_ANGLE *CameraManager::get_angle(void) { return &m_angle; }
psyqo::Matrix33 &CameraManager::get_rotation_matrix(void) { return m_rotation_matrix; };
