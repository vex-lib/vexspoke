#ifndef ALGO_SPATIAL_H
#define ALGO_SPATIAL_H

#include <stdint.h>

// algo/spatial.h — compact spatial indexing helpers.

// Interleave the low 21 bits of each coordinate into a 63-bit Morton code.
u64 Spatial_morton3D(u32 x, u32 y, u32 z);

#endif
