// vexspoke/test/lang/vec4_test.c — owner test for lang/vec4 (and vec4/vec4).
//
// Proves the 4D homogeneous vector:
//   - zero/4-arg construction, nullptr getters/setters, free;
//   - directional aliases (right/left, up/down, front/back, x/y/z/w) and exact
//     inverse signs; directional setters leave w untouched;
//   - dest-last arithmetic with aliasing, div-by-zero refusing to write;
//   - dot/length include w; normalize handles zero; lerp interpolates w too;
//   - frame-mapped accessors using the CoordFrame axis tables.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#include "lang/vec4.h"
#include "lang/vec4/vec4d.h"
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

// Checks zero/value constructors and null-safe freeing.
static void test_construction(void) {
    Vec4 *z = Vec4();
    CHECK(z != nullptr);
    CHECK(Vec4_getX(z) == 0.0f && Vec4_getY(z) == 0.0f && Vec4_getZ(z) == 0.0f && Vec4_getW(z) == 0.0f);
    Vec4_free(z);

    Vec4 *v = Vec4(1.0f, 2.0f, 3.0f, 4.0f);
    CHECK(v != nullptr);
    CHECK(Vec4_getRight(v) == 1.0f && Vec4_getUp(v) == 2.0f &&
          Vec4_getFront(v) == 3.0f && Vec4_getW(v) == 4.0f);
    Vec4_free(v);
    Vec4_free(nullptr);

    Vec4d *wide = Vec4d();
    CHECK(wide != nullptr && (*wide).w == 0.0);
    Vec4d_free(wide);
    wide = Vec4d(1.25, -2.5, 3.75, 4.5);
    CHECK(wide != nullptr && (*wide).horizontal == 1.25 && (*wide).w == 4.5);
    Vec4d_free(wide);
}

// Checks directional aliases, frame mapping, setters, and copy behavior.
static void test_accessors(void) {
    Vec4 v = { 0 };
    Vec4_set(&v, 1.0f, 2.0f, 3.0f, 4.0f);
    CHECK(Vec4_getRight(&v) == 1.0f && Vec4_getLeft(&v) == -1.0f);
    CHECK(Vec4_getUp(&v) == 2.0f && Vec4_getDown(&v) == -2.0f);
    CHECK(Vec4_getFront(&v) == 3.0f && Vec4_getBack(&v) == -3.0f);

    // Directional setters must NOT disturb w.
    Vec4_setLeft(&v, 9.0f);
    Vec4_setDown(&v, 9.0f);
    Vec4_setBack(&v, 9.0f);
    CHECK(Vec4_getRight(&v) == -9.0f && Vec4_getUp(&v) == -9.0f && Vec4_getFront(&v) == -9.0f);
    CHECK(Vec4_getW(&v) == 4.0f);

    Vec4_setX(&v, 5.0f);
    Vec4_setY(&v, 6.0f);
    Vec4_setZ(&v, 7.0f);
    Vec4_setW(&v, 8.0f);
    CHECK(Vec4_getX(&v) == 5.0f && Vec4_getY(&v) == 6.0f && Vec4_getZ(&v) == 7.0f && Vec4_getW(&v) == 8.0f);

    // Frame-mapped accessors: Y_DOWN flips vertical.
    CHECK(Vec4_getXInFrame(&v, COORD_FRAME_Y_UP_LEFT) == 5.0f);
    CHECK(Vec4_getYInFrame(&v, COORD_FRAME_Y_DOWN_LEFT) == -6.0f);
    CHECK(Vec4_getZInFrame(&v, COORD_FRAME_Y_UP_LEFT) == 7.0f);

    Vec4 c = { 0 };
    Vec4_copy(&v, &c);
    CHECK(Vec4_getX(&c) == 5.0f && Vec4_getW(&c) == 8.0f);
}

