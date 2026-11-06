// vexspoke/test/lang/mat4_test.c — owner test for lang/mat4.
//
// Proves the column-major 4x4 matrix:
//   - Mat4_0 / Mat4_identityAlloc give identity; free is null-safe;
//   - column-major layout: set(row,col) == raw[col*4+row]; zero/identity/copy;
//   - multiply by identity is neutral, and dest-aliasing left/right is safe;
//   - transpose is involutive and in-place-safe;
//   - TRS synthesis: identity args, translation, 2D TRS;
//   - transform / transformVec3 (translation moves a point; w preserved);
//   - rotate about Z by 90 degrees on a basis vector;
//   - perspective, perspectiveVulkan, and orthographic depth mappings at the
//     near/far planes;
//   - lookAt eye-at-origin mapping.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#include "lang/mat4.h"
#include "lang/vec3.h"
#include "lang/vec4.h"
#include "nio/mem.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

#define CLOSE(a, b) (fabsf((a) - (b)) <= 1e-3f)

// A known, documented production gap: printed, never counted as a pass.
#define GAP(cond, msg)                                                     \
    do {                                                                   \
        if (!(cond))                                                       \
            printf("KNOWN GAP (unproved): %s\n", msg);                     \
    } while (0)

// Returns whether every matrix element matches the identity within tolerance.
static bool is_identity(const Mat4 *m) {
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (fabsf(Mat4_get(m, r, c) - (r == c ? 1.0f : 0.0f)) > 1e-5f)
                return false;
    return true;
}

// Compares all raw matrix entries within the test's numeric tolerance.
static bool mat_equal(const Mat4 *a, const Mat4 *b) {
    for (int i = 0; i < 16; i++)
        if (fabsf(Mat4_getRaw(a, i) - Mat4_getRaw(b, i)) > 1e-3f)
            return false;
    return true;
}

// Checks allocated and in-place identity/zero construction plus null free.
static void test_construction(void) {
    Mat4 *m = Mat4_0();
    CHECK(m != nullptr);
    CHECK(is_identity(m));
    Mat4 *m2 = Mat4_identityAlloc();
    CHECK(m2 != nullptr && is_identity(m2));
    Mat4_free(m);
    Mat4_free(m2);
    Mat4_free(nullptr);

    Mat4 z;
    Mat4_zero(&z);
    for (int i = 0; i < 16; i++)
        CHECK(Mat4_getRaw(&z, i) == 0.0f);
}

// Checks column-major indexing, identity, and copying.
static void test_layout(void) {
    Mat4 m;
    Mat4_zero(&m);
    Mat4_set(&m, 1, 2, 7.0f);
    CHECK(Mat4_getRaw(&m, 2 * 4 + 1) == 7.0f);
    CHECK(Mat4_get(&m, 1, 2) == 7.0f);
    Mat4_setRaw(&m, 5, 3.0f);
    CHECK(Mat4_get(&m, 1, 1) == 3.0f);

    Mat4 i;
    Mat4_identity(&i);
    CHECK(is_identity(&i));

    Mat4 c;
    Mat4_copy(&m, &c);
    CHECK(mat_equal(&m, &c));
}

// Checks multiplication identities, composition, and destination aliasing.
static void test_multiply(void) {
    Mat4 a;
    Mat4_createTransformationMatrix(1, 2, 3, 0, 0, 0, 2, 2, 2, &a);   // T * S
    Mat4 ident;
    Mat4_identity(&ident);

    Mat4 d;
    Mat4_multiply(&ident, &a, &d);
    CHECK(mat_equal(&d, &a));
    Mat4_multiply(&a, &ident, &d);
    CHECK(mat_equal(&d, &a));

    // Two translations compose.
    Mat4 t1, t2;
    Mat4_createTransformationMatrix(1, 0, 0, 0, 0, 0, 1, 1, 1, &t1);
    Mat4_createTransformationMatrix(2, 0, 0, 0, 0, 0, 1, 1, 1, &t2);
    Mat4_multiply(&t1, &t2, &d);
    CHECK(CLOSE(Mat4_get(&d, 0, 3), 3.0f));

    // Aliasing dest==left and dest==right.
    Mat4 l = t1, r = t2, expect;
    Mat4_multiply(&t1, &t2, &expect);
    Mat4_multiply(&l, &r, &l);
    CHECK(mat_equal(&l, &expect));
    Mat4 l2 = t1, r2 = t2;
    Mat4_multiply(&l2, &r2, &r2);
    CHECK(mat_equal(&r2, &expect));
}

