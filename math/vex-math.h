#ifndef MATH_MATH_H
#define MATH_MATH_H

#include "vex.h"
#include "math/strict_math.h"
#include "math/fast_math.h"

// math/math.h — Unified Math Facade.
//
// Single Class Per File Law: Math.
//
// Unifies the Strict IEEE 754 precision math engine with the Relaxed Fast Math
// approximations under a single consistent API.
//
// DOCTRINE:
//   - Functions without the "fast_" prefix are STRICT (exact IEEE 754 precision by default).
//   - Functions with the "fast_" prefix are RELAXED APPROXIMATIONS (violates IEEE 754 for speed).

#define MATH_PI         STRICT_MATH_PI
#define MATH_HALF_PI    STRICT_MATH_HALF_PI
#define MATH_TWO_PI     STRICT_MATH_TWO_PI
#define MATH_E          STRICT_MATH_E
#define MATH_TAU        STRICT_MATH_TAU
#define MATH_DEG_TO_RAD STRICT_MATH_DEG_TO_RAD
#define MATH_RAD_TO_DEG STRICT_MATH_RAD_TO_DEG

// Strict by default (Exact IEEE 754 precision)
f32 Math_sin(f32 x);
f32 Math_cos(f32 x);
f32 Math_tan(f32 x);
f32 Math_asin(f32 x);
f32 Math_acos(f32 x);
f32 Math_atan(f32 x);
f32 Math_atan2(f32 y, f32 x);
f32 Math_sqrt(f32 x);
f32 Math_invSqrt(f32 x);
f32 Math_pow(f32 base, f32 exp);
f32 Math_exp(f32 x);
f32 Math_log(f32 x);
f32 Math_abs(f32 x);
f32 Math_floor(f32 x);
f32 Math_ceil(f32 x);
f32 Math_round(f32 x);
f32 Math_clamp(f32 val, f32 min, f32 max);
f32 Math_lerp(f32 a, f32 b, f32 t);
f32 Math_toRadians(f32 deg);
f32 Math_toDegrees(f32 rad);

// d64 precision strict
d64 Math_sinD(d64 x);
d64 Math_cosD(d64 x);
d64 Math_sqrtD(d64 x);
d64 Math_atan2D(d64 y, d64 x);

// Fast approximations (deliberately relaxes IEEE 754 for real-time 3D/graphics)
f32 Math_fast_sin(f32 x);
f32 Math_fast_cos(f32 x);
f32 Math_fast_tan(f32 x);
f32 Math_fast_atan(f32 x);
f32 Math_fast_atan2(f32 y, f32 x);
f32 Math_fast_invSqrt(f32 x);
f32 Math_fast_inv(f32 x);
f32 Math_fast_abs(f32 x);
f32 Math_fast_round(f32 x);
f32 Math_fast_clamp(f32 val, f32 min, f32 max);
f32 Math_fast_lerp(f32 a, f32 b, f32 t);
bool  Math_fast_approxEqual(f32 a, f32 b, f32 epsilon);

#endif
