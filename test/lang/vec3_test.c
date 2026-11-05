// vexspoke/test/lang/vec3_test.c — owner test for lang/vec3 (and vec3/vec3).
//
// Proves the frame-carrying 3D vector:
//   - zero/3-arg/4-arg construction; an invalid frame in Vec3_4 falls back to
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
#include "lang/vec3/vec3_int_float.h"
#include "lang/vec3/vec3_long_double.h"
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
    Vec3 *z = Vec3();

    CHECK(z != nullptr);

    CHECK(Vec3_getX(z) == 0.0f && Vec3_getY(z) == 0.0f && Vec3_getZ(z) == 0.0f);

    CHECK(Vec3_getFrame(z) == COORD_FRAME_DEFAULT);

    Vec3_free(z);

    Vec3 *v = Vec3(1.0f, 2.0f, 3.0f);

    CHECK(v != nullptr);

    CHECK(Vec3_getRight(v) == 1.0f && Vec3_getUp(v) == 2.0f && Vec3_getFront(v) == 3.0f);

    CHECK(Vec3_getFrame(v) == COORD_FRAME_DEFAULT);

    Vec3 *f = Vec3(4.0f, 5.0f, 6.0f, COORD_FRAME_Z_UP_RIGHT);

    CHECK(Vec3_getFrame(f) == COORD_FRAME_Z_UP_RIGHT);
    // Invalid frame is rejected at construction and replaced by the default.
    Vec3 *bad = Vec3(1.0f, 1.0f, 1.0f, (CoordFrame) 999);

    CHECK(Vec3_getFrame(bad) == COORD_FRAME_DEFAULT);

    Vec3_free(v);

    Vec3_free(f);

    Vec3_free(bad);

    Vec3_free(nullptr);

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

    Vec3LongDouble *sectorDouble = Vec3LongDouble();

    CHECK(sectorDouble != nullptr);

    Vec3LongDouble zeroDouble = *sectorDouble;

    CHECK(zeroDouble.horizontal.sector == 0);

    Vec3LongDouble_free(sectorDouble);

    sectorDouble = Vec3LongDouble(1, 0.25, 2, 0.5, 3, 0.75, COORD_FRAME_DEFAULT);

    CHECK(sectorDouble != nullptr);

    Vec3LongDouble doubleValue = *sectorDouble;

    CHECK(doubleValue.depth.sector == 3 && doubleValue.depth.local == 0.75);

    Vec3LongDouble_free(sectorDouble);
}

// Checks directional aliases, frame-aware accessors, and frame-preserving copy.
static void test_accessors_and_frame(void) {
    Vec3 v = { 0 };

    Vec3_set(&v, 1.0f, 2.0f, 3.0f);

    CHECK(Vec3_getRight(&v) == 1.0f && Vec3_getLeft(&v) == -1.0f);

    CHECK(Vec3_getUp(&v) == 2.0f && Vec3_getDown(&v) == -2.0f);

    CHECK(Vec3_getFront(&v) == 3.0f && Vec3_getBack(&v) == -3.0f);

    Vec3_setLeft(&v, 5.0f);

    CHECK(Vec3_getRight(&v) == -5.0f && Vec3_getLeft(&v) == 5.0f);

    Vec3_setDown(&v, 5.0f);

    CHECK(Vec3_getUp(&v) == -5.0f && Vec3_getDown(&v) == 5.0f);

    Vec3_setBack(&v, 5.0f);

    CHECK(Vec3_getFront(&v) == -5.0f && Vec3_getBack(&v) == 5.0f);

    Vec3_setX(&v, 7.0f);

    CHECK(Vec3_getX(&v) == 7.0f);

    // Default frame maps X/Y/Z straight to horizontal/vertical/depth.
    Vec3_set(&v, 10.0f, 20.0f, 30.0f);

    Vec3_setFrame(&v, COORD_FRAME_Y_DOWN_LEFT);         // flips Y

    CHECK(Vec3_getFrame(&v) == COORD_FRAME_Y_DOWN_LEFT);

    CHECK(Vec3_getX(&v) == 10.0f);

    CHECK(Vec3_getY(&v) == -20.0f);                     // sign-mapped

    CHECK(Vec3_getZ(&v) == 30.0f);

    CHECK(Vec3_getYInFrame(&v, COORD_FRAME_Y_UP_LEFT) == 20.0f);

    // Invalid frame set is ignored (keeps the current frame).
    Vec3_setFrame(&v, (CoordFrame) 42);

    CHECK(Vec3_getFrame(&v) == COORD_FRAME_Y_DOWN_LEFT);

    // Copy carries the frame.
    Vec3 c = { 0 };

    Vec3_copy(&v, &c);

    CHECK(Vec3_getFrame(&c) == Vec3_getFrame(&v));

    CHECK(Vec3_getRight(&c) == Vec3_getRight(&v));
}

