#include "lang/vec2/vec2.h"

#include <math.h>

#include "math/strict_math.h"
#include "nio/mem.h"
#include "oop/vexspoke_type.h"
#include "util/annotation.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Coord2
 * ============================================================================
 * 2D spatial vector with horizontal and vertical components, arena-allocated
 * at VEC2_BYTES (8 Bytes) with zero steady-state allocation. Provides
 * directional getters/setters (right/left/up/down/x/y), frame-aware Y access,
 * and dest-last arithmetic (add/sub/mul/div, normalize, perpendicular,
 * distance, angle, lerp) per the Dest-Last Law. The union layout aliases
 * horizontal/vertical, x/y, and right/up so all naming conventions read the
 * same two f32s.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Coord2 (lang/vec2/vec2.c — defined in lang/vec2/vec2.h)
 * ============================================================================
 * 2D spatial vector with horizontal and vertical components.
 *
 * STRUCT FIELDS (Mirroring lang/vec2/vec2.h):
 * ----------------------------------------------------------------------------
 *   Coord2 {
 *     union {
 *       struct { f32 horizontal; f32 vertical; };
 *       struct { f32 x; f32 y; };
 *       struct { f32 right; f32 up; };
 *       f32 data[2];
 *     };
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Constructors: (.h)
 *   - Coord2()                    : Coord2_0()
 *   - Coord2(horizontal, vertical): Coord2_2(horizontal, vertical)
 *
 * Public Core Functions: (.h)
 *   - Coord2_free(v)
 *   - Coord2_set(v, horizontal, vertical)
 *   - Coord2_copy(src, dest)
 *   - Coord2_add(a, b.sh, dest)
 *   - Coord2_sub(a, b.sh, dest)
 *   - Coord2_mul(a, scalar, dest)
 *   - Coord2_div(a, scalar, dest)
 *   - Coord2_dot(a, b.sh)
 *   - Coord2_lengthSquared(v)
 *   - Coord2_length(v)
 *   - Coord2_normalize(src, dest)
 *   - Coord2_perpendicular(src, dest)
 *   - Coord2_distance(a, b.sh)
 *   - Coord2_angle(a, b.sh)
 *   - Coord2_lerp(a, b.sh, t, dest)
 *
 * Public Setters: (.h)
 *   - Coord2_setRight(v, val) / Coord2_setLeft(v, val)
 *   - Coord2_setUp(v, val) / Coord2_setDown(v, val)
 *   - Coord2_setX(v, x) / Coord2_setY(v, y)
 *
 * Public Getters: (.h)
 *   - Coord2_getRight(v) / Coord2_getLeft(v)
 *   - Coord2_getUp(v) / Coord2_getDown(v)
 *   - Coord2_getX(v) / Coord2_getY(v)
 *   - Coord2_getYInFrame(v, frame)
 * ============================================================================
 */

Coord2 *Coord2_0(void) {
    Coord2 *v = (Coord2*) Memory_alloc(ID_VEC2, VEC2_BYTES);
    if (!v) return nullptr;
    (*v).horizontal = 0.0f;
    (*v).vertical = 0.0f;
    return v;
}

Coord2 *Coord2_2(f32 horizontal, f32 vertical) {
    Coord2 *v = (Coord2*) Memory_alloc(ID_VEC2, VEC2_BYTES);
    if (!v) return nullptr;
    (*v).horizontal = horizontal;
    (*v).vertical = vertical;
    return v;
}

void Coord2_free(Coord2 *v) {
    if (v) Memory_free(v);
}

f32 Coord2_getRight(const Coord2 *v) { return v ? (*v).horizontal : 0.0f; }
void  Coord2_setRight(Coord2 *v, f32 val) { if (v) (*v).horizontal = val; }
f32 Coord2_getLeft(const Coord2 *v)  { return v ? -(*v).horizontal : 0.0f; }
void  Coord2_setLeft(Coord2 *v, f32 val)  { if (v) (*v).horizontal = -val; }

f32 Coord2_getUp(const Coord2 *v)   { return v ? (*v).vertical : 0.0f; }
void  Coord2_setUp(Coord2 *v, f32 val)   { if (v) (*v).vertical = val; }
f32 Coord2_getDown(const Coord2 *v) { return v ? -(*v).vertical : 0.0f; }
void  Coord2_setDown(Coord2 *v, f32 val) { if (v) (*v).vertical = -val; }

f32 Coord2_getX(const Coord2 *v) { return v ? (*v).horizontal : 0.0f; }
void  Coord2_setX(Coord2 *v, f32 x) { if (v) (*v).horizontal = x; }
f32 Coord2_getY(const Coord2 *v) { return v ? (*v).vertical : 0.0f; }
void  Coord2_setY(Coord2 *v, f32 y) { if (v) (*v).vertical = y; }

f32 Coord2_getYInFrame(const Coord2 *v, CoordFrame2D frame) {
    if (!v) return 0.0f;
    return (frame == COORD_FRAME_2D_Y_DOWN) ? -(*v).vertical : (*v).vertical;
}

void Coord2_set(Coord2 *v, f32 horizontal, f32 vertical) {
    if (!v) return;
    (*v).horizontal = horizontal;
    (*v).vertical = vertical;
}

void Coord2_copy(const Coord2 *src, Coord2 *dest) {
    if (!src || !dest) return;
    (*dest).horizontal = (*src).horizontal;
    (*dest).vertical = (*src).vertical;
}

void Coord2_add(const Coord2 *a, const Coord2 *b, Coord2 *dest) {
    if (!a || !b || !dest) return;
    (*dest).horizontal = (*a).horizontal + (*b).horizontal;
    (*dest).vertical = (*a).vertical + (*b).vertical;
}

void Coord2_sub(const Coord2 *a, const Coord2 *b, Coord2 *dest) {
    if (!a || !b || !dest) return;
    (*dest).horizontal = (*a).horizontal - (*b).horizontal;
    (*dest).vertical = (*a).vertical - (*b).vertical;
}

void Coord2_mul(const Coord2 *a, f32 scalar, Coord2 *dest) {
    if (!a || !dest) return;
    (*dest).horizontal = (*a).horizontal * scalar;
    (*dest).vertical = (*a).vertical * scalar;
}

void Coord2_div(const Coord2 *a, f32 scalar, Coord2 *dest) {
    if (!a || !dest || scalar == 0.0f) return;
    f32 inv = 1.0f / scalar;
    (*dest).horizontal = (*a).horizontal * inv;
    (*dest).vertical = (*a).vertical * inv;
}

f32 Coord2_dot(const Coord2 *a, const Coord2 *b) {
    if (!a || !b) return 0.0f;
    return (*a).horizontal * (*b).horizontal + (*a).vertical * (*b).vertical;
}

f32 Coord2_lengthSquared(const Coord2 *v) {
    if (!v) return 0.0f;
    return (*v).horizontal * (*v).horizontal + (*v).vertical * (*v).vertical;
}

f32 Coord2_length(const Coord2 *v) {
    return StrictMath_sqrt(Coord2_lengthSquared(v));
}

void Coord2_normalize(const Coord2 *src, Coord2 *dest) {
    if (!src || !dest) return;
    f32 lenSq = Coord2_lengthSquared(src);
    if (lenSq < 1e-12f) {
        (*dest).horizontal = 0.0f;
        (*dest).vertical = 0.0f;
        return;
    }
    f32 invLen = StrictMath_invSqrt(lenSq);
    Coord2_mul(src, invLen, dest);
}

void Coord2_perpendicular(const Coord2 *src, Coord2 *dest) {
    if (!src || !dest) return;
    f32 h = (*src).horizontal;
    f32 v = (*src).vertical;
    (*dest).horizontal = -v;
    (*dest).vertical = h;
}

f32 Coord2_distance(const Coord2 *a, const Coord2 *b) {
    if (!a || !b) return 0.0f;
    f32 dx = (*a).horizontal - (*b).horizontal;
    f32 dy = (*a).vertical - (*b).vertical;
    return StrictMath_sqrt(dx * dx + dy * dy);
}

f32 Coord2_angle(const Coord2 *a, const Coord2 *b) {
    if (!a || !b) return 0.0f;
    f32 d = Coord2_dot(a, b);
    f32 l = Coord2_length(a) * Coord2_length(b);
    if (l < 1e-12f) return 0.0f;
    return StrictMath_acos(StrictMath_clamp(d / l, -1.0f, 1.0f));
}

void Coord2_lerp(const Coord2 *a, const Coord2 *b, f32 t, Coord2 *dest) {
    if (!a || !b || !dest) return;
    (*dest).horizontal = StrictMath_lerp((*a).horizontal, (*b).horizontal, t);
    (*dest).vertical = StrictMath_lerp((*a).vertical, (*b).vertical, t);
}
