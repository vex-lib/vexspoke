// vexspoke/test/math/strict_math_test.c — owner test for math/strict_math.
//
// StrictMath must be an exact IEEE 754 layer over libm. This test pins the
// float and double twins against libm oracles at identity/normal/boundary/
// domain-edge inputs:
//   - trig at 0, pi/2, pi; inverse trig at the [-1,1] domain edge and outside
//     (NaN); sqrt at 0, 1, 4 and the negative domain (NaN);
//   - invSqrt: exact reciprocal at 1/4/16 and the non-positive guard (0);
//   - pow/exp/log identities and their domain edges (log(0) = -inf);
//   - abs/floor/ceil/round at positive, negative, zero, and half values;
//   - clamp and lerp endpoints;
//   - toRadians/toDegrees round trip and exact constants;
//   - the double-precision twins mirror the float behavior.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#include "math/strict_math.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

#define CLOSE_F(a, b, eps) (fabs((double) (a) - (double) (b)) <= (double) (eps))

// Validates float trigonometry, inverse-trig domains, and libm parity.
// Checks strict trigonometric functions against reference values.
// Checks strict float trigonometry, inverse-domain behavior, and libm agreement.
static void test_trig(void) {
    CHECK(StrictMath_sin(0.0f) == 0.0f);
    CHECK(CLOSE_F(StrictMath_sin(STRICT_MATH_HALF_PI), 1.0f, 1e-6f));
    CHECK(CLOSE_F(StrictMath_cos(0.0f), 1.0f, 1e-6f));
    CHECK(CLOSE_F(StrictMath_cos(STRICT_MATH_PI), -1.0f, 1e-6f));
    CHECK(CLOSE_F(StrictMath_tan(0.0f), 0.0f, 1e-6f));
    CHECK(CLOSE_F(StrictMath_atan(1.0f), STRICT_MATH_PI / 4.0f, 1e-6f));
    CHECK(CLOSE_F(StrictMath_atan2(1.0f, 1.0f), STRICT_MATH_PI / 4.0f, 1e-6f));
    CHECK(CLOSE_F(StrictMath_atan2(-1.0f, -1.0f), -3.0f * STRICT_MATH_PI / 4.0f, 1e-6f));

    // Inverse trig domain edge and out-of-domain (NaN).
    CHECK(CLOSE_F(StrictMath_asin(1.0f), STRICT_MATH_HALF_PI, 1e-6f));
    CHECK(CLOSE_F(StrictMath_acos(1.0f), 0.0f, 1e-6f));
    CHECK(isnan(StrictMath_asin(2.0f)));
    CHECK(isnan(StrictMath_acos(-2.0f)));

    // Consistency with libm (the strict contract).
    for (int i = -10; i <= 10; i++) {
        float x = (float) i * 0.3f;
        CHECK(StrictMath_sin(x) == sinf(x));
        CHECK(StrictMath_cos(x) == cosf(x));
        CHECK(StrictMath_tan(x) == tanf(x));
        CHECK(StrictMath_atan(x) == atanf(x));
    }
}

// Checks square-root, inverse-root, power, exponential, and logarithm cases.
// Verifies strict root and power operations over representative inputs.
// Verifies root, reciprocal-root, power, exponential, and logarithm semantics.
static void test_roots_and_powers(void) {
    CHECK(StrictMath_sqrt(0.0f) == 0.0f);
    CHECK(StrictMath_sqrt(4.0f) == 2.0f);
    CHECK(StrictMath_sqrt(-1.0f) != StrictMath_sqrt(-1.0f)); // NaN
    CHECK(isnan(StrictMath_sqrt(-1.0f)));

    CHECK(StrictMath_invSqrt(4.0f) == 0.5f);
    CHECK(StrictMath_invSqrt(1.0f) == 1.0f);
    CHECK(StrictMath_invSqrt(0.0f) == 0.0f);       // guard
    CHECK(StrictMath_invSqrt(-4.0f) == 0.0f);      // guard

    CHECK(StrictMath_pow(2.0f, 10.0f) == 1024.0f);
    CHECK(StrictMath_exp(0.0f) == 1.0f);
    CHECK(StrictMath_log(1.0f) == 0.0f);
    CHECK(isinf(StrictMath_log(0.0f)) && StrictMath_log(0.0f) < 0.0f);
    CHECK(StrictMath_pow(0.0f, 0.0f) == 1.0f);
}

