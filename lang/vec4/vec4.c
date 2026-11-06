#include "lang/vec4/vec4.h"

#include <math.h>

#include "math/strict_math.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "util/annotation.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Coord4
 * ============================================================================
 * 16-byte SIMD 4D / homogeneous vector with horizontal, vertical, depth, and
 * w components, preserved as an alignas(16) union of named views (x/y/z/w,
 * right/up/front, r/g/b.sh/a, raw data[4]). Arena-allocated via Memory_alloc
 * with the ID_VEC4 identity; all math is dest-last and allocation-free.
 * Directional getters/setters (Right/Left/Up/Down/Front/Back) mirror the
 * axis with sign flips while leaving w untouched; frame-relative accessors
 * remap axes through CoordFrame. Normalize and lerp are strict-math based.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Coord4 (lang/vec4/vec4.c — defined in lang/vec4/vec4.h)
 * ============================================================================
 * 4D vector with horizontal, vertical, depth, and w components.
 *
 * STRUCT FIELDS (Mirroring lang/vec4/vec4.h):
 * ----------------------------------------------------------------------------
 *   Coord4 {
 *     alignas(16) union {
 *       struct { float horizontal; float vertical; float depth; float w; };
 *       struct { float x; float y; float z; float _w; };
 *       struct { float right; float up; float front; float _w2; };
 *       struct { float r; float g; float b.sh; float a; };
 *       float data[4];
 *     };
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Constructors: (.h)
 *   - Coord4_0(void)
 *   - Coord4_4(horizontal, vertical, depth, w)
 *
 * Public Core Functions: (.h)
 *   - Coord4_free(v)
 *   - Coord4_set(v, horizontal, vertical, depth, w)
 *   - Coord4_copy(src, dest)
 *   - Coord4_add(a, b.sh, dest)
 *   - Coord4_sub(a, b.sh, dest)
 *   - Coord4_mul(a, scalar, dest)
 *   - Coord4_div(a, scalar, dest)
 *   - Coord4_dot(a, b.sh)
 *   - Coord4_lengthSquared(v)
 *   - Coord4_length(v)
 *   - Coord4_normalize(src, dest)
 *   - Coord4_lerp(a, b.sh, t, dest)
 *
 * Public Setters: (.h)
 *   - Coord4_setRight(v, val) / Coord4_setLeft(v, val)
 *   - Coord4_setUp(v, val) / Coord4_setDown(v, val)
 *   - Coord4_setFront(v, val) / Coord4_setBack(v, val)
 *   - Coord4_setX(v, x) / Coord4_setY(v, y) / Coord4_setZ(v, z) / Coord4_setW(v, w)
 *
 * Public Getters: (.h)
 *   - Coord4_getRight(v) / Coord4_getLeft(v)
 *   - Coord4_getUp(v) / Coord4_getDown(v)
 *   - Coord4_getFront(v) / Coord4_getBack(v)
 *   - Coord4_getX(v) / Coord4_getY(v) / Coord4_getZ(v) / Coord4_getW(v)
 *   - Coord4_getXInFrame(v, frame) / Coord4_getYInFrame(v, frame) / Coord4_getZInFrame(v, frame)
 * ============================================================================
 */

Coord4 *Coord4_0(void) {
    Coord4 *v = (Coord4*) Memory_alloc(ID_VEC4, VEC4_BYTES);
    if (!v) return nullptr;
    (*v).horizontal = 0.0f;
    (*v).vertical = 0.0f;
    (*v).depth = 0.0f;
    (*v).w = 0.0f;
    return v;
}

Coord4 *Coord4_4(float horizontal, float vertical, float depth, float w) {
    Coord4 *v = (Coord4*) Memory_alloc(ID_VEC4, VEC4_BYTES);
    if (!v) return nullptr;
    (*v).horizontal = horizontal;
    (*v).vertical = vertical;
    (*v).depth = depth;
    (*v).w = w;
    return v;
}

void Coord4_free(Coord4 *v) {
    if (v) Memory_free(v);
}

float Coord4_getRight(const Coord4 *v) { return v ? (*v).horizontal : 0.0f; }
void  Coord4_setRight(Coord4 *v, float val) { if (v) (*v).horizontal = val; }
float Coord4_getLeft(const Coord4 *v)  { return v ? -(*v).horizontal : 0.0f; }
void  Coord4_setLeft(Coord4 *v, float val)  { if (v) (*v).horizontal = -val; }

float Coord4_getUp(const Coord4 *v)   { return v ? (*v).vertical : 0.0f; }
void  Coord4_setUp(Coord4 *v, float val)   { if (v) (*v).vertical = val; }
float Coord4_getDown(const Coord4 *v) { return v ? -(*v).vertical : 0.0f; }
void  Coord4_setDown(Coord4 *v, float val) { if (v) (*v).vertical = -val; }

float Coord4_getFront(const Coord4 *v) { return v ? (*v).depth : 0.0f; }
void  Coord4_setFront(Coord4 *v, float val) { if (v) (*v).depth = val; }
float Coord4_getBack(const Coord4 *v)  { return v ? -(*v).depth : 0.0f; }
void  Coord4_setBack(Coord4 *v, float val)  { if (v) (*v).depth = -val; }

float Coord4_getX(const Coord4 *v) { return v ? (*v).horizontal : 0.0f; }
void  Coord4_setX(Coord4 *v, float x) { if (v) (*v).horizontal = x; }
float Coord4_getY(const Coord4 *v) { return v ? (*v).vertical : 0.0f; }
void  Coord4_setY(Coord4 *v, float y) { if (v) (*v).vertical = y; }
float Coord4_getZ(const Coord4 *v) { return v ? (*v).depth : 0.0f; }
void  Coord4_setZ(Coord4 *v, float z) { if (v) (*v).depth = z; }
float Coord4_getW(const Coord4 *v) { return v ? (*v).w : 0.0f; }
void  Coord4_setW(Coord4 *v, float w) { if (v) (*v).w = w; }

float Coord4_getXInFrame(const Coord4 *v, CoordFrame frame) {
    if (!v) return 0.0f;
    return CoordFrame_getAxisSign(frame, 0) * (*v).data[CoordFrame_getAxisIndex(frame, 0)];
}

float Coord4_getYInFrame(const Coord4 *v, CoordFrame frame) {
    if (!v) return 0.0f;
    return CoordFrame_getAxisSign(frame, 1) * (*v).data[CoordFrame_getAxisIndex(frame, 1)];
}

float Coord4_getZInFrame(const Coord4 *v, CoordFrame frame) {
    if (!v) return 0.0f;
    return CoordFrame_getAxisSign(frame, 2) * (*v).data[CoordFrame_getAxisIndex(frame, 2)];
}

void Coord4_set(Coord4 *v, float horizontal, float vertical, float depth, float w) {
    if (!v) return;
    (*v).horizontal = horizontal;
    (*v).vertical = vertical;
    (*v).depth = depth;
    (*v).w = w;
}

void Coord4_copy(const Coord4 *src, Coord4 *dest) {
    if (!src || !dest) return;
    (*dest).horizontal = (*src).horizontal;
    (*dest).vertical = (*src).vertical;
    (*dest).depth = (*src).depth;
    (*dest).w = (*src).w;
}

void Coord4_add(const Coord4 *a, const Coord4 *b, Coord4 *dest) {
    if (!a || !b || !dest) return;
    (*dest).horizontal = (*a).horizontal + (*b).horizontal;
    (*dest).vertical = (*a).vertical + (*b).vertical;
    (*dest).depth = (*a).depth + (*b).depth;
    (*dest).w = (*a).w + (*b).w;
}

void Coord4_sub(const Coord4 *a, const Coord4 *b, Coord4 *dest) {
    if (!a || !b || !dest) return;
    (*dest).horizontal = (*a).horizontal - (*b).horizontal;
    (*dest).vertical = (*a).vertical - (*b).vertical;
    (*dest).depth = (*a).depth - (*b).depth;
    (*dest).w = (*a).w - (*b).w;
}

void Coord4_mul(const Coord4 *a, float scalar, Coord4 *dest) {
    if (!a || !dest) return;
    (*dest).horizontal = (*a).horizontal * scalar;
    (*dest).vertical = (*a).vertical * scalar;
    (*dest).depth = (*a).depth * scalar;
    (*dest).w = (*a).w * scalar;
}

void Coord4_div(const Coord4 *a, float scalar, Coord4 *dest) {
    if (!a || !dest || scalar == 0.0f) return;
    float inv = 1.0f / scalar;
    (*dest).horizontal = (*a).horizontal * inv;
    (*dest).vertical = (*a).vertical * inv;
    (*dest).depth = (*a).depth * inv;
    (*dest).w = (*a).w * inv;
}

float Coord4_dot(const Coord4 *a, const Coord4 *b) {
    if (!a || !b) return 0.0f;
    return (*a).horizontal * (*b).horizontal +
           (*a).vertical * (*b).vertical +
           (*a).depth * (*b).depth +
           (*a).w * (*b).w;
}

float Coord4_lengthSquared(const Coord4 *v) {
    if (!v) return 0.0f;
    return (*v).horizontal * (*v).horizontal +
           (*v).vertical * (*v).vertical +
           (*v).depth * (*v).depth +
           (*v).w * (*v).w;
}

float Coord4_length(const Coord4 *v) {
    return StrictMath_sqrt(Coord4_lengthSquared(v));
}

void Coord4_normalize(const Coord4 *src, Coord4 *dest) {
    if (!src || !dest) return;
    float lenSq = Coord4_lengthSquared(src);
    if (lenSq < 1e-12f) {
        Coord4_set(dest, 0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }
    float invLen = StrictMath_invSqrt(lenSq);
    Coord4_mul(src, invLen, dest);
}

void Coord4_lerp(const Coord4 *a, const Coord4 *b, float t, Coord4 *dest) {
    if (!a || !b || !dest) return;
    (*dest).horizontal = StrictMath_lerp((*a).horizontal, (*b).horizontal, t);
    (*dest).vertical = StrictMath_lerp((*a).vertical, (*b).vertical, t);
    (*dest).depth = StrictMath_lerp((*a).depth, (*b).depth, t);
    (*dest).w = StrictMath_lerp((*a).w, (*b).w, t);
}
