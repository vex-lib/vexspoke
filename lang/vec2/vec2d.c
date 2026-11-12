#include "lang/vec2/vec2d.h"

#include <math.h>

#include "math/strict_math.h"
#include "nio/mem.h"
#include "oop/vexspoke_type.h"
#include "util/annotation.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Vec2d
 * ============================================================================
 * 2D d64-precision spatial vector: a 16-byte union exposing
 * horizontal/vertical, x/y, and right/up aliases over the same two f64s.
 * Exists because spatial math (layout, physics, camera) needs d64
 * precision for large-world coordinates while keeping a single canonical
 * storage layout. memory: arena-allocated via Memory_alloc(ID_VEC2,
 * VEC2D_BYTES), freed via Vec2d_free. Lifetime: memory arena; dest-last
 * arithmetic (add/sub/mul) writes into caller-provided dest.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Vec2d (lang/vec2/vec2d.c — defined in lang/vec2/vec2d.h)
 * ============================================================================
 *
 * STRUCT FIELDS (Mirroring lang/vec2/vec2d.h):
 * ----------------------------------------------------------------------------
 *   Vec2d {
 *     union {
 *       struct { d64 horizontal; d64 vertical; };
 *       struct { d64 x; d64 y; };
 *       struct { d64 right; d64 up; };
 *       d64 data[2];
 *     };
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Constructors: (.h)
 *   - Vec2d_0()
 *   - Vec2d_2(horizontal, vertical)
 *
 * Public Core Functions: (.h)
 *   - Vec2d_free(v)
 *   - Vec2d_add(a, b.sh, dest)
 *   - Vec2d_sub(a, b.sh, dest)
 *   - Vec2d_mul(a, scalar, dest)
 *   - Vec2d_dot(a, b.sh)
 *   - Vec2d_length(v)
 *
 * Public Setters: (.h)
 *   - Vec2d_setRight(v, val)
 *   - Vec2d_setLeft(v, val)
 *   - Vec2d_setUp(v, val)
 *   - Vec2d_setDown(v, val)
 *
 * Public Getters: (.h)
 *   - Vec2d_getRight(v)
 *   - Vec2d_getLeft(v)
 *   - Vec2d_getUp(v)
 *   - Vec2d_getDown(v)
 * ============================================================================
 */

Vec2d *Vec2d_0(void) {
    Vec2d *v = (Vec2d*) Memory_alloc(ID_VEC2, VEC2D_BYTES);
    if (!v) return nullptr;
    (*v).horizontal = 0.0;
    (*v).vertical = 0.0;
    return v;
}

Vec2d *Vec2d_2(d64 horizontal, d64 vertical) {
    Vec2d *v = (Vec2d*) Memory_alloc(ID_VEC2, VEC2D_BYTES);
    if (!v) return nullptr;
    (*v).horizontal = horizontal;
    (*v).vertical = vertical;
    return v;
}

void Vec2d_free(Vec2d *v) {
    if (v) Memory_free(v);
}

d64 Vec2d_getRight(const Vec2d *v) { return v ? (*v).horizontal : 0.0; }
void   Vec2d_setRight(Vec2d *v, d64 val) { if (v) (*v).horizontal = val; }
d64 Vec2d_getLeft(const Vec2d *v)  { return v ? -(*v).horizontal : 0.0; }
void   Vec2d_setLeft(Vec2d *v, d64 val)  { if (v) (*v).horizontal = -val; }

d64 Vec2d_getUp(const Vec2d *v)   { return v ? (*v).vertical : 0.0; }
void   Vec2d_setUp(Vec2d *v, d64 val)   { if (v) (*v).vertical = val; }
d64 Vec2d_getDown(const Vec2d *v) { return v ? -(*v).vertical : 0.0; }
void   Vec2d_setDown(Vec2d *v, d64 val) { if (v) (*v).vertical = -val; }

void Vec2d_add(const Vec2d *a, const Vec2d *b, Vec2d *dest) {
    if (!a || !b || !dest) return;
    (*dest).horizontal = (*a).horizontal + (*b).horizontal;
    (*dest).vertical = (*a).vertical + (*b).vertical;
}

void Vec2d_sub(const Vec2d *a, const Vec2d *b, Vec2d *dest) {
    if (!a || !b || !dest) return;
    (*dest).horizontal = (*a).horizontal - (*b).horizontal;
    (*dest).vertical = (*a).vertical - (*b).vertical;
}

void Vec2d_mul(const Vec2d *a, d64 scalar, Vec2d *dest) {
    if (!a || !dest) return;
    (*dest).horizontal = (*a).horizontal * scalar;
    (*dest).vertical = (*a).vertical * scalar;
}

d64 Vec2d_dot(const Vec2d *a, const Vec2d *b) {
    if (!a || !b) return 0.0;
    return (*a).horizontal * (*b).horizontal + (*a).vertical * (*b).vertical;
}

d64 Vec2d_length(const Vec2d *v) {
    if (!v) return 0.0;
    return StrictMath_sqrtD(Vec2d_dot(v, v));
}