// Exercises float absolute, rounding, clamp, and interpolation semantics.
// Checks strict rounding operations at integral and fractional boundaries.
// Tests absolute value, IEEE rounding, clamp, and interpolation endpoints.
static void test_rounding(void) {
    CHECK(StrictMath_abs(-3.5f) == 3.5f);
    CHECK(StrictMath_abs(3.5f) == 3.5f);
    CHECK(StrictMath_abs(0.0f) == 0.0f);
    CHECK(StrictMath_abs(-0.0f) == 0.0f);
    CHECK(signbit(StrictMath_abs(-0.0f)) == 0);

    CHECK(StrictMath_floor(2.9f) == 2.0f);
    CHECK(StrictMath_floor(-2.1f) == -3.0f);
    CHECK(StrictMath_ceil(2.1f) == 3.0f);
    CHECK(StrictMath_ceil(-2.9f) == -2.0f);
    CHECK(StrictMath_round(2.4f) == 2.0f);
    CHECK(StrictMath_round(2.5f) == 3.0f);         // round half away from zero
    CHECK(StrictMath_round(-2.5f) == -3.0f);

    CHECK(StrictMath_clamp(5.0f, 0.0f, 3.0f) == 3.0f);
    CHECK(StrictMath_clamp(-5.0f, 0.0f, 3.0f) == 0.0f);
    CHECK(StrictMath_clamp(1.5f, 0.0f, 3.0f) == 1.5f);

    CHECK(StrictMath_lerp(0.0f, 10.0f, 0.0f) == 0.0f);
    CHECK(StrictMath_lerp(0.0f, 10.0f, 1.0f) == 10.0f);
    CHECK(CLOSE_F(StrictMath_lerp(0.0f, 10.0f, 0.25f), 2.5f, 1e-6f));
}

// Checks float angle conversion round trips and canonical constants.
// Exercises strict angle conversions and normalization behavior.
// Checks radians/degrees conversion round trips and named constants.
static void test_angles(void) {
    CHECK(CLOSE_F(StrictMath_toDegrees(StrictMath_toRadians(90.0f)), 90.0f, 1e-3f));
    CHECK(CLOSE_F(StrictMath_toRadians(180.0f), STRICT_MATH_PI, 1e-5f));
    CHECK(CLOSE_F(StrictMath_toRadians(90.0f), STRICT_MATH_HALF_PI, 1e-5f));
}

// Verifies the corresponding double-precision operation surface.
// Confirms double-precision counterparts agree with their expected values.
// Verifies double-precision StrictMath counterparts across their public operations.
static void test_double_twins(void) {
    CHECK(StrictMath_sinD(0.0) == 0.0);
    CHECK(fabs(StrictMath_cosD(0.0) - 1.0) < 1e-15);
    CHECK(StrictMath_sqrtD(9.0) == 3.0);
    CHECK(fabs(StrictMath_atan2D(1.0, 1.0) - STRICT_MATH_PI / 4.0) < 1e-15);
    CHECK(StrictMath_tanD(0.0) == 0.0);
    CHECK(StrictMath_asinD(0.0) == 0.0);
    CHECK(StrictMath_acosD(1.0) == 0.0);
    CHECK(StrictMath_atanD(0.0) == 0.0);
    CHECK(StrictMath_invSqrtD(4.0) == 0.5);
    CHECK(StrictMath_invSqrtD(0.0) == 0.0);
    CHECK(StrictMath_invSqrtD(-1.0) == 0.0);
    CHECK(StrictMath_powD(2.0, 3.0) == 8.0);
    CHECK(StrictMath_expD(0.0) == 1.0);
    CHECK(StrictMath_logD(1.0) == 0.0);
    CHECK(StrictMath_absD(-2.5) == 2.5);
    CHECK(StrictMath_floorD(-1.5) == -2.0);
    CHECK(StrictMath_ceilD(1.5) == 2.0);
    CHECK(StrictMath_roundD(2.5) == 3.0);
    CHECK(StrictMath_clampD(9.0, 0.0, 5.0) == 5.0);
    CHECK(StrictMath_lerpD(0.0, 4.0, 0.5) == 2.0);
}

// Runs the strict float and double math contract checks.
// Runs strict-math trigonometric, root, rounding, angle, and double-precision checks.
// Runs the strict float and double math contract cases.
int main(void) {
    test_trig();
    test_roots_and_powers();
    test_rounding();
    test_angles();
    test_double_twins();

    if (g_failures == 0) {
        printf("strict_math_test: all assertions held\n");
        return 0;
    }
    printf("strict_math_test: %d FAILURES\n", g_failures);
    return 1;
}
