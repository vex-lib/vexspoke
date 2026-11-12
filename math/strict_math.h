#ifndef MATH_STRICT_MATH_H
#define MATH_STRICT_MATH_H

#include "vex.h"

// math/strict_math.h — Strict IEEE 754 Math Engine.
//
// Single Class Per File Law: StrictMath.
//
// Full-precision, strictly IEEE 754-compliant f32ing-point routines for
// physics simulations, celestial mechanics, exact trajectory integration,
// and collision manifolds. Guaranteed bit-level conformance and error bounds.

#define STRICT_MATH_PI         3.14159265358979323846
#define STRICT_MATH_HALF_PI    1.57079632679489661923
#define STRICT_MATH_TWO_PI     6.28318530717958647692
#define STRICT_MATH_E          2.71828182845904523536
#define STRICT_MATH_TAU        6.28318530717958647692
#define STRICT_MATH_DEG_TO_RAD 0.01745329251994329577
#define STRICT_MATH_RAD_TO_DEG 57.2957795130823208768

// Single-precision strict routines
f32 StrictMath_sin(f32 x);
f32 StrictMath_cos(f32 x);
f32 StrictMath_tan(f32 x);
f32 StrictMath_asin(f32 x);
f32 StrictMath_acos(f32 x);
f32 StrictMath_atan(f32 x);
f32 StrictMath_atan2(f32 y, f32 x);
f32 StrictMath_sqrt(f32 x);
f32 StrictMath_invSqrt(f32 x);
f32 StrictMath_pow(f32 base, f32 exp);
f32 StrictMath_exp(f32 x);
f32 StrictMath_log(f32 x);
f32 StrictMath_abs(f32 x);
f32 StrictMath_floor(f32 x);
f32 StrictMath_ceil(f32 x);
f32 StrictMath_round(f32 x);
f32 StrictMath_clamp(f32 val, f32 min, f32 max);
f32 StrictMath_lerp(f32 a, f32 b, f32 t);
f32 StrictMath_toRadians(f32 deg);
f32 StrictMath_toDegrees(f32 rad);

// d64-precision strict routines for celestial & large-world coordinates
d64 StrictMath_sinD(d64 x);
d64 StrictMath_cosD(d64 x);
d64 StrictMath_tanD(d64 x);
d64 StrictMath_asinD(d64 x);
d64 StrictMath_acosD(d64 x);
d64 StrictMath_atanD(d64 x);
d64 StrictMath_atan2D(d64 y, d64 x);
d64 StrictMath_sqrtD(d64 x);
d64 StrictMath_invSqrtD(d64 x);
d64 StrictMath_powD(d64 base, d64 exp);
d64 StrictMath_expD(d64 x);
d64 StrictMath_logD(d64 x);
d64 StrictMath_absD(d64 x);
d64 StrictMath_floorD(d64 x);
d64 StrictMath_ceilD(d64 x);
d64 StrictMath_roundD(d64 x);
d64 StrictMath_clampD(d64 val, d64 min, d64 max);
d64 StrictMath_lerpD(d64 a, d64 b, d64 t);

#endif
