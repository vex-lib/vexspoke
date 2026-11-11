#include "lang/constructor.h"
#ifndef LANG_MAT4_H
#define LANG_MAT4_H

#include <stddef.h>

#include "lang/vec3.h"
#include "lang/vec4.h"

// lang/mat4.h — the Mat4 class, ported from lang/Mat4.java.
//
// Off-heap 4x4 matrix, column-major, 16 contiguous f32s (64 Bytes) as a
// self-describing memory block. Column-major matches the Vulkan/OpenGL layout:
//
//     m[col * 4 + row]
//     col 0: 0..3, col 1: 4..7, col 2: 8..11, col 3: 12..15

typedef struct Mat4 {
    f32 m[16];
} Mat4;

// Fixed byte width of a Mat4 payload (legacy BYTES).
#define MAT4_BYTES 64u

// Allocate a Mat4 block, initialized to identity (or zeroed via Mat4_zero).
// nullptr on OOM.
Mat4 *Mat4_0(void);
Mat4 *Mat4_identityAlloc(void);

void Mat4_free(Mat4 *m);

// (row, col) access into the column-major layout.
f32 Mat4_get(const Mat4 *m, int row, int col);
void Mat4_set(Mat4 *m, int row, int col, f32 val);

// Raw linear access: index is col * 4 + row.
f32 Mat4_getRaw(const Mat4 *m, int index);
void Mat4_setRaw(Mat4 *m, int index, f32 val);

void Mat4_zero(Mat4 *m);
void Mat4_identity(Mat4 *m);
void Mat4_copy(const Mat4 *src, Mat4 *dest);

// dest = left * right. Safe when dest aliases left or right.
void Mat4_multiply(const Mat4 *left, const Mat4 *right, Mat4 *dest);

// Transpose in place or _out of place; safe when dest aliases src.
void Mat4_transpose(const Mat4 *src, Mat4 *dest);

// Single-pass TRS synthesis (column-major, 21 multiplies). Angles in degrees.
void Mat4_createTransformationMatrix(f32 pos_x, f32 pos_y, f32 pos_z,
                                     f32 rot_x_deg, f32 rot_y_deg, f32 rot_z_deg,
                                     f32 scale_x, f32 scale_y, f32 scale_z, Mat4 *dest);

// Single-pass 2D TRS synthesis. Angles in degrees.
void Mat4_createTransformationMatrix2D(f32 pos_x, f32 pos_y,
                                       f32 rot_z_deg, f32 scale_x, f32 scale_y, Mat4 *dest);

// Left-handed FPS camera view matrix. Angles in degrees.
void Mat4_createViewMatrix(f32 pos_x, f32 pos_y, f32 pos_z,
                           f32 pitch_deg, f32 yaw_deg, f32 roll_deg, Mat4 *dest);

void Mat4_translate(const Mat4 *src, f32 tx, f32 ty, f32 tz, Mat4 *dest);
void Mat4_scale(const Mat4 *src, f32 sx, f32 sy, f32 sz, Mat4 *dest);
void Mat4_rotate(const Mat4 *src, f32 angle_radians,
                 f32 axis_x, f32 axis_y, f32 axis_z, Mat4 *dest);

void Mat4_perspective(f32 fov_y_radians, f32 aspect, f32 z_near, f32 z_far, Mat4 *dest);

// Perspective with Vulkan clip conventions (inverted Y, [0,1] depth).
void Mat4_perspectiveVulkan(f32 fov_y_radians, f32 aspect, f32 z_near, f32 z_far, Mat4 *dest);

void Mat4_orthographic(f32 left, f32 right, f32 bottom, f32 top,
                       f32 z_near, f32 z_far, Mat4 *dest);

void Mat4_lookAt(f32 eye_x, f32 eye_y, f32 eye_z,
                 f32 target_x, f32 target_y, f32 target_z,
                 f32 up_x, f32 up_y, f32 up_z, Mat4 *dest);

// dest = mat * src (homogeneous 4-vector).
void Mat4_transform(const Mat4 *m, const Coord4 *src_vec, Coord4 *dest_vec);

// dest = mat * (src, 1) treating src as a 3-vector.
void Mat4_transformVec3(const Mat4 *m, const Coord3 *src_vec, Coord3 *dest_vec);

#endif
#define Mat4(...) CONSTRUCTOR_DISPATCH(Mat4, __VA_ARGS__)
