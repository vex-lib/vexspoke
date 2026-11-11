#ifndef LANG_VEC4_VEC4D_H
#define LANG_VEC4_VEC4D_H

#include <stddef.h>
#include <stdint.h>

#include "lang/constructor.h"

// lang/vec4/vec4d.h — 32-byte SIMD 4D d64-Precision Vector.
//
// Single Class Per File Law: Vec4d.

typedef struct Vec4d {
    union {
        struct { d64 horizontal; d64 vertical; d64 depth; d64 w; };
        struct { d64 x; d64 y; d64 z; d64 _w; };
        struct { d64 right; d64 up; d64 front; d64 _w2; };
        alignas(32) d64 data[4];
    };
} Vec4d;

#define VEC4D_BYTES 32u

Vec4d *Vec4d_0(void);
Vec4d *Vec4d_4(d64 horizontal, d64 vertical, d64 depth, d64 w);
#define Vec4d(...) CONSTRUCTOR_DISPATCH(Vec4d, __VA_ARGS__)
void Vec4d_free(Vec4d *v);

d64 Vec4d_getRight(const Vec4d *v);
void   Vec4d_setRight(Vec4d *v, d64 val);
d64 Vec4d_getLeft(const Vec4d *v);
void   Vec4d_setLeft(Vec4d *v, d64 val);

d64 Vec4d_getUp(const Vec4d *v);
void   Vec4d_setUp(Vec4d *v, d64 val);
d64 Vec4d_getDown(const Vec4d *v);
void   Vec4d_setDown(Vec4d *v, d64 val);

d64 Vec4d_getFront(const Vec4d *v);
void   Vec4d_setFront(Vec4d *v, d64 val);
d64 Vec4d_getBack(const Vec4d *v);
void   Vec4d_setBack(Vec4d *v, d64 val);

void Vec4d_add(const Vec4d *a, const Vec4d *b, Vec4d *dest);
void Vec4d_sub(const Vec4d *a, const Vec4d *b, Vec4d *dest);
void Vec4d_mul(const Vec4d *a, d64 scalar, Vec4d *dest);
d64 Vec4d_dot(const Vec4d *a, const Vec4d *b);
d64 Vec4d_length(const Vec4d *v);

#endif
