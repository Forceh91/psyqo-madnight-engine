#include "psyqo/fixed-point.hh"
#include "psyqo/trigonometry.hh"

psyqo::Angle atan2_fixed(int16_t y, int16_t x);
psyqo::Angle LerpAngle(psyqo::Angle a, psyqo::Angle b, psyqo::FixedPoint<10> t);