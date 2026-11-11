#ifndef LANG_VEC3_VEC3_H
#define LANG_VEC3_VEC3_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "lang/constructor.h"
#include "math/coord_frame.h"

// lang/vec3/vec3.h — 16-byte SIMD Spatial 3D Vector.
//
// Single Class Per File Law: Coord3.
//
// Represents a coordinate-agnostic 3D spatial vector with horizontal, vertical,
// and depth dimensions, carrying its CoordFrame as true embedded metadata in
// the 4th f32 slot. Preserves 16-byte SIMD alignment for NEON / SSE.
//
// Spatial semantic axes:
//   - horizontal (x): Right (+), Left (-)
//   - vertical   (y): Up (+),    Down (-)
//   - depth      (z): Front (+), Back (-)
//
// Direct directional inverses:
//   getBack() == -getFront() (e.g. if front is 28, back is -28)
//   getLeft() == -getRight()
//   getDown() == -getUp()

typedef struct Coord3 {
    union {
        struct { f32 horizontal; f32 vertical; f32 depth; u32 frame; };
        struct { f32 x; f32 y; f32 z; u32 _frame; };
        struct { f32 right; f32 up; f32 front; u32 _f; };
        alignas(16) f32 data[4];
    };
} Coord3;

#define VEC3_BYTES 16u

// Constructors
Coord3 *Coord3_0(void);
Coord3 *Coord3_3(f32 horizontal, f32 vertical, f32 depth);
Coord3 *Coord3_4(f32 horizontal, f32 vertical, f32 depth, CoordFrame frame);
#define Coord3(...) CONSTRUCTOR_DISPATCH(Coord3, __VA_ARGS__)
void Coord3_free(Coord3 *v);

// Spatial Directional Getters & Setters
f32 Coord3_getRight(const Coord3 *v);
void Coord3_setRight(Coord3 *v, f32 val);
f32 Coord3_getLeft(const Coord3 *v);
void Coord3_setLeft(Coord3 *v, f32 val);

f32 Coord3_getUp(const Coord3 *v);
void Coord3_setUp(Coord3 *v, f32 val);
f32 Coord3_getDown(const Coord3 *v);
void Coord3_setDown(Coord3 *v, f32 val);

f32 Coord3_getFront(const Coord3 *v);
void Coord3_setFront(Coord3 *v, f32 val);
f32 Coord3_getBack(const Coord3 *v);
void Coord3_setBack(Coord3 *v, f32 val);

// Frame Metadata Accessors
CoordFrame Coord3_getFrame(const Coord3 *v);
void Coord3_setFrame(Coord3 *v, CoordFrame frame);

// Frame-mapped One-Line Coordinate Accessors
f32 Coord3_getX(const Coord3 *v);
void Coord3_setX(Coord3 *v, f32 x);
f32 Coord3_getY(const Coord3 *v);
void Coord3_setY(Coord3 *v, f32 y);
f32 Coord3_getZ(const Coord3 *v);
void Coord3_setZ(Coord3 *v, f32 z);

f32 Coord3_getXInFrame(const Coord3 *v, CoordFrame frame);
f32 Coord3_getYInFrame(const Coord3 *v, CoordFrame frame);
f32 Coord3_getZInFrame(const Coord3 *v, CoordFrame frame);

void Coord3_set(Coord3 *v, f32 horizontal, f32 vertical, f32 depth);
void Coord3_copy(const Coord3 *src, Coord3 *dest);

// Coordinate Frame Transform
void Coord3_toFrame(const Coord3 *src, CoordFrame targetFrame, Coord3 *dest);

// Vector Arithmetic
void Coord3_add(const Coord3 *a, const Coord3 *b, Coord3 *dest);
void Coord3_sub(const Coord3 *a, const Coord3 *b, Coord3 *dest);
void Coord3_mul(const Coord3 *a, f32 scalar, Coord3 *dest);
void Coord3_div(const Coord3 *a, f32 scalar, Coord3 *dest);

f32 Coord3_dot(const Coord3 *a, const Coord3 *b);
void Coord3_cross(const Coord3 *a, const Coord3 *b, Coord3 *dest);

f32 Coord3_lengthSquared(const Coord3 *v);
f32 Coord3_length(const Coord3 *v);

// Normalization: Strict IEEE (1/sqrt) vs Fast Relaxed (Quake invSqrt)
void Coord3_normalize(const Coord3 *src, Coord3 *dest);
void Coord3_fastNormalize(const Coord3 *src, Coord3 *dest);

f32 Coord3_distance(const Coord3 *a, const Coord3 *b);
f32 Coord3_angle(const Coord3 *a, const Coord3 *b);

void Coord3_project(const Coord3 *vector, const Coord3 *onto, Coord3 *dest);
void Coord3_reflect(const Coord3 *incident, const Coord3 *normal, Coord3 *dest);
void Coord3_clamp(const Coord3 *src, f32 min_val, f32 max_val, Coord3 *dest);
void Coord3_abs(const Coord3 *src, Coord3 *dest);
void Coord3_lerp(const Coord3 *a, const Coord3 *b, f32 t, Coord3 *dest);

#endif
