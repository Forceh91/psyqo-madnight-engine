#include "psyqo/fixed-point.hh"
#include "psyqo/trigonometry.hh"

psyqo::Angle atan2_fixed(int16_t y, int16_t x);
psyqo::Angle LerpAngle(const psyqo::Angle &a, const psyqo::Angle &b, const psyqo::FixedPoint<10> &t);