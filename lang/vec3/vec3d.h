#ifndef LANG_VEC3_VEC3D_H
#define LANG_VEC3_VEC3D_H

#include <stddef.h>
#include <stdint.h>

#include "lang/constructor.h"
#include <stdbool.h>

#include "math/coord_frame.h"

// lang/vec3/vec3d.h — 32-byte SIMD d64-Precision 3D Spatial Vector.
//
// Single Class Per File Law: Vec3d.
//
// Ultra-precision 3D vector for celestial physics, gravitational simulations,
// and orbital integration. Aligned to 32 Bytes for AVX2 / Apple NEON pair.

typedef struct Vec3d {
    union {
        struct { d64 horizontal; d64 vertical; d64 depth; u64 frame; };
        struct { d64 x; d64 y; d64 z; u64 _frame; };
        struct { d64 right; d64 up; d64 front; u64 _f; };
        alignas(32) d64 data[4];
    };
} Vec3d;

#define VEC3D_BYTES 32u

Vec3d *Vec3d_0(void);
Vec3d *Vec3d_3(d64 horizontal, d64 vertical, d64 depth);
Vec3d *Vec3d_4(d64 horizontal, d64 vertical, d64 depth, CoordFrame frame);
#define Vec3d(...) CONSTRUCTOR_DISPATCH(Vec3d, __VA_ARGS__)
void Vec3d_free(Vec3d *v);

// Spatial Directional Getters & Setters
d64 Vec3d_getRight(const Vec3d *v);
void   Vec3d_setRight(Vec3d *v, d64 val);
d64 Vec3d_getLeft(const Vec3d *v);
void   Vec3d_setLeft(Vec3d *v, d64 val);

d64 Vec3d_getUp(const Vec3d *v);
void   Vec3d_setUp(Vec3d *v, d64 val);
d64 Vec3d_getDown(const Vec3d *v);
void   Vec3d_setDown(Vec3d *v, d64 val);

d64 Vec3d_getFront(const Vec3d *v);
void   Vec3d_setFront(Vec3d *v, d64 val);
d64 Vec3d_getBack(const Vec3d *v);
void   Vec3d_setBack(Vec3d *v, d64 val);

// Frame-mapped getters
d64 Vec3d_getX(const Vec3d *v);
d64 Vec3d_getY(const Vec3d *v);
d64 Vec3d_getZ(const Vec3d *v);

void Vec3d_add(const Vec3d *a, const Vec3d *b, Vec3d *dest);
void Vec3d_sub(const Vec3d *a, const Vec3d *b, Vec3d *dest);
void Vec3d_mul(const Vec3d *a, d64 scalar, Vec3d *dest);
d64 Vec3d_dot(const Vec3d *a, const Vec3d *b);
d64 Vec3d_length(const Vec3d *v);
void Vec3d_normalize(const Vec3d *src, Vec3d *dest);

#endif
