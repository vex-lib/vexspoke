#ifndef LANG_VEC4_VEC4_H
#define LANG_VEC4_VEC4_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "lang/constructor.h"
#include "math/coord_frame.h"

// lang/vec4/vec4.h — 16-byte SIMD Spatial 4D / Homogeneous Vector.
//
// Single Class Per File Law: Coord4.
//
// Represents a 4D spatial / projective / quaternion vector with horizontal,
// vertical, depth, and w components. Preserves 16-byte SIMD alignment.

typedef struct Coord4 {
    union {
        struct { f32 horizontal; f32 vertical; f32 depth; f32 w; };
        struct { f32 x; f32 y; f32 z; f32 _w; };
        struct { f32 right; f32 up; f32 front; f32 _w2; };
        struct { f32 r; f32 g; f32 b; f32 a; };
        alignas(16) f32 data[4];
    };
} Coord4;

#define VEC4_BYTES 16u

Coord4 *Coord4_0(void);
Coord4 *Coord4_4(f32 horizontal, f32 vertical, f32 depth, f32 w);
#define Coord4(...) CONSTRUCTOR_DISPATCH(Coord4, __VA_ARGS__)
void Coord4_free(Coord4 *v);

// Spatial Directional Getters & Setters (w is untouched)
f32 Coord4_getRight(const Coord4 *v);
void  Coord4_setRight(Coord4 *v, f32 val);
f32 Coord4_getLeft(const Coord4 *v);
void  Coord4_setLeft(Coord4 *v, f32 val);

f32 Coord4_getUp(const Coord4 *v);
void  Coord4_setUp(Coord4 *v, f32 val);
f32 Coord4_getDown(const Coord4 *v);
void  Coord4_setDown(Coord4 *v, f32 val);

f32 Coord4_getFront(const Coord4 *v);
void  Coord4_setFront(Coord4 *v, f32 val);
f32 Coord4_getBack(const Coord4 *v);
void  Coord4_setBack(Coord4 *v, f32 val);

f32 Coord4_getX(const Coord4 *v);
void  Coord4_setX(Coord4 *v, f32 x);
f32 Coord4_getY(const Coord4 *v);
void  Coord4_setY(Coord4 *v, f32 y);
f32 Coord4_getZ(const Coord4 *v);
void  Coord4_setZ(Coord4 *v, f32 z);
f32 Coord4_getW(const Coord4 *v);
void  Coord4_setW(Coord4 *v, f32 w);

f32 Coord4_getXInFrame(const Coord4 *v, CoordFrame frame);
f32 Coord4_getYInFrame(const Coord4 *v, CoordFrame frame);
f32 Coord4_getZInFrame(const Coord4 *v, CoordFrame frame);

void Coord4_set(Coord4 *v, f32 horizontal, f32 vertical, f32 depth, f32 w);
void Coord4_copy(const Coord4 *src, Coord4 *dest);

void Coord4_add(const Coord4 *a, const Coord4 *b, Coord4 *dest);
void Coord4_sub(const Coord4 *a, const Coord4 *b, Coord4 *dest);
void Coord4_mul(const Coord4 *a, f32 scalar, Coord4 *dest);
void Coord4_div(const Coord4 *a, f32 scalar, Coord4 *dest);

f32 Coord4_dot(const Coord4 *a, const Coord4 *b);
f32 Coord4_lengthSquared(const Coord4 *v);
f32 Coord4_length(const Coord4 *v);
void  Coord4_normalize(const Coord4 *src, Coord4 *dest);

void Coord4_lerp(const Coord4 *a, const Coord4 *b, f32 t, Coord4 *dest);

#endif
