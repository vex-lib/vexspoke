// vexspoke/test/lang/vec2_test.c — owner test for lang/vec2 (and vec2/vec2).
//
// Proves the 2D vector family:
//   - zero/2-arg construction, nullptr getters/setters, free;
//   - directional aliases (right/left, up/down, x/y) and their exact inverse
//     signs; setLeft/setDown store the negated value;
//   - frame-aware Y (Y_UP vs Y_DOWN);
//   - dest-last arithmetic with aliasing, and div-by-zero refusing to write;
//   - dot/length/normalize (including the zero-length guard), perpendicular,
//     distance, angle (parallel/orthogonal/opposite), and lerp.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#include "lang/vec2.h"
#include "lang/vec2/vec2d.h"
#include "nio/mem.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

#define CLOSE(a, b) (fabsf((a) - (b)) <= 1e-5f)

// Checks zero/value constructors, directional aliases, and free behavior.
static void test_construction(void) {
    Coord2 *z = Coord2();
    CHECK(z != nullptr);
    CHECK(Coord2_getX(z) == 0.0f && Coord2_getY(z) == 0.0f);
    Coord2_free(z);

    Coord2 *v = Coord2(3.0f, -4.0f);
    CHECK(v != nullptr);
    CHECK(Coord2_getRight(v) == 3.0f);
    CHECK(Coord2_getUp(v) == -4.0f);
    CHECK(Coord2_getLeft(v) == -3.0f);
    CHECK(Coord2_getDown(v) == 4.0f);
    Coord2_free(v);
    Coord2_free(nullptr);

    Vec2d *wide = Vec2d();
    CHECK(wide != nullptr && (*wide).horizontal == 0.0 && (*wide).vertical == 0.0);
    Vec2d_free(wide);
    wide = Vec2d(1.25, -2.5);
    CHECK(wide != nullptr && (*wide).horizontal == 1.25 && (*wide).vertical == -2.5);
    Vec2d_free(wide);
}

// Checks setters, coordinate aliases, frame mapping, and copy behavior.
static void test_accessors(void) {
    Coord2 v = { 0 };
    Coord2_set(&v, 1.0f, 2.0f);
    CHECK(Coord2_getRight(&v) == 1.0f && Coord2_getUp(&v) == 2.0f);

    Coord2_setRight(&v, 5.0f);
    CHECK(Coord2_getRight(&v) == 5.0f && Coord2_getX(&v) == 5.0f);
    Coord2_setLeft(&v, 5.0f);
    CHECK(Coord2_getRight(&v) == -5.0f && Coord2_getLeft(&v) == 5.0f);
    Coord2_setUp(&v, 7.0f);
    CHECK(Coord2_getUp(&v) == 7.0f && Coord2_getY(&v) == 7.0f);
    Coord2_setDown(&v, 7.0f);
    CHECK(Coord2_getUp(&v) == -7.0f && Coord2_getDown(&v) == 7.0f);

    Coord2_setY(&v, 3.0f);
    CHECK(Coord2_getY(&v) == 3.0f);

    // Frame-aware Y.
    CHECK(Coord2_getYInFrame(&v, COORD_FRAME_2D_Y_UP) == 3.0f);
    CHECK(Coord2_getYInFrame(&v, COORD_FRAME_2D_Y_DOWN) == -3.0f);

    // Copy.
    Coord2 c = { 0 };
    Coord2_copy(&v, &c);
    CHECK(Coord2_getX(&c) == Coord2_getX(&v) && Coord2_getY(&c) == Coord2_getY(&v));
}

// Checks vector arithmetic, aliasing, division rejection, and null guards.
static void test_arithmetic(void) {
    Coord2 a = { 0 }, b = { 0 }, d = { 0 };
    Coord2_set(&a, 3.0f, 4.0f);
    Coord2_set(&b, 1.0f, -2.0f);

    Coord2_add(&a, &b, &d);
    CHECK(Coord2_getX(&d) == 4.0f && Coord2_getY(&d) == 2.0f);
    Coord2_sub(&a, &b, &d);
    CHECK(Coord2_getX(&d) == 2.0f && Coord2_getY(&d) == 6.0f);
    Coord2_mul(&a, 2.0f, &d);
    CHECK(Coord2_getX(&d) == 6.0f && Coord2_getY(&d) == 8.0f);
    Coord2_div(&a, 2.0f, &d);
    CHECK(Coord2_getX(&d) == 1.5f && Coord2_getY(&d) == 2.0f);

    // Aliasing.
    Coord2 ali = { 0 };
    Coord2_set(&ali, 1.0f, 1.0f);
    Coord2_add(&ali, &b, &ali);
    CHECK(Coord2_getX(&ali) == 2.0f && Coord2_getY(&ali) == -1.0f);

    // div by zero refuses to write.
    Coord2 keep = { 0 };
    Coord2_set(&keep, 9.0f, 9.0f);
    Coord2_div(&a, 0.0f, &keep);
    CHECK(Coord2_getX(&keep) == 9.0f && Coord2_getY(&keep) == 9.0f);

    // Null guards.
    Coord2_add(nullptr, &b, &d);
    Coord2_add(&a, nullptr, &d);
    Coord2_add(&a, &b, nullptr);
    Coord2_mul(nullptr, 2.0f, &d);
    Coord2_div(&a, 2.0f, nullptr);
}

