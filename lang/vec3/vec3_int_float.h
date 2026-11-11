#ifndef LANG_VEC3_VEC3_INT_FLOAT_H
#define LANG_VEC3_VEC3_INT_FLOAT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "lang/constructor.h"

#include "math/coord_frame.h"

// lang/vec3/vec3_int_f32.h — Large-World 3D Vector (int32 sector + f32 local).
//
// Single Class Per File Law: Vec3IntFloat.
//
// Eliminates 32-bit f32ing-point jitter at large distances by pairing
// an integer grid tile / sector with a normalized local offset per axis.

typedef struct CoordIntFloat {
    i32 sector;
    f32 local;
} CoordIntFloat;

typedef struct Vec3IntFloat {
    CoordIntFloat horizontal;
    CoordIntFloat vertical;
    CoordIntFloat depth;
    u32 frame;
} Vec3IntFloat;

Vec3IntFloat *Vec3IntFloat_0(void);
Vec3IntFloat *Vec3IntFloat_7(i32 sectorH, f32 localH, i32 sectorV, f32 localV,
                             i32 sectorD, f32 localD, CoordFrame frame);
#define Vec3IntFloat(...) CONSTRUCTOR_DISPATCH(Vec3IntFloat, __VA_ARGS__)
void Vec3IntFloat_free(Vec3IntFloat *v);

// Normalize sector overflow: when |local| >= sectorSize, bump sector
void Vec3IntFloat_rebalance(Vec3IntFloat *v, f32 sectorSize);

// Directional queries
f32 Vec3IntFloat_getRightLocal(const Vec3IntFloat *v);
f32 Vec3IntFloat_getLeftLocal(const Vec3IntFloat *v);
f32 Vec3IntFloat_getUpLocal(const Vec3IntFloat *v);
f32 Vec3IntFloat_getDownLocal(const Vec3IntFloat *v);
f32 Vec3IntFloat_getFrontLocal(const Vec3IntFloat *v);
f32 Vec3IntFloat_getBackLocal(const Vec3IntFloat *v);

#endif