// Checks arithmetic, frame propagation, division rejection, and null guards.
static void test_arithmetic(void) {
    Vec3 a = { 0 }, b = { 0 }, d = { 0 };

    Vec3_set(&a, 3.0f, -4.0f, 2.0f);

    Vec3_set(&b, 1.0f, 2.0f, -3.0f);

    Vec3_add(&a, &b, &d);

    CHECK(Vec3_getX(&d) == 4.0f && Vec3_getY(&d) == -2.0f && Vec3_getZ(&d) == -1.0f);

    Vec3_sub(&a, &b, &d);

    CHECK(Vec3_getX(&d) == 2.0f && Vec3_getY(&d) == -6.0f && Vec3_getZ(&d) == 5.0f);

    Vec3_mul(&a, 2.0f, &d);

    CHECK(Vec3_getX(&d) == 6.0f && Vec3_getY(&d) == -8.0f && Vec3_getZ(&d) == 4.0f);

    Vec3_div(&a, 2.0f, &d);

    CHECK(Vec3_getX(&d) == 1.5f && Vec3_getY(&d) == -2.0f && Vec3_getZ(&d) == 1.0f);

    Vec3 keep = { 0 };

    Vec3_set(&keep, 9.0f, 9.0f, 9.0f);

    Vec3_div(&a, 0.0f, &keep);

    CHECK(Vec3_getZ(&keep) == 9.0f);

    // Frame metadata is carried through arithmetic.
    Vec3 framed = { 0 };

    Vec3_set(&framed, 1.0f, 1.0f, 1.0f);

    Vec3_setFrame(&framed, COORD_FRAME_Z_UP_LEFT);

    Vec3_add(&framed, &framed, &d);

    CHECK(Vec3_getFrame(&d) == COORD_FRAME_Z_UP_LEFT);

    Vec3_add(nullptr, &b, &d);

    Vec3_add(&a, nullptr, &d);

    Vec3_add(&a, &b, nullptr);

    Vec3_mul(nullptr, 2.0f, &d);
}

// Checks vector products, normalization, projection, reflection, and geometry.
static void test_geometry(void) {
    Vec3 a = { 0 }, b = { 0 }, d = { 0 };

    Vec3_set(&a, 3.0f, 4.0f, 0.0f);

    Vec3_set(&b, 1.0f, 0.0f, 0.0f);

    CHECK(Vec3_dot(&a, &b) == 3.0f);

    CHECK(Vec3_lengthSquared(&a) == 25.0f);

    CHECK(CLOSE(Vec3_length(&a), 5.0f));

    // Right-handed cross identities.
    Vec3 ex = { 0 }, ey = { 0 }, ez = { 0 };

    Vec3_set(&ex, 1.0f, 0.0f, 0.0f);

    Vec3_set(&ey, 0.0f, 1.0f, 0.0f);

    Vec3_set(&ez, 0.0f, 0.0f, 1.0f);

    Vec3_cross(&ex, &ey, &d);

    CHECK(Vec3_getX(&d) == 0.0f && Vec3_getY(&d) == 0.0f && Vec3_getZ(&d) == 1.0f);

    Vec3_cross(&ey, &ez, &d);

    CHECK(Vec3_getX(&d) == 1.0f);

    Vec3_cross(&ez, &ex, &d);

    CHECK(Vec3_getY(&d) == 1.0f);

    Vec3_cross(&b, &b, &d);

    CHECK(Vec3_lengthSquared(&d) == 0.0f);

    // Normalize.
    Vec3_normalize(&a, &d);

    CHECK(CLOSE(Vec3_length(&d), 1.0f));

    CHECK(CLOSE(Vec3_getX(&d), 0.6f) && CLOSE(Vec3_getY(&d), 0.8f));

    Vec3_fastNormalize(&a, &d);

    CHECK(fabsf(Vec3_length(&d) - 1.0f) <= 0.01f);
    // Zero-length normalization yields zero (both strict and fast).
    Vec3 zero = { 0 };

    Vec3_normalize(&zero, &d);

    CHECK(Vec3_lengthSquared(&d) == 0.0f);

    Vec3_fastNormalize(&zero, &d);

    CHECK(Vec3_lengthSquared(&d) == 0.0f);

    // Distance.
    CHECK(CLOSE(Vec3_distance(&ex, &ey), sqrtf(2.0f)));

    // Angle.
    CHECK(CLOSE(Vec3_angle(&ex, &ex), 0.0f));

    CHECK(CLOSE(Vec3_angle(&ex, &ey), 1.5707963f));

    CHECK(CLOSE(Vec3_angle(&ex, &(Vec3){ .data = { -1.0f, 0.0f, 0.0f, 0.0f } }), 3.1415927f));

    CHECK(Vec3_angle(&zero, &ex) == 0.0f);

    // Projection: project (3,4,0) onto (1,0,0) -> (3,0,0).
    Vec3_project(&a, &b, &d);

    CHECK(CLOSE(Vec3_getX(&d), 3.0f) && CLOSE(Vec3_getY(&d), 0.0f));
    // Projecting onto a zero vector yields zero.
    Vec3_project(&a, &zero, &d);

    CHECK(Vec3_lengthSquared(&d) == 0.0f);

    // Reflection of (1,-1,0) off normal (0,1,0) -> (1,1,0).
    Vec3 inc = { 0 }, nrm = { 0 };

    Vec3_set(&inc, 1.0f, -1.0f, 0.0f);

    Vec3_set(&nrm, 0.0f, 1.0f, 0.0f);

    Vec3_reflect(&inc, &nrm, &d);

    CHECK(CLOSE(Vec3_getX(&d), 1.0f) && CLOSE(Vec3_getY(&d), 1.0f));

    // Clamp and abs.
    Vec3 wild = { 0 };

    Vec3_set(&wild, -9.0f, 5.0f, 0.5f);

    Vec3_clamp(&wild, -1.0f, 1.0f, &d);

    CHECK(Vec3_getX(&d) == -1.0f && Vec3_getY(&d) == 1.0f && Vec3_getZ(&d) == 0.5f);

    Vec3_abs(&wild, &d);

    CHECK(Vec3_getX(&d) == 9.0f && Vec3_getY(&d) == 5.0f);

    // Lerp endpoints + midpoint.
    Vec3_lerp(&ex, &ey, 0.5f, &d);

    CHECK(CLOSE(Vec3_getX(&d), 0.5f) && CLOSE(Vec3_getY(&d), 0.5f));

    Vec3_lerp(&ex, &ey, 0.0f, &d);

    CHECK(Vec3_getX(&d) == 1.0f && Vec3_getY(&d) == 0.0f);
}

