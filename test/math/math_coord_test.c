#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "math/coord_frame.h"
#include "math/strict_math.h"
#include "math/fast_math.h"
#include "math/vex-math.h"
#include "lang/vec2/vec2.h"
#include "lang/vec3/vec3.h"
#include "lang/vec3/vec3d.h"
#include "lang/vec3/vec3_int_f32.h"
#include "lang/vec3/vec3_long_f64.h"
#include "lang/vec4/vec4.h"
#include "lang/vec4/vec4d.h"

static int g_failures = 0;

#define CHECK(name, cond) do { \
    if (cond) { printf("[math_coord_test] PASS %s\n", name); } \
    else { printf("[math_coord_test] FAIL %s\n", name); g_failures++; } \
} while (0)

// Integrates coordinate-frame, math-facade, and spatial-vector behavior checks.
// Verifies coordinate-aware math operations across the selected frame mappings.
// Integrates frame mappings, strict/fast math dispatch, and vector direction semantics.
int main(void) {
    printf("=== Running 12-Frame Math & Vector Test Suite ===\n");

    // 1. The 12 Coordinate Frames
    CHECK("default frame is Y_UP_LEFT", COORD_FRAME_DEFAULT == COORD_FRAME_Y_UP_LEFT);
    CHECK("frame 0 is valid", CoordFrame_isValid(COORD_FRAME_Y_UP_LEFT));
    CHECK("frame 11 is valid", CoordFrame_isValid(COORD_FRAME_X_DOWN_RIGHT));
    CHECK("frame 12 is invalid", !CoordFrame_isValid(12));

    CoordBasis b0 = CoordFrame_getBasis(COORD_FRAME_Y_UP_LEFT);
    CHECK("Y_UP_LEFT basis X axis is horizontal", b0.axis[0] == 0 && b0.sign[0] == 1);
    CHECK("Y_UP_LEFT basis Y axis is vertical", b0.axis[1] == 1 && b0.sign[1] == 1);
    CHECK("Y_UP_LEFT basis Z axis is depth", b0.axis[2] == 2 && b0.sign[2] == 1);

    CoordBasis b1 = CoordFrame_getBasis(COORD_FRAME_Y_UP_RIGHT);
    CHECK("Y_UP_RIGHT Z sign is negative (Right-handed)", b1.sign[2] == -1);

    // 2. Strict Math vs Fast Math
    f32 s_sin = StrictMath_sin((f32) STRICT_MATH_HALF_PI);
    CHECK("strict sin(pi/2) is 1.0", fabsf(s_sin - 1.0f) < 1e-6f);

    f32 f_sin = FastMath_sin(FAST_MATH_HALF_PI);
    CHECK("fast sin(pi/2) is ~1.0", fabsf(f_sin - 1.0f) < 0.01f);

    f32 inv_strict = StrictMath_invSqrt(16.0f);
    f32 inv_fast = FastMath_invSqrt(16.0f);
    CHECK("strict invSqrt(16) is 0.25", fabsf(inv_strict - 0.25f) < 1e-6f);
    CHECK("fast invSqrt(16) is ~0.25", fabsf(inv_fast - 0.25f) < 0.01f);

    // 3. Unified Math Facade
    CHECK("Math_sin is strict by default", fabsf(Math_sin((f32)MATH_HALF_PI) - 1.0f) < 1e-6f);
    CHECK("Math_fast_sin uses approximation", fabsf(Math_fast_sin(FAST_MATH_HALF_PI) - 1.0f) < 0.01f);
    CHECK("Math_fast_invSqrt is fast approx", fabsf(Math_fast_invSqrt(4.0f) - 0.5f) < 0.01f);

    // 4. Coord3 Spatial Semantics & Directional Inverses
    Coord3 v = { .horizontal = 14.0f, .vertical = 5.0f, .depth = 28.0f, .frame = (u32) COORD_FRAME_DEFAULT };

    // Nuance check: if front coordinate is 28, getBack() must be -28!
    CHECK("Coord3 getFront is 28", Coord3_getFront(&v) == 28.0f);
    CHECK("Coord3 getBack is -28", Coord3_getBack(&v) == -28.0f);
    CHECK("Coord3 getRight is 14", Coord3_getRight(&v) == 14.0f);
    CHECK("Coord3 getLeft is -14", Coord3_getLeft(&v) == -14.0f);
    CHECK("Coord3 getUp is 5", Coord3_getUp(&v) == 5.0f);
    CHECK("Coord3 getDown is -5", Coord3_getDown(&v) == -5.0f);

    Coord3_setBack(&v, 28.0f);
    CHECK("Coord3 setBack(28) sets depth to -28", Coord3_getFront(&v) == -28.0f);
    CHECK("Coord3 getBack is now 28", Coord3_getBack(&v) == 28.0f);

    // One-line indexing math for coordinates
    Coord3_setFront(&v, 28.0f);
    CHECK("Coord3 getX in Y_UP_LEFT is horizontal", Coord3_getX(&v) == 14.0f);
    CHECK("Coord3 getY in Y_UP_LEFT is vertical", Coord3_getY(&v) == 5.0f);
    CHECK("Coord3 getZ in Y_UP_LEFT is depth", Coord3_getZ(&v) == 28.0f);

    // Coordinate Frame transform: to OpenGL right-handed Y_UP_RIGHT (Z is negated)
    Coord3 v_gl;
    Coord3_toFrame(&v, COORD_FRAME_Y_UP_RIGHT, &v_gl);
    CHECK("v_gl getX is 14", Coord3_getX(&v_gl) == 14.0f);
    CHECK("v_gl getY is 5", Coord3_getY(&v_gl) == 5.0f);
    CHECK("v_gl getZ is -28 (negated depth)", Coord3_getZ(&v_gl) == -28.0f);

    // 5. Vec3d d64 Precision
    Vec3d vd = { .horizontal = 100.0, .vertical = 200.0, .depth = 300.0, .frame = (u64) COORD_FRAME_DEFAULT };
    CHECK("Vec3d getFront is 300", Vec3d_getFront(&vd) == 300.0);
    CHECK("Vec3d getBack is -300", Vec3d_getBack(&vd) == -300.0);

    // 6. Vec3IntFloat Sector Rebalance
    Vec3IntFloat vif;
    vif.horizontal.sector = 0;
    vif.horizontal.local = 150.0f;
    vif.vertical.sector = 0;
    vif.vertical.local = 50.0f;
    vif.depth.sector = 0;
    vif.depth.local = 250.0f;
    vif.frame = (u32) COORD_FRAME_DEFAULT;
    Vec3IntFloat_rebalance(&vif, 100.0f);
    CHECK("vif horizontal sector rebalanced to 1", vif.horizontal.sector == 1);
    CHECK("vif horizontal local rebalanced to 50", vif.horizontal.local == 50.0f);
    CHECK("vif depth sector rebalanced to 2", vif.depth.sector == 2);
    CHECK("vif depth local rebalanced to 50", vif.depth.local == 50.0f);

    // 7. Coord2 & Coord4 Spatial Inverses
    Coord2 v2 = { .horizontal = 10.0f, .vertical = 20.0f };
    CHECK("Coord2 getRight is 10", Coord2_getRight(&v2) == 10.0f);
    CHECK("Coord2 getLeft is -10", Coord2_getLeft(&v2) == -10.0f);
    CHECK("Coord2 getUp is 20", Coord2_getUp(&v2) == 20.0f);
    CHECK("Coord2 getDown is -20", Coord2_getDown(&v2) == -20.0f);

    Coord4 v4 = { .horizontal = 1.0f, .vertical = 2.0f, .depth = 3.0f, .w = 1.0f };
    CHECK("Coord4 getRight is 1", Coord4_getRight(&v4) == 1.0f);
    CHECK("Coord4 getLeft is -1", Coord4_getLeft(&v4) == -1.0f);
    CHECK("Coord4 getFront is 3", Coord4_getFront(&v4) == 3.0f);
    CHECK("Coord4 getBack is -3", Coord4_getBack(&v4) == -3.0f);
    CHECK("Coord4 w preserved", v4.w == 1.0f);

    printf("\n=== 12-Frame Math & Vector Test Summary: %d failures ===\n", g_failures);
    return g_failures > 0 ? 1 : 0;
}
