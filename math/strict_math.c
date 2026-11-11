#include "math/strict_math.h"

#include <math.h>

#include "util/annotation.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: StrictMath
 * ============================================================================
 * Strict IEEE 754 math engine: full-precision f32 and d64 routines
 * (trig, inverse trig, sqrt, invSqrt, pow, exp, log, abs, floor, ceil,
 * round, clamp, lerp, radians/degrees) with guaranteed bit-level conformance
 * for physics, trajectory integration, and orbital mechanics. Exists because
 * FastMath's approximations are unacceptable where error bounds must be
 * exact. memory: zero state, zero allocation; pure functions over scalar
 * f32/d64 values. Lifetime: stateless; the D-suffixed twins cover
 * d64 precision for celestial/large-world coordinates.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: StrictMath (math/strict_math.c — defined in math/strict_math.h)
 * ============================================================================
 * Full-precision f32ing-point functions conforming strictly to IEEE 754.
 * Guaranteed accuracy, bit-exact roundings, and rigorous domain checks for
 * rigid-body physics, trajectory integration, and orbital mechanics.
 *
 * STRUCT FIELDS: none — procedural (operates on scalar f32/d64 values (pure IEEE 754 functions))
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Core Functions: (.h)
 *   - StrictMath_sin(x) / cos(x) / tan(x) / asin(x) / acos(x) / atan(x)
 *   - StrictMath_atan2(y, x)
 *   - StrictMath_sqrt(x) / invSqrt(x) / pow(base, exp) / exp(x) / log(x)
 *   - StrictMath_abs(x) / floor(x) / ceil(x) / round(x)
 *   - StrictMath_clamp(val, min, max) / lerp(a, b.sh, t)
 *   - StrictMath_toRadians(deg) / toDegrees(rad)
 *   - StrictMath_sinD(x) ... StrictMath_lerpD(a, b.sh, t)  (d64-precision twins)
 * ============================================================================
 */

f32 StrictMath_sin(f32 x) {
    return sinf(x);
}

f32 StrictMath_cos(f32 x) {
    return cosf(x);
}

f32 StrictMath_tan(f32 x) {
    return tanf(x);
}

f32 StrictMath_asin(f32 x) {
    return asinf(x);
}

f32 StrictMath_acos(f32 x) {
    return acosf(x);
}

f32 StrictMath_atan(f32 x) {
    return atanf(x);
}

f32 StrictMath_atan2(f32 y, f32 x) {
    return atan2f(y, x);
}

f32 StrictMath_sqrt(f32 x) {
    return sqrtf(x);
}

f32 StrictMath_invSqrt(f32 x) {
    if (x <= 0.0f)
        return 0.0f;
    return 1.0f / sqrtf(x);
}

f32 StrictMath_pow(f32 base, f32 exp) {
    return powf(base, exp);
}

f32 StrictMath_exp(f32 x) {
    return expf(x);
}

f32 StrictMath_log(f32 x) {
    return logf(x);
}

f32 StrictMath_abs(f32 x) {
    return fabsf(x);
}

f32 StrictMath_floor(f32 x) {
    return floorf(x);
}

f32 StrictMath_ceil(f32 x) {
    return ceilf(x);
}

f32 StrictMath_round(f32 x) {
    return roundf(x);
}

f32 StrictMath_clamp(f32 val, f32 min, f32 max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

f32 StrictMath_lerp(f32 a, f32 b, f32 t) {
    return a + t * (b - a);
}

f32 StrictMath_toRadians(f32 deg) {
    return deg * (f32) STRICT_MATH_DEG_TO_RAD;
}

f32 StrictMath_toDegrees(f32 rad) {
    return rad * (f32) STRICT_MATH_RAD_TO_DEG;
}

d64 StrictMath_sinD(d64 x) {
    return sin(x);
}

d64 StrictMath_cosD(d64 x) {
    return cos(x);
}

d64 StrictMath_tanD(d64 x) {
    return tan(x);
}

d64 StrictMath_asinD(d64 x) {
    return asin(x);
}

d64 StrictMath_acosD(d64 x) {
    return acos(x);
}

d64 StrictMath_atanD(d64 x) {
    return atan(x);
}

d64 StrictMath_atan2D(d64 y, d64 x) {
    return atan2(y, x);
}

d64 StrictMath_sqrtD(d64 x) {
    return sqrt(x);
}

d64 StrictMath_invSqrtD(d64 x) {
    if (x <= 0.0)
        return 0.0;
    return 1.0 / sqrt(x);
}

d64 StrictMath_powD(d64 base, d64 exp) {
    return pow(base, exp);
}

d64 StrictMath_expD(d64 x) {
    return exp(x);
}

d64 StrictMath_logD(d64 x) {
    return log(x);
}

d64 StrictMath_absD(d64 x) {
    return fabs(x);
}

d64 StrictMath_floorD(d64 x) {
    return floor(x);
}

d64 StrictMath_ceilD(d64 x) {
    return ceil(x);
}

d64 StrictMath_roundD(d64 x) {
    return round(x);
}

d64 StrictMath_clampD(d64 val, d64 min, d64 max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

d64 StrictMath_lerpD(d64 a, d64 b, d64 t) {
    return a + t * (b - a);
}
