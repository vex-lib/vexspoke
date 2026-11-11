#ifndef LANG_VEC2_VEC2_H
#define LANG_VEC2_VEC2_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "lang/constructor.h"

// lang/vec2/vec2.h — 2D Spatial Vector.
//
// Single Class Per File Law: Coord2.
//
// 2D spatial vector with horizontal and vertical dimensions.
//   - horizontal (x): Right (+), Left (-)
//   - vertical   (y): Up (+),    Down (-)
//
// Direct directional inverses:
//   getLeft() == -getRight()
//   getDown() == -getUp()

typedef enum CoordFrame2D {
    COORD_FRAME_2D_Y_UP = 0,   // Cartesian math (+Y Up, +X Right)
    COORD_FRAME_2D_Y_DOWN = 1  // UI / Canvas / Screen (+Y Down, +X Right)
} CoordFrame2D;

typedef struct Coord2 {
    union {
        struct { f32 horizontal; f32 vertical; };
        struct { f32 x; f32 y; };
        struct { f32 right; f32 up; };
        f32 data[2];
    };
} Coord2;

#define VEC2_BYTES 8u

Coord2 *Coord2_0(void);
Coord2 *Coord2_2(f32 horizontal, f32 vertical);
#define Coord2(...) CONSTRUCTOR_DISPATCH(Coord2, __VA_ARGS__)
void Coord2_free(Coord2 *v);

// Directional Getters & Setters
f32 Coord2_getRight(const Coord2 *v);
void  Coord2_setRight(Coord2 *v, f32 val);
f32 Coord2_getLeft(const Coord2 *v);
void  Coord2_setLeft(Coord2 *v, f32 val);

f32 Coord2_getUp(const Coord2 *v);
void  Coord2_setUp(Coord2 *v, f32 val);
f32 Coord2_getDown(const Coord2 *v);
void  Coord2_setDown(Coord2 *v, f32 val);

f32 Coord2_getX(const Coord2 *v);
void  Coord2_setX(Coord2 *v, f32 x);
f32 Coord2_getY(const Coord2 *v);
void  Coord2_setY(Coord2 *v, f32 y);

f32 Coord2_getYInFrame(const Coord2 *v, CoordFrame2D frame);

void Coord2_set(Coord2 *v, f32 horizontal, f32 vertical);
void Coord2_copy(const Coord2 *src, Coord2 *dest);

void Coord2_add(const Coord2 *a, const Coord2 *b, Coord2 *dest);
void Coord2_sub(const Coord2 *a, const Coord2 *b, Coord2 *dest);
void Coord2_mul(const Coord2 *a, f32 scalar, Coord2 *dest);
void Coord2_div(const Coord2 *a, f32 scalar, Coord2 *dest);

f32 Coord2_dot(const Coord2 *a, const Coord2 *b);
f32 Coord2_lengthSquared(const Coord2 *v);
f32 Coord2_length(const Coord2 *v);
void  Coord2_normalize(const Coord2 *src, Coord2 *dest);
void  Coord2_perpendicular(const Coord2 *src, Coord2 *dest);
f32 Coord2_distance(const Coord2 *a, const Coord2 *b);
f32 Coord2_angle(const Coord2 *a, const Coord2 *b);
void  Coord2_lerp(const Coord2 *a, const Coord2 *b, f32 t, Coord2 *dest);

#endif
