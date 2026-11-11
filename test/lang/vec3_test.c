// vexspoke/test/lang/vec3_test.c — owner test for lang/vec3 (and vec3/vec3).
//
// Proves the frame-carrying 3D vector:
//   - zero/3-arg/4-arg construction; an invalid frame in Coord3_4 falls back to
//     the default, setFrame ignores an invalid frame;
//   - directional aliases and exact inverse signs; frame metadata survives
//     arithmetic and normalization;
//   - frame-mapped accessors (default frame + Y_DOWN sign flip);
//   - dest-last arithmetic with aliasing, div-by-zero refusing to write;
//   - dot/cross (right-handed identities), length, strict and fast normalize
//     with zero guards, distance, angle, projection, reflection, clamp, abs,
//     and lerp.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#include "lang/vec3.h"
#include "lang/vec3/vec3d.h"
#include "lang/vec3/vec3_int_f32.h"
#include "lang/vec3/vec3_long_f64.h"
#include "nio/mem.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

#define CLOSE(a, b) (fabsf((a) - (b)) <= 1e-4f)

// Checks constructor arities, frame validation, and free behavior.
static void test_construction(void) {
    Coord3 *z = Coord3();

    CHECK(z != nullptr);

    CHECK(Coord3_getX(z) == 0.0f && Coord3_getY(z) == 0.0f && Coord3_getZ(z) == 0.0f);

    CHECK(Coord3_getFrame(z) == COORD_FRAME_DEFAULT);

    Coord3_free(z);

    Coord3 *v = Coord3(1.0f, 2.0f, 3.0f);

    CHECK(v != nullptr);

    CHECK(Coord3_getRight(v) == 1.0f && Coord3_getUp(v) == 2.0f && Coord3_getFront(v) == 3.0f);

    CHECK(Coord3_getFrame(v) == COORD_FRAME_DEFAULT);

    Coord3 *f = Coord3(4.0f, 5.0f, 6.0f, COORD_FRAME_Z_UP_RIGHT);

    CHECK(Coord3_getFrame(f) == COORD_FRAME_Z_UP_RIGHT);
    // Invalid frame is rejected at construction and replaced by the default.
    Coord3 *bad = Coord3(1.0f, 1.0f, 1.0f, (CoordFrame) 999);

    CHECK(Coord3_getFrame(bad) == COORD_FRAME_DEFAULT);

    Coord3_free(v);

    Coord3_free(f);

    Coord3_free(bad);

    Coord3_free(nullptr);

    Vec3d *wide = Vec3d();

    CHECK(wide != nullptr && (*wide).horizontal == 0.0 && (*wide).frame == COORD_FRAME_DEFAULT);

    Vec3d_free(wide);

    wide = Vec3d(1.25, -2.5, 3.75);

    CHECK(wide != nullptr && (*wide).depth == 3.75);

    Vec3d_free(wide);

    wide = Vec3d(1.25, -2.5, 3.75, COORD_FRAME_Z_UP_RIGHT);

    CHECK(wide != nullptr && (*wide).frame == COORD_FRAME_Z_UP_RIGHT);

    Vec3d_free(wide);

    Vec3IntFloat *sectorFloat = Vec3IntFloat();

    CHECK(sectorFloat != nullptr);

    Vec3IntFloat zeroFloat = *sectorFloat;

    CHECK(zeroFloat.horizontal.sector == 0);

    Vec3IntFloat_free(sectorFloat);

    sectorFloat = Vec3IntFloat(1, 0.25f, 2, 0.5f, 3, 0.75f, COORD_FRAME_DEFAULT);

    CHECK(sectorFloat != nullptr);

    Vec3IntFloat sectorValue = *sectorFloat;

    CHECK(sectorValue.depth.sector == 3 && sectorValue.depth.local == 0.75f);

    Vec3IntFloat_free(sectorFloat);

    Vec3Longf64 *sectorf64 = Vec3Longf64();

    CHECK(sectorf64 != nullptr);

    Vec3Longf64 zerof64 = *sectorf64;

    CHECK(zerof64.horizontal.sector == 0);

    Vec3Longf64_free(sectorf64);

    sectorf64 = Vec3Longf64(1, 0.25, 2, 0.5, 3, 0.75, COORD_FRAME_DEFAULT);

    CHECK(sectorf64 != nullptr);

    Vec3Longf64 f64Value = *sectorf64;

    CHECK(f64Value.depth.sector == 3 && f64Value.depth.local == 0.75);

    Vec3Longf64_free(sectorf64);
}

