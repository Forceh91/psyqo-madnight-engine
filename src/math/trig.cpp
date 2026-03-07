#include "trig.hh"
#include "psyqo/trigonometry.hh"
#include <cstdint>

using namespace psyqo::fixed_point_literals;
using namespace psyqo::trig_literals;

/**
 * @brief Fixed-point atan2 implementation.
 *
 * Returns a psyqo::Angle in the range (-1.0_pi, 1.0_pi] representing fractions of Pi:
 *   1.0_pi =  180°
 *   0.5_pi =   90°
 *  0.25_pi =   45°
 *   0.0_pi =    0°
 *  -0.5_pi =  -90°
 *
 * Input: x in [-128, 127] (horizontal, positive = right)
 *        y in [-128, 127] (vertical,   positive = up)
 *
 * Uses a first-quadrant atan(t) LUT where t = min/max in [0,1],
 * then mirrors into the correct quadrant/sign.
 */

// --- atan LUT ---
// 65 entries covering t = 0.0 .. 1.0 in steps of 1/64.
// Values are atan(t)/Pi, scaled to fixed-point Q10 (multiply by 1024).
// atan(t)/Pi ranges from 0 (t=0) to 0.25 (t=1, i.e. 45°).
// Generated via: round(atan(i/64.0) / M_PI * 1024) for i in 0..64
static const int16_t ATAN2_LUT[65] = {
      0,   5,  10,  15,  20,  25,  30,  35,
     40,  45,  50,  55,  60,  65,  70,  74,
     79,  84,  88,  93,  97, 102, 106, 110,
    115, 119, 123, 127, 131, 135, 139, 143,
    147, 151, 154, 158, 161, 165, 168, 171,
    175, 178, 181, 184, 187, 190, 193, 196,
    198, 201, 204, 206, 209, 211, 214, 216,
    218, 221, 223, 225, 227, 229, 231, 233,
    256  // atan(1)/pi * 1024 = exactly 256 (= 45°)
};

/**
 * @brief Lookup atan(ratio) where ratio = num/den, both positive, den >= num.
 *        Returns result in Q10 fixed point (units of 1/1024 Pi).
 *        i.e. result range: [0, 256] representing [0°, 45°].
 */
static int16_t atan_first_octant(int32_t num, int32_t den)
{
    if (den == 0) return 256; // 45°, shouldn't happen if called correctly

    // Scale num to get a 6-bit index into LUT (0..64)
    // t = num/den in [0,1], index = t * 64
    // index = (num * 64) / den
    int32_t num_scaled = num * 64;
    int32_t idx = num_scaled / den;
    if (idx >= 64) idx = 63;

    // Linear interpolation between LUT[idx] and LUT[idx+1]
    // frac = (num * 64) % den  -- the remainder for lerp
    int32_t base  = ATAN2_LUT[idx];
    int32_t delta = ATAN2_LUT[idx + 1] - base;

    // frac in [0, den): lerp = base + delta * frac / den
    int32_t frac = num_scaled - idx * den;
    int16_t result = (int16_t)(base + (delta * frac) / den);
    return result;
}

/**
 * @brief Fixed-point atan2 returning a psyqo::Angle.
 *
 * @param y  Vertical component   [-128, 127], positive = up
 * @param x  Horizontal component [-128, 127], positive = right
 * @return   psyqo::Angle in the range (-1.0_pi, 1.0_pi], i.e. (-180°, 180°]
 *
 * Uses a compass/clock convention where up = 0°, rotating clockwise:
 *   0.0_pi  =   0° = up
 *   0.5_pi  =  90° = right
 *   1.0_pi  = 180° = down
 *  -0.5_pi  = -90° = left
 *
 * Internally computed in Q10 fixed point where 1024 = Pi = 180°,
 * then stored directly into Angle::value with no float conversion.
 *
 * Special cases:
 *   (0,  0) ->  0.0_pi
 *   (1,  0) ->  0.0_pi (up)
 *   (0,  1) ->  0.5_pi (right)
 *   (-1, 0) ->  1.0_pi (down)
 *   (0, -1) -> -0.5_pi (left)
 */
psyqo::Angle atan2_fixed(int16_t y, int16_t x)
{
    if (x == 0 && y == 0) return 0.0_pi;

    int32_t ax = x < 0 ? -x : x;
    int32_t ay = y < 0 ? -y : y;

    int16_t angle;

    // Treat Y as the primary axis (up = 0°), so we pass (ax, ay) when
    // ay >= ax, mirroring standard atan2 but with x and y swapped.
    if (ay >= ax) {
        // Octants where Y dominates: ratio = ax/ay in [0,1]
        angle = atan_first_octant(ax, ay);   // [0, 256] = [0°, 45°]
    } else {
        // Octants where X dominates: ratio = ay/ax in [0,1], mirror to 90°
        angle = 512 - atan_first_octant(ay, ax);  // [256, 512] = [45°, 90°]
    }

    // Quadrant reconstruction with Y-up-as-zero convention:
    //   Up-Right  (x>=0, y>=0): [  0°,  90°] -> angle as-is
    //   Down-Right(x>=0, y< 0): [ 90°, 180°] -> 1024 - angle
    //   Down-Left (x< 0, y< 0): [-180°,-90°] -> -(1024 - angle)
    //   Up-Left   (x< 0, y>=0): [ -90°,  0°] -> -angle
    if (x >= 0 && y < 0) {
        angle = 1024 - angle;
    } else if (x < 0 && y < 0) {
        angle = -(1024 - angle);
    } else if (x < 0 && y >= 0) {
        angle = -angle;
    }

    psyqo::Angle result = 0.0_pi;
    result.value = angle;
    return result;
}
