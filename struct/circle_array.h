#ifndef STRUCT_CIRCLE_ARRAY_H
#define STRUCT_CIRCLE_ARRAY_H

#include "vex.h"

#include "struct/collection.h"

// struct/circle_array.h — 2D Circular Matrix evaluated via the Pythagorean Theorem.
//
// Represents a 2D circle bounded by a square matrix of size (2*radius + 1)^2.
// Valid cells satisfy: (gridX - radius)^2 + (gridY - radius)^2 <= radius^2.
// Alternatively accessible via center-relative offsets (dx, dy) where dx^2 + dy^2 <= radius^2.

typedef struct CircleArray {
    Collection collection;   // Base collection header (typeId, activeCount, etc.)
    i32    radius;       // Radius in cells
    i32    diameter;     // 2 * radius + 1
} CircleArray;

// Allocation: creates a CircleArray with radius and element class.
CircleArray *CircleArray_2(i32 radius, u32 elementClass);
CircleArray *CircleArray_3(i32 radius, u32 elementClass, bytes stride);
#define CircleArray(...) CONSTRUCTOR_DISPATCH(CircleArray, __VA_ARGS__)
void CircleArray_free(CircleArray *self);

i32 CircleArray_radius(const CircleArray *self);
i32 CircleArray_diameter(const CircleArray *self);
bytes  CircleArray_validCellCount(const CircleArray *self);

// Pythagorean containment tests
bool CircleArray_containsGrid(const CircleArray *self, i32 gridX, i32 gridY);
bool CircleArray_containsOffset(const CircleArray *self, i32 dx, i32 dy);
int64_t CircleArray_distanceSquaredGrid(const CircleArray *self, i32 gridX, i32 gridY);
int64_t CircleArray_distanceSquaredOffset(i32 dx, i32 dy);

// Cell access by grid coordinates [0, diameter - 1] (get output first, set input last)
bool CircleArray_getGrid(any dest, const CircleArray *self, i32 gridX, i32 gridY);
bool CircleArray_setGrid(CircleArray *self, i32 gridX, i32 gridY, const any src);

// Cell access by center-relative offsets [-radius, radius]
bool CircleArray_getOffset(any dest, const CircleArray *self, i32 dx, i32 dy);
bool CircleArray_setOffset(CircleArray *self, i32 dx, i32 dy, const any src);

// Raw pointer to cell slot inside circular matrix (nullptr if outside circle or bounds)
u8 *CircleArray_slotGrid(CircleArray *self, i32 gridX, i32 gridY);
u8 *CircleArray_slotOffset(CircleArray *self, i32 dx, i32 dy);

// Iteration callback across all valid circular cells
typedef void (*CircleArrayCellFn)(i32 gridX, i32 gridY, i32 dx, i32 dy, const any element, any userData);
void CircleArray_forEach(const CircleArray *self, CircleArrayCellFn callback, any userData);

#endif