// Checks directional aliases, frame-aware accessors, and frame-preserving copy.
static void test_accessors_and_frame(void) {
    Coord3 v = { 0 };

    Coord3_set(&v, 1.0f, 2.0f, 3.0f);

    CHECK(Coord3_getRight(&v) == 1.0f && Coord3_getLeft(&v) == -1.0f);

    CHECK(Coord3_getUp(&v) == 2.0f && Coord3_getDown(&v) == -2.0f);

    CHECK(Coord3_getFront(&v) == 3.0f && Coord3_getBack(&v) == -3.0f);

    Coord3_setLeft(&v, 5.0f);

    CHECK(Coord3_getRight(&v) == -5.0f && Coord3_getLeft(&v) == 5.0f);

    Coord3_setDown(&v, 5.0f);

    CHECK(Coord3_getUp(&v) == -5.0f && Coord3_getDown(&v) == 5.0f);

    Coord3_setBack(&v, 5.0f);

    CHECK(Coord3_getFront(&v) == -5.0f && Coord3_getBack(&v) == 5.0f);

    Coord3_setX(&v, 7.0f);

    CHECK(Coord3_getX(&v) == 7.0f);

    // Default frame maps X/Y/Z straight to horizontal/vertical/depth.
    Coord3_set(&v, 10.0f, 20.0f, 30.0f);

    Coord3_setFrame(&v, COORD_FRAME_Y_DOWN_LEFT);         // flips Y

    CHECK(Coord3_getFrame(&v) == COORD_FRAME_Y_DOWN_LEFT);

    CHECK(Coord3_getX(&v) == 10.0f);

    CHECK(Coord3_getY(&v) == -20.0f);                     // sign-mapped

    CHECK(Coord3_getZ(&v) == 30.0f);

    CHECK(Coord3_getYInFrame(&v, COORD_FRAME_Y_UP_LEFT) == 20.0f);

    // Invalid frame set is ignored (keeps the current frame).
    Coord3_setFrame(&v, (CoordFrame) 42);

    CHECK(Coord3_getFrame(&v) == COORD_FRAME_Y_DOWN_LEFT);

    // Copy carries the frame.
    Coord3 c = { 0 };

    Coord3_copy(&v, &c);

    CHECK(Coord3_getFrame(&c) == Coord3_getFrame(&v));

    CHECK(Coord3_getRight(&c) == Coord3_getRight(&v));
}

// Checks arithmetic, frame propagation, division rejection, and null guards.
static void test_arithmetic(void) {
    Coord3 a = { 0 }, b = { 0 }, d = { 0 };

    Coord3_set(&a, 3.0f, -4.0f, 2.0f);

    Coord3_set(&b, 1.0f, 2.0f, -3.0f);

    Coord3_add(&a, &b, &d);

    CHECK(Coord3_getX(&d) == 4.0f && Coord3_getY(&d) == -2.0f && Coord3_getZ(&d) == -1.0f);

    Coord3_sub(&a, &b, &d);

    CHECK(Coord3_getX(&d) == 2.0f && Coord3_getY(&d) == -6.0f && Coord3_getZ(&d) == 5.0f);

    Coord3_mul(&a, 2.0f, &d);

    CHECK(Coord3_getX(&d) == 6.0f && Coord3_getY(&d) == -8.0f && Coord3_getZ(&d) == 4.0f);

    Coord3_div(&a, 2.0f, &d);

    CHECK(Coord3_getX(&d) == 1.5f && Coord3_getY(&d) == -2.0f && Coord3_getZ(&d) == 1.0f);

    Coord3 keep = { 0 };

    Coord3_set(&keep, 9.0f, 9.0f, 9.0f);

    Coord3_div(&a, 0.0f, &keep);

    CHECK(Coord3_getZ(&keep) == 9.0f);

    // Frame metadata is carried through arithmetic.
    Coord3 framed = { 0 };

    Coord3_set(&framed, 1.0f, 1.0f, 1.0f);

    Coord3_setFrame(&framed, COORD_FRAME_Z_UP_LEFT);

    Coord3_add(&framed, &framed, &d);

    CHECK(Coord3_getFrame(&d) == COORD_FRAME_Z_UP_LEFT);

    Coord3_add(nullptr, &b, &d);

    Coord3_add(&a, nullptr, &d);

    Coord3_add(&a, &b, nullptr);

    Coord3_mul(nullptr, 2.0f, &d);
}