// Checks coordinate-frame conversion and invalid-target fallback.
static void test_to_frame(void) {
    Vec3 src = { 0 };

    Vec3_set(&src, 1.0f, 2.0f, 3.0f);

    Vec3 dst = { 0 };

    Vec3_toFrame(&src, COORD_FRAME_X_UP_RIGHT, &dst);

    CHECK(Vec3_getFrame(&dst) == COORD_FRAME_X_UP_RIGHT);

    CHECK(Vec3_getRight(&dst) == 1.0f && Vec3_getUp(&dst) == 2.0f && Vec3_getFront(&dst) == 3.0f);
    // Invalid target falls back to the default.
    Vec3_toFrame(&src, (CoordFrame) 77, &dst);

    CHECK(Vec3_getFrame(&dst) == COORD_FRAME_DEFAULT);
}

// Checks safe defaults and no-op behavior for null Vec3 arguments.
static void test_nulls(void) {
    Vec3 d = { 0 };

    CHECK(Vec3_getRight(nullptr) == 0.0f);

    CHECK(Vec3_getLeft(nullptr) == 0.0f);

    CHECK(Vec3_getUp(nullptr) == 0.0f);

    CHECK(Vec3_getFront(nullptr) == 0.0f);

    CHECK(Vec3_getX(nullptr) == 0.0f);

    CHECK(Vec3_getFrame(nullptr) == COORD_FRAME_DEFAULT);

    Vec3_set(nullptr, 1, 2, 3);

    Vec3_setFrame(nullptr, COORD_FRAME_Y_UP_LEFT);

    Vec3_copy(nullptr, nullptr);

    CHECK(Vec3_dot(nullptr, nullptr) == 0.0f);

    CHECK(Vec3_length(nullptr) == 0.0f);

    Vec3_cross(nullptr, nullptr, nullptr);

    Vec3_normalize(nullptr, nullptr);

    Vec3_fastNormalize(nullptr, nullptr);

    CHECK(Vec3_distance(nullptr, nullptr) == 0.0f);

    CHECK(Vec3_angle(nullptr, nullptr) == 0.0f);

    Vec3_project(nullptr, nullptr, nullptr);

    Vec3_reflect(nullptr, nullptr, nullptr);

    Vec3_clamp(nullptr, 0, 1, &d);

    Vec3_abs(nullptr, &d);

    Vec3_lerp(nullptr, nullptr, 0.5f, nullptr);

    Vec3_toFrame(nullptr, COORD_FRAME_DEFAULT, nullptr);
}

// Runs Vec3 owner cases after initializing the memory substrate.
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