// Checks transpose values, involution, and in-place operation.
static void test_transpose(void) {
    Mat4 a;
    Mat4_set(&a, 0, 1, 4.0f);
    Mat4_set(&a, 1, 0, 9.0f);
    Mat4_set(&a, 2, 3, 5.0f);

    Mat4 t, tt;
    Mat4_transpose(&a, &t);
    CHECK(Mat4_get(&t, 1, 0) == 4.0f);
    CHECK(Mat4_get(&t, 0, 1) == 9.0f);
    CHECK(Mat4_get(&t, 3, 2) == 5.0f);
    Mat4_transpose(&t, &tt);
    CHECK(mat_equal(&a, &tt));

    // In-place.
    Mat4 ip = a;
    Mat4_transpose(&ip, &ip);
    CHECK(mat_equal(&ip, &t));
}

// Checks TRS construction and homogeneous/vector transform results.
static void test_transforms(void) {
    // Identity arguments -> identity.
    Mat4 trs;
    Mat4_createTransformationMatrix(0, 0, 0, 0, 0, 0, 1, 1, 1, &trs);
    CHECK(is_identity(&trs));

    // Pure translation goes into the last column.
    Mat4 trans;
    Mat4_createTransformationMatrix(10, 20, 30, 0, 0, 0, 1, 1, 1, &trans);
    CHECK(Mat4_get(&trans, 0, 3) == 10.0f);
    CHECK(Mat4_get(&trans, 1, 3) == 20.0f);
    CHECK(Mat4_get(&trans, 2, 3) == 30.0f);
    CHECK(Mat4_get(&trans, 3, 3) == 1.0f);

    // Transform a homogeneous point.
    Coord4 p = { 0 }, out = { 0 };
    Coord4_set(&p, 1.0f, 1.0f, 1.0f, 1.0f);
    Mat4_transform(&trans, &p, &out);
    CHECK(CLOSE(Coord4_getX(&out), 11.0f));
    CHECK(CLOSE(Coord4_getY(&out), 21.0f));
    CHECK(CLOSE(Coord4_getZ(&out), 31.0f));
    CHECK(CLOSE(Coord4_getW(&out), 1.0f));

    // transformVec3 (implicit w=1).
    Coord3 p3 = { 0 }, o3 = { 0 };
    Coord3_set(&p3, 1.0f, 1.0f, 1.0f);
    Mat4_transformVec3(&trans, &p3, &o3);
    CHECK(CLOSE(Coord3_getX(&o3), 11.0f) && CLOSE(Coord3_getZ(&o3), 31.0f));

    // 2D TRS identity.
    Mat4 t2d;
    Mat4_createTransformationMatrix2D(0, 0, 0, 1, 1, &t2d);
    CHECK(CLOSE(Mat4_get(&t2d, 0, 0), 1.0f));
    CHECK(CLOSE(Mat4_get(&t2d, 1, 1), 1.0f));
    CHECK(Mat4_get(&t2d, 2, 2) == 1.0f && Mat4_get(&t2d, 3, 3) == 1.0f);

    // Rotation about Z by +90 degrees: (1,0,0) -> (0,1,0).
    Mat4 rot;
    Mat4_createTransformationMatrix(0, 0, 0, 0, 0, 90, 1, 1, 1, &rot);
    Coord4 xaxis = { 0 }, rout = { 0 };
    Coord4_set(&xaxis, 1.0f, 0.0f, 0.0f, 1.0f);
    Mat4_transform(&rot, &xaxis, &rout);
    CHECK(CLOSE(Coord4_getX(&rout), 0.0f));
    CHECK(CLOSE(Coord4_getY(&rout), 1.0f));
}

// Checks axis rotation, zero-axis rejection, and transform chaining.
static void test_rotate_axis(void) {
    Mat4 ident;
    Mat4_identity(&ident);
    Mat4 r;
    Mat4_identity(&r);
    Mat4_rotate(&r, 1.5707963f, 0, 0, 1, &r);       // +90 deg about Z, in place
    Coord4 x = { 0 }, out = { 0 };
    Coord4_set(&x, 1.0f, 0.0f, 0.0f, 1.0f);
    Mat4_transform(&r, &x, &out);
    CHECK(fabsf(Coord4_getX(&out)) < 0.01f);          // fast invSqrt tolerance
    CHECK(fabsf(Coord4_getY(&out) - 1.0f) < 0.01f);

    // A rotation carries src's translation column (3) into a non-aliased dest.
    Mat4 srcTranslated;
    Mat4_identity(&srcTranslated);
    Mat4_translate(&srcTranslated, 7.0f, 0.0f, 0.0f, &srcTranslated);
    Mat4 dest;
    Mat4_identity(&dest);
    Mat4_rotate(&srcTranslated, 0.5f, 0, 0, 1, &dest);
    CHECK(CLOSE(Mat4_get(&dest, 0, 3), 7.0f));

    // A zero-length axis returns without touching dest.
    Mat4 keep;
    Mat4_createTransformationMatrix(5, 0, 0, 0, 0, 0, 1, 1, 1, &keep);
    Mat4 before = keep;
    Mat4_rotate(&ident, 1.0f, 0, 0, 0, &keep);
    CHECK(mat_equal(&keep, &before));

    // Translate then scale (dest-last chains).
    Mat4 t;
    Mat4_translate(&ident, 1, 2, 3, &t);
    Mat4 s;
    Mat4_scale(&t, 2, 2, 2, &s);
    CHECK(CLOSE(Mat4_get(&s, 0, 0), 2.0f));
    CHECK(CLOSE(Mat4_get(&s, 0, 3), 1.0f));      // translation preserved
}