// Checks dot/length, normalization, perpendicular, angles, distance, and lerp.
static void test_geometry(void) {
    Coord2 a = { 0 }, b = { 0 }, d = { 0 };
    Coord2_set(&a, 3.0f, 4.0f);
    Coord2_set(&b, -6.0f, 8.0f);

    CHECK(Coord2_dot(&a, &b) == 14.0f);          // -18 + 32
    CHECK(Coord2_lengthSquared(&a) == 25.0f);
    CHECK(CLOSE(Coord2_length(&a), 5.0f));
    CHECK(CLOSE(Coord2_distance(&a, &b), sqrtf(9.0f * 9.0f + 4.0f * 4.0f)));

    // Normalize: unit length, direction preserved.
    Coord2_normalize(&a, &d);
    CHECK(CLOSE(Coord2_length(&d), 1.0f));
    CHECK(CLOSE(Coord2_getX(&d), 0.6f) && CLOSE(Coord2_getY(&d), 0.8f));
    // Zero-length normalization yields zero, not NaN.
    Coord2 zero = { 0 };
    Coord2_normalize(&zero, &d);
    CHECK(Coord2_getX(&d) == 0.0f && Coord2_getY(&d) == 0.0f);

    // Perpendicular is a 90-degree rotation.
    Coord2_perpendicular(&a, &d);
    CHECK(Coord2_getX(&d) == -4.0f && Coord2_getY(&d) == 3.0f);
    CHECK(CLOSE(Coord2_dot(&a, &d), 0.0f));

    // Angles.
    Coord2 ex = { 0 }, ey = { 0 }, negx = { 0 };
    Coord2_set(&ex, 1.0f, 0.0f);
    Coord2_set(&ey, 0.0f, 1.0f);
    Coord2_set(&negx, -1.0f, 0.0f);
    CHECK(CLOSE(Coord2_angle(&ex, &ex), 0.0f));
    CHECK(CLOSE(Coord2_angle(&ex, &ey), 1.5707963f));
    CHECK(CLOSE(Coord2_angle(&ex, &negx), 3.1415927f));
    CHECK(Coord2_angle(&zero, &ex) == 0.0f);      // zero-length guard

    // Lerp.
    Coord2_lerp(&a, &b, 0.5f, &d);
    CHECK(CLOSE(Coord2_getX(&d), -1.5f) && CLOSE(Coord2_getY(&d), 6.0f));
    Coord2_lerp(&a, &b, 0.0f, &d);
    CHECK(Coord2_getX(&d) == 3.0f && Coord2_getY(&d) == 4.0f);
    Coord2_lerp(&a, &b, 1.0f, &d);
    CHECK(Coord2_getX(&d) == -6.0f && Coord2_getY(&d) == 8.0f);
}

// Checks safe defaults and no-op behavior for null vector arguments.
static void test_nulls(void) {
    CHECK(Coord2_getRight(nullptr) == 0.0f);
    CHECK(Coord2_getLeft(nullptr) == 0.0f);
    CHECK(Coord2_getUp(nullptr) == 0.0f);
    CHECK(Coord2_getDown(nullptr) == 0.0f);
    CHECK(Coord2_getX(nullptr) == 0.0f);
    CHECK(Coord2_getY(nullptr) == 0.0f);
    CHECK(Coord2_getYInFrame(nullptr, COORD_FRAME_2D_Y_UP) == 0.0f);
    Coord2_set(nullptr, 1.0f, 2.0f);
    Coord2_setRight(nullptr, 1.0f);
    Coord2_copy(nullptr, nullptr);
    CHECK(Coord2_dot(nullptr, nullptr) == 0.0f);
    CHECK(Coord2_lengthSquared(nullptr) == 0.0f);
    CHECK(Coord2_length(nullptr) == 0.0f);
    Coord2_normalize(nullptr, nullptr);
    Coord2_perpendicular(nullptr, nullptr);
    CHECK(Coord2_distance(nullptr, nullptr) == 0.0f);
    CHECK(Coord2_angle(nullptr, nullptr) == 0.0f);
    Coord2_lerp(nullptr, nullptr, 0.5f, nullptr);
}

// Runs Coord2 owner cases after initializing the memory substrate.
int main(void) {
    CHECK(Memory_init(0));
    test_construction();
    test_accessors();
    test_arithmetic();
    test_geometry();
    test_nulls();

    if (g_failures == 0) {
        printf("vec2_test: all assertions held\n");
        return 0;
    }
    printf("vec2_test: %d FAILURES\n", g_failures);
    return 1;
}