// Checks four-component arithmetic, aliasing, division rejection, and nulls.
static void test_arithmetic(void) {
    Vec4 a = { 0 }, b = { 0 }, d = { 0 };
    Vec4_set(&a, 1.0f, 2.0f, 3.0f, 4.0f);
    Vec4_set(&b, 5.0f, 6.0f, 7.0f, 8.0f);

    Vec4_add(&a, &b, &d);
    CHECK(Vec4_getX(&d) == 6.0f && Vec4_getW(&d) == 12.0f);
    Vec4_sub(&b, &a, &d);
    CHECK(Vec4_getX(&d) == 4.0f && Vec4_getZ(&d) == 4.0f && Vec4_getW(&d) == 4.0f);
    Vec4_mul(&a, 2.0f, &d);
    CHECK(Vec4_getX(&d) == 2.0f && Vec4_getW(&d) == 8.0f);
    Vec4_div(&a, 2.0f, &d);
    CHECK(Vec4_getX(&d) == 0.5f && Vec4_getZ(&d) == 1.5f);

    Vec4 keep = { 0 };
    Vec4_set(&keep, 9, 9, 9, 9);
    Vec4_div(&a, 0.0f, &keep);
    CHECK(Vec4_getW(&keep) == 9.0f);

    // Aliasing.
    Vec4 ali = { 0 };
    Vec4_set(&ali, 1, 1, 1, 1);
    Vec4_add(&ali, &b, &ali);
    CHECK(Vec4_getX(&ali) == 6.0f && Vec4_getW(&ali) == 9.0f);

    Vec4_add(nullptr, &b, &d);
    Vec4_mul(nullptr, 1.0f, &d);
}

// Checks four-dimensional dot/length, normalization, and interpolation.
static void test_geometry(void) {
    Vec4 a = { 0 }, b = { 0 }, d = { 0 };
    Vec4_set(&a, 1.0f, 2.0f, 2.0f, 0.0f);
    Vec4_set(&b, 1.0f, 0.0f, 0.0f, 0.0f);

    CHECK(Vec4_dot(&a, &b) == 1.0f);
    CHECK(Vec4_lengthSquared(&a) == 9.0f);
    CHECK(CLOSE(Vec4_length(&a), 3.0f));
    // w participates in length.
    Vec4 wv = { 0 };
    Vec4_set(&wv, 0.0f, 0.0f, 0.0f, 4.0f);
    CHECK(Vec4_lengthSquared(&wv) == 16.0f);

    Vec4_normalize(&a, &d);
    CHECK(CLOSE(Vec4_length(&d), 1.0f));
    CHECK(CLOSE(Vec4_getX(&d), 1.0f / 3.0f));
    Vec4 zero = { 0 };
    Vec4_normalize(&zero, &d);
    CHECK(Vec4_lengthSquared(&d) == 0.0f);

    Vec4_lerp(&a, &b, 0.5f, &d);
    CHECK(CLOSE(Vec4_getX(&d), 1.0f));
    CHECK(CLOSE(Vec4_getY(&d), 1.0f));
    CHECK(CLOSE(Vec4_getW(&d), 0.0f));
    Vec4_lerp(&a, &b, 1.0f, &d);
    CHECK(Vec4_getZ(&d) == 0.0f);
}

// Checks safe defaults and no-op behavior for null Vec4 arguments.
static void test_nulls(void) {
    CHECK(Vec4_getRight(nullptr) == 0.0f);
    CHECK(Vec4_getLeft(nullptr) == 0.0f);
    CHECK(Vec4_getUp(nullptr) == 0.0f);
    CHECK(Vec4_getFront(nullptr) == 0.0f);
    CHECK(Vec4_getX(nullptr) == 0.0f);
    CHECK(Vec4_getW(nullptr) == 0.0f);
    CHECK(Vec4_getXInFrame(nullptr, COORD_FRAME_DEFAULT) == 0.0f);
    Vec4_set(nullptr, 1, 2, 3, 4);
    Vec4_setW(nullptr, 1.0f);
    Vec4_copy(nullptr, nullptr);
    CHECK(Vec4_dot(nullptr, nullptr) == 0.0f);
    CHECK(Vec4_length(nullptr) == 0.0f);
    Vec4_normalize(nullptr, nullptr);
    Vec4_lerp(nullptr, nullptr, 0.5f, nullptr);
}

// Runs Vec4 owner cases after initializing the memory substrate.
int main(void) {
    CHECK(Memory_init(0));
    test_construction();
    test_accessors();
    test_arithmetic();
    test_geometry();
    test_nulls();

    if (g_failures == 0) {
        printf("vec4_test: all assertions held\n");
        return 0;
    }
    printf("vec4_test: %d FAILURES\n", g_failures);
    return 1;
}
