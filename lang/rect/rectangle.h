#ifndef LANG_RECT_RECTANGLE_H
#define LANG_RECT_RECTANGLE_H

#include "vex.h"

// lang/rect/rectangle.h — Pixel-Space Rectangle.
//
// Single Class Per File Law: Rectangle.
//
// Axis-aligned rectangle in native pixel space (the Pixel Coordinate
// Contract: the Graphics API, layout buffers and hit-testing all speak
// native hardware pixels; -1..1 NDC exists only inside vertex shaders).
// Floats allow sub-pixel precision (crisp-edge snapping happens in the
// raster backends); integer positions use the Point class.
//
// Screen convention: +Y is DOWN (top-left origin, Y-down), matching
// CoreAnimation/Vulkan/Direct drawables. x/y are the TOP-LEFT corner.
// An empty rectangle is width <= 0 or height <= 0.

typedef struct Rectangle {
    f32 x;       // top-left x, native pixels
    f32 y;       // top-left y, native pixels (Y-down)
    f32 width;   // extent along +X, native pixels
    f32 height;  // extent along +Y, native pixels
} Rectangle;

#define RECTANGLE_BYTES 16u

// Constructors:
//   Rectangle()                 — empty at origin
//   Rectangle(x, y, w, h)       — explicit rectangle bounds
Rectangle *Rectangle_0(void);
Rectangle *Rectangle_4(f32 x, f32 y, f32 width, f32 height);
#define Rectangle(...) CONSTRUCTOR_DISPATCH(Rectangle, __VA_ARGS__)
void Rectangle_free(Rectangle *r);

// Setters
void Rectangle_setX(Rectangle *r, f32 x);
void Rectangle_setY(Rectangle *r, f32 y);
void Rectangle_setWidth(Rectangle *r, f32 width);
void Rectangle_setHeight(Rectangle *r, f32 height);
void Rectangle_set(Rectangle *r, f32 x, f32 y, f32 width, f32 height);

// Getters (null-safe: 0 on null)
f32 Rectangle_getX(const Rectangle *r);
f32 Rectangle_getY(const Rectangle *r);
f32 Rectangle_getWidth(const Rectangle *r);
f32 Rectangle_getHeight(const Rectangle *r);
f32 Rectangle_getLeft(const Rectangle *r);
f32 Rectangle_getTop(const Rectangle *r);
f32 Rectangle_getRight(const Rectangle *r);
f32 Rectangle_getBottom(const Rectangle *r);

// Core functions (dest-last: (srcA, srcB, dest))
void Rectangle_copy(const Rectangle *src, Rectangle *dest);
void Rectangle_translate(const Rectangle *src, f32 dx, f32 dy, Rectangle *dest);
void Rectangle_inflate(const Rectangle *src, f32 dx, f32 dy, Rectangle *dest);
void Rectangle_inset(const Rectangle *src, f32 l, f32 t, f32 r, f32 b, Rectangle *dest);
void Rectangle_intersection(const Rectangle *a, const Rectangle *b, Rectangle *dest);
void Rectangle_merge(const Rectangle *a, const Rectangle *b, Rectangle *dest);

// Predicates (null-safe: false on null)
bool Rectangle_isEmpty(const Rectangle *r);
bool Rectangle_containsPoint(const Rectangle *r, f32 px, f32 py);
bool Rectangle_containsRect(const Rectangle *outer, const Rectangle *inner);
bool Rectangle_intersects(const Rectangle *a, const Rectangle *b);
bool Rectangle_equals(const Rectangle *a, const Rectangle *b);

#endif
