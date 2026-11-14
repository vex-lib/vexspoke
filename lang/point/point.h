#ifndef LANG_POINT_POINT_H
#define LANG_POINT_POINT_H

#include "vex.h"

// lang/point/point.h — Integer Pixel Point.
//
// Single Class Per File Law: Point.
//
// Integer-precision 2D position in native pixel space (the Pixel Coordinate
// Contract: the Graphics API, layout buffers and hit-testing all speak
// native hardware pixels; -1..1 NDC exists only inside vertex shaders).
// i32 members give element-wise accuracy for pixel snapping, software
// raster ops and integer geometry where f32s buy nothing.
//
// Screen convention: +Y is DOWN in UI pixel space (the CoordFrame2D
// Y-down frame; Point does not carry a frame flag — pixel space is always
// top-left origin, matching CoreAnimation/Vulkan/Direct drawables).

typedef struct Point {
    i32 x;  // horizontal pixel position (native pixels)
    i32 y;  // vertical pixel position (native pixels, Y-down)
} Point;

#define POINT_BYTES 8u

// Constructors:
//   Point()        — origin (0, 0)
//   Point(x, y)    — explicit pixel position
Point *Point_0();
Point *Point_2(i32 x, i32 y);
#define Point(...) CONSTRUCTOR_DISPATCH(Point, __VA_ARGS__)
fn Point_free(Point *p);

// Setters
fn Point_setX(Point *p, i32 x);
fn Point_setY(Point *p, i32 y);
fn Point_set(Point *p, i32 x, i32 y);

// Getters (null-safe: 0 on null)
i32 Point_getX(const Point *p);
i32 Point_getY(const Point *p);

// Core functions (dest-last: (srcA, srcB, dest))
fn Point_copy(const Point *src, Point *dest);
fn Point_add(const Point *a, const Point *b, Point *dest);
fn Point_sub(const Point *a, const Point *b, Point *dest);
fn Point_scale(const Point *a, i32 scalar, Point *dest);
fn Point_negate(const Point *src, Point *dest);
fn Point_min(const Point *a, const Point *b, Point *dest);
fn Point_max(const Point *a, const Point *b, Point *dest);

// Metrics (null-safe: 0 / false on null)
int64_t Point_distanceSquared(const Point *a, const Point *b);
int64_t Point_manhattanDistance(const Point *a, const Point *b);
d64 Point_distance(const Point *a, const Point *b);
bool Point_equals(const Point *a, const Point *b);

#endif