// Checks vector products, normalization, projection, reflection, and geometry.
static void test_geometry(void) {
    Coord3 a = { 0 }, b = { 0 }, d = { 0 };

    Coord3_set(&a, 3.0f, 4.0f, 0.0f);

    Coord3_set(&b, 1.0f, 0.0f, 0.0f);

    CHECK(Coord3_dot(&a, &b) == 3.0f);

    CHECK(Coord3_lengthSquared(&a) == 25.0f);

    CHECK(CLOSE(Coord3_length(&a), 5.0f));

    // Right-handed cross identities.
    Coord3 ex = { 0 }, ey = { 0 }, ez = { 0 };

    Coord3_set(&ex, 1.0f, 0.0f, 0.0f);

    Coord3_set(&ey, 0.0f, 1.0f, 0.0f);

    Coord3_set(&ez, 0.0f, 0.0f, 1.0f);

    Coord3_cross(&ex, &ey, &d);

    CHECK(Coord3_getX(&d) == 0.0f && Coord3_getY(&d) == 0.0f && Coord3_getZ(&d) == 1.0f);

    Coord3_cross(&ey, &ez, &d);

    CHECK(Coord3_getX(&d) == 1.0f);

    Coord3_cross(&ez, &ex, &d);

    CHECK(Coord3_getY(&d) == 1.0f);

    Coord3_cross(&b, &b, &d);

    CHECK(Coord3_lengthSquared(&d) == 0.0f);

    // Normalize.
    Coord3_normalize(&a, &d);

    CHECK(CLOSE(Coord3_length(&d), 1.0f));

    CHECK(CLOSE(Coord3_getX(&d), 0.6f) && CLOSE(Coord3_getY(&d), 0.8f));

    Coord3_fastNormalize(&a, &d);

    CHECK(fabsf(Coord3_length(&d) - 1.0f) <= 0.01f);
    // Zero-length normalization yields zero (both strict and fast).
    Coord3 zero = { 0 };

    Coord3_normalize(&zero, &d);

    CHECK(Coord3_lengthSquared(&d) == 0.0f);

    Coord3_fastNormalize(&zero, &d);

    CHECK(Coord3_lengthSquared(&d) == 0.0f);

    // Distance.
    CHECK(CLOSE(Coord3_distance(&ex, &ey), sqrtf(2.0f)));

    // Angle.
    CHECK(CLOSE(Coord3_angle(&ex, &ex), 0.0f));

    CHECK(CLOSE(Coord3_angle(&ex, &ey), 1.5707963f));

    CHECK(CLOSE(Coord3_angle(&ex, &(Coord3){ .data = { -1.0f, 0.0f, 0.0f, 0.0f } }), 3.1415927f));

    CHECK(Coord3_angle(&zero, &ex) == 0.0f);

    // Projection: project (3,4,0) onto (1,0,0) -> (3,0,0).
    Coord3_project(&a, &b, &d);

    CHECK(CLOSE(Coord3_getX(&d), 3.0f) && CLOSE(Coord3_getY(&d), 0.0f));
    // Projecting onto a zero vector yields zero.
    Coord3_project(&a, &zero, &d);

    CHECK(Coord3_lengthSquared(&d) == 0.0f);

    // Reflection of (1,-1,0) off normal (0,1,0) -> (1,1,0).
    Coord3 inc = { 0 }, nrm = { 0 };

    Coord3_set(&inc, 1.0f, -1.0f, 0.0f);

    Coord3_set(&nrm, 0.0f, 1.0f, 0.0f);

    Coord3_reflect(&inc, &nrm, &d);

    CHECK(CLOSE(Coord3_getX(&d), 1.0f) && CLOSE(Coord3_getY(&d), 1.0f));

    // Clamp and abs.
    Coord3 wild = { 0 };

    Coord3_set(&wild, -9.0f, 5.0f, 0.5f);

    Coord3_clamp(&wild, -1.0f, 1.0f, &d);

    CHECK(Coord3_getX(&d) == -1.0f && Coord3_getY(&d) == 1.0f && Coord3_getZ(&d) == 0.5f);

    Coord3_abs(&wild, &d);

    CHECK(Coord3_getX(&d) == 9.0f && Coord3_getY(&d) == 5.0f);

    // Lerp endpoints + midpoint.
    Coord3_lerp(&ex, &ey, 0.5f, &d);

    CHECK(CLOSE(Coord3_getX(&d), 0.5f) && CLOSE(Coord3_getY(&d), 0.5f));

    Coord3_lerp(&ex, &ey, 0.0f, &d);

    CHECK(Coord3_getX(&d) == 1.0f && Coord3_getY(&d) == 0.0f);
}

