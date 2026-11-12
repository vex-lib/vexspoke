#include "lang/vec3/vec3.h"

#include <math.h>
#include <string.h>

#include "math/strict_math.h"
#include "math/fast_math.h"
#include "nio/mem.h"
#include "oop/vexspoke_type.h"
#include "util/annotation.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Coord3
 * ============================================================================
 * Coordinate-agnostic 3D spatial vector: horizontal/vertical/depth components
 * with the owning CoordFrame stored as true embedded metadata in the 4th
 * f32 slot, preserving 16-byte SIMD alignment for NEON/SSE. Directional
 * accessors (right/left/up/down/front/back) expose semantic axes whose
 * inverses are exact negations, while frame-mapped X/Y/Z accessors translate
 * through CoordFrame axis index/sign tables. All arithmetic is dest-last and
 * null-guarded; normalize uses strict IEEE inv-sqrt while fastNormalize uses
 * the relaxed Quake approximation.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Coord3 (lang/vec3/vec3.c — defined in lang/vec3/vec3.h)
 * ============================================================================
 * Coordinate-agnostic 3D spatial vector carrying embedded CoordFrame metadata.
 * Components:
 *   - horizontal: Right (+) / Left (-)
 *   - vertical:   Up (+)    / Down (-)
 *   - depth:      Front (+) / Back (-)
 *
 * Direct directional inversion:
 *   getBack() == -getFront()
 *   getLeft() == -getRight()
 *   getDown() == -getUp()
 *
 * STRUCT FIELDS (Mirroring lang/vec3/vec3.h):
 * ----------------------------------------------------------------------------
 *   Coord3 {
 *     alignas(16) union {
 *       struct { f32 horizontal; f32 vertical; f32 depth; u32 frame; };
 *       struct { f32 x; f32 y; f32 z; u32 _frame; };
 *       struct { f32 right; f32 up; f32 front; u32 _f; };
 *       f32 data[4];
 *     };
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Constructors: (.h)
 *   - Coord3_0(void)
 *   - Coord3_3(horizontal, vertical, depth)
 *   - Coord3_4(horizontal, vertical, depth, frame)
 *   - Coord3_free(v)
 *
 * Public Setters: (.h)
 *   - Coord3_setRight(v, val) / Coord3_setLeft(v, val)
 *   - Coord3_setUp(v, val) / Coord3_setDown(v, val)
 *   - Coord3_setFront(v, val) / Coord3_setBack(v, val)
 *   - Coord3_setFrame(v, frame)
 *   - Coord3_setX(v, x) / Coord3_setY(v, y) / Coord3_setZ(v, z)
 *   - Coord3_set(v, horizontal, vertical, depth)
 *
 * Public Getters: (.h)
 *   - Coord3_getRight(v) / Coord3_getLeft(v)
 *   - Coord3_getUp(v) / Coord3_getDown(v)
 *   - Coord3_getFront(v) / Coord3_getBack(v)
 *   - Coord3_getFrame(v)
 *   - Coord3_getX(v) / Coord3_getY(v) / Coord3_getZ(v)
 *   - Coord3_getXInFrame(v, frame) / Coord3_getYInFrame(v, frame) / Coord3_getZInFrame(v, frame)
 *
 * Public Core Functions: (.h)
 *   - Coord3_copy(src, dest)
 *   - Coord3_toFrame(src, targetFrame, dest)
 *   - Coord3_add(a, b.sh, dest) / Coord3_sub(a, b.sh, dest)
 *   - Coord3_mul(a, scalar, dest) / Coord3_div(a, scalar, dest)
 *   - Coord3_dot(a, b.sh)
 *   - Coord3_cross(a, b.sh, dest)
 *   - Coord3_lengthSquared(v) / Coord3_length(v)
 *   - Coord3_normalize(src, dest) / Coord3_fastNormalize(src, dest)
 *   - Coord3_distance(a, b.sh) / Coord3_angle(a, b.sh)
 *   - Coord3_project(vector, onto, dest)
 *   - Coord3_reflect(incident, normal, dest)
 *   - Coord3_clamp(src, min_val, max_val, dest)
 *   - Coord3_abs(src, dest)
 *   - Coord3_lerp(a, b.sh, t, dest)
 * ============================================================================
 */

Coord3 *Coord3_0(void) {
    Coord3 *v = (Coord3*) Memory_alloc(ID_VEC3, VEC3_BYTES);
    if (!v) return nullptr;
    (*v).horizontal = 0.0f;
    (*v).vertical = 0.0f;
    (*v).depth = 0.0f;
    (*v).frame = (u32) COORD_FRAME_DEFAULT;
    return v;
}

Coord3 *Coord3_3(f32 horizontal, f32 vertical, f32 depth) {
    Coord3 *v = (Coord3*) Memory_alloc(ID_VEC3, VEC3_BYTES);
    if (!v) return nullptr;
    (*v).horizontal = horizontal;
    (*v).vertical = vertical;
    (*v).depth = depth;
    (*v).frame = (u32) COORD_FRAME_DEFAULT;
    return v;
}

Coord3 *Coord3_4(f32 horizontal, f32 vertical, f32 depth, CoordFrame frame) {
    Coord3 *v = (Coord3*) Memory_alloc(ID_VEC3, VEC3_BYTES);
    if (!v) return nullptr;
    (*v).horizontal = horizontal;
    (*v).vertical = vertical;
    (*v).depth = depth;
    (*v).frame = CoordFrame_isValid(frame) ? (u32) frame : (u32) COORD_FRAME_DEFAULT;
    return v;
}

void Coord3_free(Coord3 *v) {
    if (v) Memory_free(v);
}

f32 Coord3_getRight(const Coord3 *v) { return v ? (*v).horizontal : 0.0f; }
void  Coord3_setRight(Coord3 *v, f32 val) { if (v) (*v).horizontal = val; }
f32 Coord3_getLeft(const Coord3 *v) { return v ? -(*v).horizontal : 0.0f; }
void  Coord3_setLeft(Coord3 *v, f32 val) { if (v) (*v).horizontal = -val; }

f32 Coord3_getUp(const Coord3 *v) { return v ? (*v).vertical : 0.0f; }
void  Coord3_setUp(Coord3 *v, f32 val) { if (v) (*v).vertical = val; }
f32 Coord3_getDown(const Coord3 *v) { return v ? -(*v).vertical : 0.0f; }
void  Coord3_setDown(Coord3 *v, f32 val) { if (v) (*v).vertical = -val; }

f32 Coord3_getFront(const Coord3 *v) { return v ? (*v).depth : 0.0f; }
void  Coord3_setFront(Coord3 *v, f32 val) { if (v) (*v).depth = val; }
f32 Coord3_getBack(const Coord3 *v) { return v ? -(*v).depth : 0.0f; }
void  Coord3_setBack(Coord3 *v, f32 val) { if (v) (*v).depth = -val; }

CoordFrame Coord3_getFrame(const Coord3 *v) {
    return v ? (CoordFrame) (*v).frame : COORD_FRAME_DEFAULT;
}

void Coord3_setFrame(Coord3 *v, CoordFrame frame) {
    if (v && CoordFrame_isValid(frame)) {
        (*v).frame = (u32) frame;
    }
}

f32 Coord3_getXInFrame(const Coord3 *v, CoordFrame frame) {
    if (!v) return 0.0f;
    return CoordFrame_getAxisSign(frame, 0) * (*v).data[CoordFrame_getAxisIndex(frame, 0)];
}

f32 Coord3_getYInFrame(const Coord3 *v, CoordFrame frame) {
    if (!v) return 0.0f;
    return CoordFrame_getAxisSign(frame, 1) * (*v).data[CoordFrame_getAxisIndex(frame, 1)];
}

f32 Coord3_getZInFrame(const Coord3 *v, CoordFrame frame) {
    if (!v) return 0.0f;
    return CoordFrame_getAxisSign(frame, 2) * (*v).data[CoordFrame_getAxisIndex(frame, 2)];
}

f32 Coord3_getX(const Coord3 *v) {
    if (!v) return 0.0f;
    return Coord3_getXInFrame(v, (CoordFrame) (*v).frame);
}

void Coord3_setX(Coord3 *v, f32 x) {
    if (!v) return;
    CoordFrame f = (CoordFrame) (*v).frame;
     idx = CoordFrame_getAxisIndex(f, 0);
    f32 sign = CoordFrame_getAxisSign(f, 0);
    (*v).data[idx] = x * sign;
}

f32 Coord3_getY(const Coord3 *v) {
    if (!v) return 0.0f;
    return Coord3_getYInFrame(v, (CoordFrame) (*v).frame);
}

void Coord3_setY(Coord3 *v, f32 y) {
    if (!v) return;
    CoordFrame f = (CoordFrame) (*v).frame;
     idx = CoordFrame_getAxisIndex(f, 1);
    f32 sign = CoordFrame_getAxisSign(f, 1);
    (*v).data[idx] = y * sign;
}

f32 Coord3_getZ(const Coord3 *v) {
    if (!v) return 0.0f;
    return Coord3_getZInFrame(v, (CoordFrame) (*v).frame);
}

void Coord3_setZ(Coord3 *v, f32 z) {
    if (!v) return;
    CoordFrame f = (CoordFrame) (*v).frame;
     idx = CoordFrame_getAxisIndex(f, 2);
    f32 sign = CoordFrame_getAxisSign(f, 2);
    (*v).data[idx] = z * sign;
}

void Coord3_set(Coord3 *v, f32 horizontal, f32 vertical, f32 depth) {
    if (!v) return;
    (*v).horizontal = horizontal;
    (*v).vertical = vertical;
    (*v).depth = depth;
}

void Coord3_copy(const Coord3 *src, Coord3 *dest) {
    if (!src || !dest) return;
    (*dest).horizontal = (*src).horizontal;
    (*dest).vertical = (*src).vertical;
    (*dest).depth = (*src).depth;
    (*dest).frame = (*src).frame;
}

void Coord3_toFrame(const Coord3 *src, CoordFrame targetFrame, Coord3 *dest) {
    if (!src || !dest) return;
    if (!CoordFrame_isValid(targetFrame)) {
        targetFrame = COORD_FRAME_DEFAULT;
    }
    // Extract semantic directions from src
    f32 r = (*src).horizontal;
    f32 u = (*src).vertical;
    f32 f = (*src).depth;

    (*dest).horizontal = r;
    (*dest).vertical = u;
    (*dest).depth = f;
    (*dest).frame = (u32) targetFrame;
}

void Coord3_add(const Coord3 *a, const Coord3 *b, Coord3 *dest) {
    if (!a || !b || !dest) return;
    (*dest).horizontal = (*a).horizontal + (*b).horizontal;
    (*dest).vertical = (*a).vertical + (*b).vertical;
    (*dest).depth = (*a).depth + (*b).depth;
    (*dest).frame = (*a).frame;
}

void Coord3_sub(const Coord3 *a, const Coord3 *b, Coord3 *dest) {
    if (!a || !b || !dest) return;
    (*dest).horizontal = (*a).horizontal - (*b).horizontal;
    (*dest).vertical = (*a).vertical - (*b).vertical;
    (*dest).depth = (*a).depth - (*b).depth;
    (*dest).frame = (*a).frame;
}

void Coord3_mul(const Coord3 *a, f32 scalar, Coord3 *dest) {
    if (!a || !dest) return;
    (*dest).horizontal = (*a).horizontal * scalar;
    (*dest).vertical = (*a).vertical * scalar;
    (*dest).depth = (*a).depth * scalar;
    (*dest).frame = (*a).frame;
}

void Coord3_div(const Coord3 *a, f32 scalar, Coord3 *dest) {
    if (!a || !dest || scalar == 0.0f) return;
    f32 inv = 1.0f / scalar;
    (*dest).horizontal = (*a).horizontal * inv;
    (*dest).vertical = (*a).vertical * inv;
    (*dest).depth = (*a).depth * inv;
    (*dest).frame = (*a).frame;
}

f32 Coord3_dot(const Coord3 *a, const Coord3 *b) {
    if (!a || !b) return 0.0f;
    return (*a).horizontal * (*b).horizontal +
           (*a).vertical * (*b).vertical +
           (*a).depth * (*b).depth;
}

void Coord3_cross(const Coord3 *a, const Coord3 *b, Coord3 *dest) {
    if (!a || !b || !dest) return;
    f32 x1 = (*a).horizontal, y1 = (*a).vertical, z1 = (*a).depth;
    f32 x2 = (*b).horizontal, y2 = (*b).vertical, z2 = (*b).depth;
    (*dest).horizontal = y1 * z2 - z1 * y2;
    (*dest).vertical = z1 * x2 - x1 * z2;
    (*dest).depth = x1 * y2 - y1 * x2;
    (*dest).frame = (*a).frame;
}

f32 Coord3_lengthSquared(const Coord3 *v) {
    if (!v) return 0.0f;
    return (*v).horizontal * (*v).horizontal +
           (*v).vertical * (*v).vertical +
           (*v).depth * (*v).depth;
}

f32 Coord3_length(const Coord3 *v) {
    return StrictMath_sqrt(Coord3_lengthSquared(v));
}

void Coord3_normalize(const Coord3 *src, Coord3 *dest) {
    if (!src || !dest) return;
    f32 lenSq = Coord3_lengthSquared(src);
    if (lenSq < 1e-12f) {
        (*dest).horizontal = 0.0f;
        (*dest).vertical = 0.0f;
        (*dest).depth = 0.0f;
        (*dest).frame = (*src).frame;
        return;
    }
    f32 invLen = StrictMath_invSqrt(lenSq);
    Coord3_mul(src, invLen, dest);
}

void Coord3_fastNormalize(const Coord3 *src, Coord3 *dest) {
    if (!src || !dest) return;
    f32 lenSq = Coord3_lengthSquared(src);
    if (lenSq < 1e-8f) {
        (*dest).horizontal = 0.0f;
        (*dest).vertical = 0.0f;
        (*dest).depth = 0.0f;
        (*dest).frame = (*src).frame;
        return;
    }
    f32 invLen = FastMath_invSqrt(lenSq);
    Coord3_mul(src, invLen, dest);
}

f32 Coord3_distance(const Coord3 *a, const Coord3 *b) {
    if (!a || !b) return 0.0f;
    f32 dx = (*a).horizontal - (*b).horizontal;
    f32 dy = (*a).vertical - (*b).vertical;
    f32 dz = (*a).depth - (*b).depth;
    return StrictMath_sqrt(dx * dx + dy * dy + dz * dz);
}

f32 Coord3_angle(const Coord3 *a, const Coord3 *b) {
    if (!a || !b) return 0.0f;
    f32 d = Coord3_dot(a, b);
    f32 l = Coord3_length(a) * Coord3_length(b);
    if (l < 1e-12f) return 0.0f;
    f32 cosVal = StrictMath_clamp(d / l, -1.0f, 1.0f);
    return StrictMath_acos(cosVal);
}

void Coord3_project(const Coord3 *vector, const Coord3 *onto, Coord3 *dest) {
    if (!vector || !onto || !dest) return;
    f32 ontoLenSq = Coord3_lengthSquared(onto);
    if (ontoLenSq < 1e-12f) {
        Coord3_set(dest, 0.0f, 0.0f, 0.0f);
        return;
    }
    f32 scale = Coord3_dot(vector, onto) / ontoLenSq;
    Coord3_mul(onto, scale, dest);
}

void Coord3_reflect(const Coord3 *incident, const Coord3 *normal, Coord3 *dest) {
    if (!incident || !normal || !dest) return;
    f32 factor = 2.0f * Coord3_dot(incident, normal);
    (*dest).horizontal = (*incident).horizontal - factor * (*normal).horizontal;
    (*dest).vertical = (*incident).vertical - factor * (*normal).vertical;
    (*dest).depth = (*incident).depth - factor * (*normal).depth;
    (*dest).frame = (*incident).frame;
}

void Coord3_clamp(const Coord3 *src, f32 min_val, f32 max_val, Coord3 *dest) {
    if (!src || !dest) return;
    (*dest).horizontal = StrictMath_clamp((*src).horizontal, min_val, max_val);
    (*dest).vertical = StrictMath_clamp((*src).vertical, min_val, max_val);
    (*dest).depth = StrictMath_clamp((*src).depth, min_val, max_val);
    (*dest).frame = (*src).frame;
}

void Coord3_abs(const Coord3 *src, Coord3 *dest) {
    if (!src || !dest) return;
    (*dest).horizontal = StrictMath_abs((*src).horizontal);
    (*dest).vertical = StrictMath_abs((*src).vertical);
    (*dest).depth = StrictMath_abs((*src).depth);
    (*dest).frame = (*src).frame;
}

void Coord3_lerp(const Coord3 *a, const Coord3 *b, f32 t, Coord3 *dest) {
    if (!a || !b || !dest) return;
    (*dest).horizontal = StrictMath_lerp((*a).horizontal, (*b).horizontal, t);
    (*dest).vertical = StrictMath_lerp((*a).vertical, (*b).vertical, t);
    (*dest).depth = StrictMath_lerp((*a).depth, (*b).depth, t);
    (*dest).frame = (*a).frame;
}