// Checks perspective, Vulkan perspective, and orthographic depth mappings.
static void test_projections(void) {
    Mat4 p;
    Mat4_perspective(1.5707963f, 1.0f, 1.0f, 100.0f, &p);
    CHECK(Mat4_get(&p, 3, 2) == -1.0f);
    CHECK(CLOSE(Mat4_get(&p, 0, 0), 1.0f));
    CHECK(CLOSE(Mat4_get(&p, 1, 1), 1.0f));

    Coord4 near = { 0 }, out = { 0 };
    Coord4_set(&near, 0, 0, -1, 1);
    Mat4_transform(&p, &near, &out);
    CHECK(CLOSE(Coord4_getZ(&out) / Coord4_getW(&out), -1.0f));    // near plane -> -1
    Coord4 far = { 0 };
    Coord4_set(&far, 0, 0, -100, 1);
    Mat4_transform(&p, &far, &out);
    CHECK(CLOSE(Coord4_getZ(&out) / Coord4_getW(&out), 1.0f));     // far plane -> +1

    // Vulkan variant: Y flipped, depth in [0,1].
    Mat4 pv;
    Mat4_perspectiveVulkan(1.5707963f, 1.0f, 1.0f, 100.0f, &pv);
    CHECK(CLOSE(Mat4_get(&pv, 1, 1), -1.0f));
    Coord4_set(&near, 0, 0, -1, 1);
    Mat4_transform(&pv, &near, &out);
    CHECK(CLOSE(Coord4_getZ(&out) / Coord4_getW(&out), 0.0f));
    Coord4_set(&far, 0, 0, -100, 1);
    Mat4_transform(&pv, &far, &out);
    CHECK(CLOSE(Coord4_getZ(&out) / Coord4_getW(&out), 1.0f));

    // Orthographic near/far mapping.
    Mat4 o;
    Mat4_orthographic(-1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 100.0f, &o);
    CHECK(CLOSE(Mat4_get(&o, 0, 0), 1.0f));
    CHECK(CLOSE(Mat4_get(&o, 1, 1), 1.0f));
    Coord4_set(&near, 0, 0, -1, 1);
    Mat4_transform(&o, &near, &out);
    CHECK(CLOSE(Coord4_getZ(&out), -1.0f));
    Coord4_set(&far, 0, 0, -100, 1);
    Mat4_transform(&o, &far, &out);
    CHECK(CLOSE(Coord4_getZ(&out), 1.0f));
}

// Checks that look-at maps the world origin into the expected view position.
static void test_look_at(void) {
    Mat4 v;
    Mat4_lookAt(0, 0, 5, 0, 0, 0, 0, 1, 0, &v);
    Coord4 origin = { 0 }, out = { 0 };
    Coord4_set(&origin, 0, 0, 0, 1);
    Mat4_transform(&v, &origin, &out);
    CHECK(CLOSE(Coord4_getX(&out), 0.0f));
    CHECK(CLOSE(Coord4_getY(&out), 0.0f));
    CHECK(fabsf(Coord4_getZ(&out) + 5.0f) < 0.02f);   // fast invSqrt tolerance
    CHECK(CLOSE(Coord4_getW(&out), 1.0f));
}

// Runs the Mat4 owner cases after initializing the memory substrate.
int main(void) {
    CHECK(Memory_init(0));
    test_construction();
    test_layout();
    test_multiply();
    test_transpose();
    test_transforms();
    test_rotate_axis();
    test_projections();
    test_look_at();

    if (g_failures == 0) {
        printf("mat4_test: all assertions held\n");
        return 0;
    }
    printf("mat4_test: %d FAILURES\n", g_failures);
    return 1;
}
