#ifndef LANG_VEC3_VEC3_H
#define LANG_VEC3_VEC3_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "util/constructor.h"
#include "math/coord_frame.h"

// lang/vec3/vec3.h — 16-byte SIMD Spatial 3D Vector.
//
// Single Class Per File Law: Coord3.
//
// Represents a coordinate-agnostic 3D spatial vector with horizontal, vertical,
// and depth dimensions, carrying its CoordFrame as true embedded metadata in
// the 4th float slot. Preserves 16-byte SIMD alignment for NEON / SSE.
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
        struct { float horizontal; float vertical; float depth; uint32_t frame; };
        struct { float x; float y; float z; uint32_t _frame; };
        struct { float right; float up; float front; uint32_t _f; };
        alignas(16) float data[4];
    };
} Coord3;

#define VEC3_BYTES 16u

// Constructors
Coord3 *Coord3_0(void);
Coord3 *Coord3_3(float horizontal, float vertical, float depth);
Coord3 *Coord3_4(float horizontal, float vertical, float depth, CoordFrame frame);
#define Coord3(...) CONSTRUCTOR_DISPATCH(Coord3, __VA_ARGS__)
void Coord3_free(Coord3 *v);

// Spatial Directional Getters & Setters
float Coord3_getRight(const Coord3 *v);
void Coord3_setRight(Coord3 *v, float val);
float Coord3_getLeft(const Coord3 *v);
void Coord3_setLeft(Coord3 *v, float val);

float Coord3_getUp(const Coord3 *v);
void Coord3_setUp(Coord3 *v, float val);
float Coord3_getDown(const Coord3 *v);
void Coord3_setDown(Coord3 *v, float val);

float Coord3_getFront(const Coord3 *v);
void Coord3_setFront(Coord3 *v, float val);
float Coord3_getBack(const Coord3 *v);
void Coord3_setBack(Coord3 *v, float val);

// Frame Metadata Accessors
CoordFrame Coord3_getFrame(const Coord3 *v);
void Coord3_setFrame(Coord3 *v, CoordFrame frame);

// Frame-mapped One-Line Coordinate Accessors
float Coord3_getX(const Coord3 *v);
void Coord3_setX(Coord3 *v, float x);
float Coord3_getY(const Coord3 *v);
void Coord3_setY(Coord3 *v, float y);
float Coord3_getZ(const Coord3 *v);
void Coord3_setZ(Coord3 *v, float z);

float Coord3_getXInFrame(const Coord3 *v, CoordFrame frame);
float Coord3_getYInFrame(const Coord3 *v, CoordFrame frame);
float Coord3_getZInFrame(const Coord3 *v, CoordFrame frame);

void Coord3_set(Coord3 *v, float horizontal, float vertical, float depth);
void Coord3_copy(const Coord3 *src, Coord3 *dest);

// Coordinate Frame Transform
void Coord3_toFrame(const Coord3 *src, CoordFrame targetFrame, Coord3 *dest);

// Vector Arithmetic
void Coord3_add(const Coord3 *a, const Coord3 *b, Coord3 *dest);
void Coord3_sub(const Coord3 *a, const Coord3 *b, Coord3 *dest);
void Coord3_mul(const Coord3 *a, float scalar, Coord3 *dest);
void Coord3_div(const Coord3 *a, float scalar, Coord3 *dest);

float Coord3_dot(const Coord3 *a, const Coord3 *b);
void Coord3_cross(const Coord3 *a, const Coord3 *b, Coord3 *dest);

float Coord3_lengthSquared(const Coord3 *v);
float Coord3_length(const Coord3 *v);

// Normalization: Strict IEEE (1/sqrt) vs Fast Relaxed (Quake invSqrt)
void Coord3_normalize(const Coord3 *src, Coord3 *dest);
void Coord3_fastNormalize(const Coord3 *src, Coord3 *dest);

float Coord3_distance(const Coord3 *a, const Coord3 *b);
float Coord3_angle(const Coord3 *a, const Coord3 *b);

void Coord3_project(const Coord3 *vector, const Coord3 *onto, Coord3 *dest);
void Coord3_reflect(const Coord3 *incident, const Coord3 *normal, Coord3 *dest);
void Coord3_clamp(const Coord3 *src, float min_val, float max_val, Coord3 *dest);
void Coord3_abs(const Coord3 *src, Coord3 *dest);
void Coord3_lerp(const Coord3 *a, const Coord3 *b, float t, Coord3 *dest);

#endif
