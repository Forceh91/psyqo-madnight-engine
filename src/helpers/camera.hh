#ifndef _CAMERA_H
#define _CAMERA_H

#include "psyqo/advancedpad.hh"
#include "psyqo/matrix.hh"
#include "psyqo/trigonometry.hh"
#include "psyqo/vector.hh"

typedef struct _CAMERA_ANGLE
{
    psyqo::Angle x;
    psyqo::Angle y;
    psyqo::Angle z;
} CAMERA_ANGLE;

class CameraManager final
{
    static psyqo::Vec3 m_pos;
    static CAMERA_ANGLE m_angle;
    static psyqo::Matrix33 m_rotation_matrix;
    static void set_rotation_matrix(void);

public:
    static void init(void);
    static void set_position(psyqo::FixedPoint<12> x, psyqo::FixedPoint<12> y, psyqo::FixedPoint<12> z);
    static psyqo::Vec3 &get_pos(void);
    static CAMERA_ANGLE *get_angle(void);
    static psyqo::Matrix33 &get_rotation_matrix(void);
    static void process(void);
};

#endif