// Checks coordinate-frame conversion and invalid-target fallback.
static void test_to_frame(void) {
    Coord3 src = { 0 };

    Coord3_set(&src, 1.0f, 2.0f, 3.0f);

    Coord3 dst = { 0 };

    Coord3_toFrame(&src, COORD_FRAME_X_UP_RIGHT, &dst);

    CHECK(Coord3_getFrame(&dst) == COORD_FRAME_X_UP_RIGHT);

    CHECK(Coord3_getRight(&dst) == 1.0f && Coord3_getUp(&dst) == 2.0f && Coord3_getFront(&dst) == 3.0f);
    // Invalid target falls back to the default.
    Coord3_toFrame(&src, (CoordFrame) 77, &dst);

    CHECK(Coord3_getFrame(&dst) == COORD_FRAME_DEFAULT);
}

// Checks safe defaults and no-op behavior for null Coord3 arguments.
static void test_nulls(void) {
    Coord3 d = { 0 };

    CHECK(Coord3_getRight(nullptr) == 0.0f);

    CHECK(Coord3_getLeft(nullptr) == 0.0f);

    CHECK(Coord3_getUp(nullptr) == 0.0f);

    CHECK(Coord3_getFront(nullptr) == 0.0f);

    CHECK(Coord3_getX(nullptr) == 0.0f);

    CHECK(Coord3_getFrame(nullptr) == COORD_FRAME_DEFAULT);

    Coord3_set(nullptr, 1, 2, 3);

    Coord3_setFrame(nullptr, COORD_FRAME_Y_UP_LEFT);

    Coord3_copy(nullptr, nullptr);

    CHECK(Coord3_dot(nullptr, nullptr) == 0.0f);

    CHECK(Coord3_length(nullptr) == 0.0f);

    Coord3_cross(nullptr, nullptr, nullptr);

    Coord3_normalize(nullptr, nullptr);

    Coord3_fastNormalize(nullptr, nullptr);

    CHECK(Coord3_distance(nullptr, nullptr) == 0.0f);

    CHECK(Coord3_angle(nullptr, nullptr) == 0.0f);

    Coord3_project(nullptr, nullptr, nullptr);

    Coord3_reflect(nullptr, nullptr, nullptr);

    Coord3_clamp(nullptr, 0, 1, &d);

    Coord3_abs(nullptr, &d);

    Coord3_lerp(nullptr, nullptr, 0.5f, nullptr);

    Coord3_toFrame(nullptr, COORD_FRAME_DEFAULT, nullptr);
}

// Runs Coord3 owner cases after initializing the memory substrate.
int main(void) {
    CHECK(Memory_init(0));

    test_construction();

    test_accessors_and_frame();

    test_arithmetic();

    test_geometry();

    test_to_frame();

    test_nulls();

    if (g_failures == 0) {
        printf("vec3_test: all assertions held\n");

        return 0;
    }

    printf("vec3_test: %d FAILURES\n", g_failures);

    return 1;
}
