#include "lang/mat4.h"

#include "math/fast_math.h"
#include "nio/mem.h"
#include "oop/vexspoke_type.h"
#include "util/annotation.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Mat4
 * ============================================================================
 * 64-byte column-major 4x4 matrix class, ported from lang/Mat4.java. All
 * transforms (translation, scale, rotation, perspective, orthographic,
 * lookAt) write dest-last into caller-provided matrices; the single f32
 * array m[16] keeps the struct trivially copyable and SIMD-friendly.
 * Mat4_0 allocates through the memory arena with the TYPE_MAT4_SINGLETON
 * identity; every other function operates on caller-owned storage with zero
 * allocation. Getters/setters index column-major (m[col * 4 + row]).
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Mat4 (lang/mat4.c)
 * ============================================================================
 * the Mat4 class, ported from lang/Mat4.java.
 *
 * STRUCT FIELDS (Mirroring lang/mat4.h):
 * ----------------------------------------------------------------------------
 *   Mat4 {
 *     f32 m[16]; // column-major 4x4 matrix
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Mat4_0(void)
 *
 * Core Functions:
 *   - Mat4_identityAlloc(void)
 *   - Mat4_free(m)
 *   - Mat4_zero(m)
 *   - Mat4_identity(m)
 *   - Mat4_copy(src, dest)
 *   - Mat4_multiply(left, right, dest)
 *   - Mat4_transpose(src, dest)
 *   - Mat4_createTransformationMatrix(pos_x, pos_y, pos_z, rot_x_deg, rot_y_deg, rot_z_deg, scale_x, scale_y, scale_z, dest)
 *   - Mat4_createTransformationMatrix2D(pos_x, pos_y, rot_z_deg, scale_x, scale_y, dest)
 *   - Mat4_createViewMatrix(pos_x, pos_y, pos_z, pitch_deg, yaw_deg, roll_deg, dest)
 *   - Mat4_translate(src, tx, ty, tz, dest)
 *   - Mat4_scale(src, sx, sy, sz, dest)
 *   - Mat4_rotate(src, angle_radians, axis_x, axis_y, axis_z, dest)
 *   - Mat4_perspective(fov_y_radians, aspect, z_near, z_far, dest)
 *   - Mat4_perspectiveVulkan(fov_y_radians, aspect, z_near, z_far, dest)
 *   - Mat4_orthographic(left, right, bottom, top, z_near, z_far, dest)
 *   - Mat4_lookAt(eye_x, eye_y, eye_z, target_x, target_y, target_z, up_x, up_y, up_z, dest)
 *   - Mat4_transform(m, src_vec, dest_vec)
 *   - Mat4_transformVec3(m, src_vec, dest_vec)
 *
 * Setters:
 *   - Mat4_set(m, row, col, val)
 *   - Mat4_setRaw(m, index, val)
 *
 * Getters:
 *   - Mat4_get(m, row, col)
 *   - Mat4_getRaw(m, index)
 * ============================================================================
 */


// mat4.c — Mat4 port (Legacy: lang/Mat4.java). 64-byte column-major matrix.

Mat4 *Mat4_0(void) {
    Mat4 *m = Memory_alloc(TYPE_MAT4_SINGLETON, sizeof(Mat4));
    if (m)
        Mat4_identity(m);
    return m;
}

Mat4 *Mat4_identityAlloc(void) {
    return Mat4_0();
}

void Mat4_free(Mat4 *m) {
    Memory_free(m);
}

f32 Mat4_get(const Mat4 *m, int row, int col) {
    return (*m).m[col * 4 + row];
}

void Mat4_set(Mat4 *m, int row, int col, f32 val) {
    (*m).m[col * 4 + row] = val;
}

f32 Mat4_getRaw(const Mat4 *m, int index) {
    return (*m).m[index];
}

void Mat4_setRaw(Mat4 *m, int index, f32 val) {
    (*m).m[index] = val;
}

void Mat4_zero(Mat4 *m) {
    for (int i = 0; i < 16; i++)
        (*m).m[i] = 0.0f;
}

void Mat4_identity(Mat4 *m) {
    Mat4_zero(m);
    Mat4_set(m, 0, 0, 1.0f);
    Mat4_set(m, 1, 1, 1.0f);
    Mat4_set(m, 2, 2, 1.0f);
    Mat4_set(m, 3, 3, 1.0f);
}

void Mat4_copy(const Mat4 *src, Mat4 *dest) {
    for (int i = 0; i < 16; i++)
        (*dest).m[i] = (*src).m[i];
}

void Mat4_multiply(const Mat4 *left, const Mat4 *right, Mat4 *dest) {
    f32 m00 = Mat4_get(left, 0, 0), m01 = Mat4_get(left, 0, 1), m02 = Mat4_get(left, 0, 2), m03 = Mat4_get(left, 0, 3);
    f32 m10 = Mat4_get(left, 1, 0), m11 = Mat4_get(left, 1, 1), m12 = Mat4_get(left, 1, 2), m13 = Mat4_get(left, 1, 3);
    f32 m20 = Mat4_get(left, 2, 0), m21 = Mat4_get(left, 2, 1), m22 = Mat4_get(left, 2, 2), m23 = Mat4_get(left, 2, 3);
    f32 m30 = Mat4_get(left, 3, 0), m31 = Mat4_get(left, 3, 1), m32 = Mat4_get(left, 3, 2), m33 = Mat4_get(left, 3, 3);

    for (int col = 0; col < 4; col++) {
        f32 r0 = Mat4_get(right, 0, col);
        f32 r1 = Mat4_get(right, 1, col);
        f32 r2 = Mat4_get(right, 2, col);
        f32 r3 = Mat4_get(right, 3, col);

        Mat4_set(dest, 0, col, m00 * r0 + m01 * r1 + m02 * r2 + m03 * r3);
        Mat4_set(dest, 1, col, m10 * r0 + m11 * r1 + m12 * r2 + m13 * r3);
        Mat4_set(dest, 2, col, m20 * r0 + m21 * r1 + m22 * r2 + m23 * r3);
        Mat4_set(dest, 3, col, m30 * r0 + m31 * r1 + m32 * r2 + m33 * r3);
    }
}

void Mat4_transpose(const Mat4 *src, Mat4 *dest) {
    if (dest == src) {
        f32 t;
        t = (*dest).m[1];  (*dest).m[1]  = (*dest).m[4];  (*dest).m[4]  = t;
        t = (*dest).m[2];  (*dest).m[2]  = (*dest).m[8];  (*dest).m[8]  = t;
        t = (*dest).m[3];  (*dest).m[3]  = (*dest).m[12]; (*dest).m[12] = t;
        t = (*dest).m[6];  (*dest).m[6]  = (*dest).m[9];  (*dest).m[9]  = t;
        t = (*dest).m[7];  (*dest).m[7]  = (*dest).m[13]; (*dest).m[13] = t;
        t = (*dest).m[11]; (*dest).m[11] = (*dest).m[14]; (*dest).m[14] = t;
        return;
    }
    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 4; col++)
            Mat4_set(dest, col, row, Mat4_get(src, row, col));
}

void Mat4_createTransformationMatrix(f32 pos_x, f32 pos_y, f32 pos_z,
                                     f32 rot_x_deg, f32 rot_y_deg, f32 rot_z_deg,
                                     f32 scale_x, f32 scale_y, f32 scale_z, Mat4 *dest) {
    f32 rx = rot_x_deg * FAST_MATH_DEG_TO_RAD;
    f32 ry = rot_y_deg * FAST_MATH_DEG_TO_RAD;
    f32 rz = rot_z_deg * FAST_MATH_DEG_TO_RAD;

    f32 cx = FastMath_cos(rx);
    f32 sx = FastMath_sin(rx);
    f32 cy = FastMath_cos(ry);
    f32 sy = FastMath_sin(ry);
    f32 cz = FastMath_cos(rz);
    f32 sz = FastMath_sin(rz);

    f32 cycz = cy * cz;
    f32 cysz = cy * sz;

    Mat4_set(dest, 0, 0, cycz * scale_x);
    Mat4_set(dest, 1, 0, cysz * scale_x);
    Mat4_set(dest, 2, 0, -sy * scale_x);
    Mat4_set(dest, 3, 0, 0.0f);

    Mat4_set(dest, 0, 1, (sx * sy * cz - cx * sz) * scale_y);
    Mat4_set(dest, 1, 1, (sx * sy * sz + cx * cz) * scale_y);
    Mat4_set(dest, 2, 1, (sx * cy) * scale_y);
    Mat4_set(dest, 3, 1, 0.0f);

    Mat4_set(dest, 0, 2, (cx * sy * cz + sx * sz) * scale_z);
    Mat4_set(dest, 1, 2, (cx * sy * sz - sx * cz) * scale_z);
    Mat4_set(dest, 2, 2, (cx * cy) * scale_z);
    Mat4_set(dest, 3, 2, 0.0f);

    Mat4_set(dest, 0, 3, pos_x);
    Mat4_set(dest, 1, 3, pos_y);
    Mat4_set(dest, 2, 3, pos_z);
    Mat4_set(dest, 3, 3, 1.0f);
}

void Mat4_createTransformationMatrix2D(f32 pos_x, f32 pos_y,
                                       f32 rot_z_deg, f32 scale_x, f32 scale_y, Mat4 *dest) {
    f32 rz = rot_z_deg * FAST_MATH_DEG_TO_RAD;
    f32 cz = FastMath_cos(rz);
    f32 sz = FastMath_sin(rz);

    Mat4_zero(dest);
    Mat4_set(dest, 0, 0, cz * scale_x);
    Mat4_set(dest, 1, 0, sz * scale_x);
    Mat4_set(dest, 0, 1, -sz * scale_y);
    Mat4_set(dest, 1, 1, cz * scale_y);
    Mat4_set(dest, 2, 2, 1.0f);
    Mat4_set(dest, 3, 3, 1.0f);
    Mat4_set(dest, 0, 3, pos_x);
    Mat4_set(dest, 1, 3, pos_y);
}

void Mat4_createViewMatrix(f32 pos_x, f32 pos_y, f32 pos_z,
                           f32 pitch_deg, f32 yaw_deg, f32 roll_deg, Mat4 *dest) {
    (void)roll_deg;

    f32 pitch_rad = pitch_deg * FAST_MATH_DEG_TO_RAD;
    f32 yaw_rad = yaw_deg * FAST_MATH_DEG_TO_RAD;

    f32 cp = FastMath_cos(pitch_rad);
    f32 sp = FastMath_sin(pitch_rad);
    f32 cy = FastMath_cos(yaw_rad);
    f32 sy = FastMath_sin(yaw_rad);

    f32 m00 = cy,  m01 = sy * sp, m02 = sy * cp;
    f32 m10 = 0.0f, m11 = cp,     m12 = -sp;
    f32 m20 = -sy, m21 = cy * sp, m22 = cy * cp;

    Mat4_set(dest, 0, 0, m00); Mat4_set(dest, 0, 1, m01); Mat4_set(dest, 0, 2, m02); Mat4_set(dest, 0, 3, 0.0f);
    Mat4_set(dest, 1, 0, m10); Mat4_set(dest, 1, 1, m11); Mat4_set(dest, 1, 2, m12); Mat4_set(dest, 1, 3, 0.0f);
    Mat4_set(dest, 2, 0, m20); Mat4_set(dest, 2, 1, m21); Mat4_set(dest, 2, 2, m22); Mat4_set(dest, 2, 3, 0.0f);

    Mat4_set(dest, 3, 0, -(m00 * pos_x + m10 * pos_y + m20 * pos_z));
    Mat4_set(dest, 3, 1, -(m01 * pos_x + m11 * pos_y + m21 * pos_z));
    Mat4_set(dest, 3, 2, -(m02 * pos_x + m12 * pos_y + m22 * pos_z));
    Mat4_set(dest, 3, 3, 1.0f);
}

void Mat4_translate(const Mat4 *src, f32 tx, f32 ty, f32 tz, Mat4 *dest) {
    Mat4_copy(src, dest);
    f32 m00 = Mat4_get(src, 0, 0), m01 = Mat4_get(src, 0, 1), m02 = Mat4_get(src, 0, 2), m03 = Mat4_get(src, 0, 3);
    f32 m10 = Mat4_get(src, 1, 0), m11 = Mat4_get(src, 1, 1), m12 = Mat4_get(src, 1, 2), m13 = Mat4_get(src, 1, 3);
    f32 m20 = Mat4_get(src, 2, 0), m21 = Mat4_get(src, 2, 1), m22 = Mat4_get(src, 2, 2), m23 = Mat4_get(src, 2, 3);
    f32 m30 = Mat4_get(src, 3, 0), m31 = Mat4_get(src, 3, 1), m32 = Mat4_get(src, 3, 2), m33 = Mat4_get(src, 3, 3);

    Mat4_set(dest, 0, 3, m00 * tx + m01 * ty + m02 * tz + m03);
    Mat4_set(dest, 1, 3, m10 * tx + m11 * ty + m12 * tz + m13);
    Mat4_set(dest, 2, 3, m20 * tx + m21 * ty + m22 * tz + m23);
    Mat4_set(dest, 3, 3, m30 * tx + m31 * ty + m32 * tz + m33);
}

void Mat4_scale(const Mat4 *src, f32 sx, f32 sy, f32 sz, Mat4 *dest) {
    Mat4_copy(src, dest);
    for (int row = 0; row < 4; row++) {
        Mat4_set(dest, row, 0, Mat4_get(src, row, 0) * sx);
        Mat4_set(dest, row, 1, Mat4_get(src, row, 1) * sy);
        Mat4_set(dest, row, 2, Mat4_get(src, row, 2) * sz);
    }
}

void Mat4_rotate(const Mat4 *src, f32 angle_radians,
                 f32 axis_x, f32 axis_y, f32 axis_z, Mat4 *dest) {
    f32 len_sq = axis_x * axis_x + axis_y * axis_y + axis_z * axis_z;
    if (len_sq <= FAST_MATH_EPSILON)
        return;
    f32 inv_len = FastMath_invSqrt(len_sq);
    axis_x *= inv_len;
    axis_y *= inv_len;
    axis_z *= inv_len;

    f32 c = FastMath_cos(angle_radians);
    f32 s = FastMath_sin(angle_radians);
    f32 nc = 1.0f - c;

    f32 xy = axis_x * axis_y, yz = axis_y * axis_z, zx = axis_z * axis_x;
    f32 xs = axis_x * s,      ys = axis_y * s,      zs = axis_z * s;

    f32 r00 = axis_x * axis_x * nc + c;
    f32 r01 = xy * nc - zs;
    f32 r02 = zx * nc + ys;

    f32 r10 = xy * nc + zs;
    f32 r11 = axis_y * axis_y * nc + c;
    f32 r12 = yz * nc - xs;

    f32 r20 = zx * nc - ys;
    f32 r21 = yz * nc + xs;
    f32 r22 = axis_z * axis_z * nc + c;

    for (int row = 0; row < 4; row++) {
        f32 m0 = Mat4_get(src, row, 0);
        f32 m1 = Mat4_get(src, row, 1);
        f32 m2 = Mat4_get(src, row, 2);

        Mat4_set(dest, row, 0, m0 * r00 + m1 * r10 + m2 * r20);
        Mat4_set(dest, row, 1, m0 * r01 + m1 * r11 + m2 * r21);
        Mat4_set(dest, row, 2, m0 * r02 + m1 * r12 + m2 * r22);
        // carry the translation column unchanged (a rotation has no translation,
        // but src's column 3 must survive into a non-aliased dest)
        Mat4_set(dest, row, 3, Mat4_get(src, row, 3));
    }
}

void Mat4_perspective(f32 fov_y_radians, f32 aspect, f32 z_near, f32 z_far, Mat4 *dest) {
    Mat4_zero(dest);
    f32 tan_half_fov = FastMath_tan(fov_y_radians / 2.0f);

    Mat4_set(dest, 0, 0, 1.0f / (aspect * tan_half_fov));
    Mat4_set(dest, 1, 1, 1.0f / tan_half_fov);
    Mat4_set(dest, 2, 2, -(z_far + z_near) / (z_far - z_near));
    Mat4_set(dest, 2, 3, -(2.0f * z_far * z_near) / (z_far - z_near));
    Mat4_set(dest, 3, 2, -1.0f);
}

void Mat4_perspectiveVulkan(f32 fov_y_radians, f32 aspect, f32 z_near, f32 z_far, Mat4 *dest) {
    Mat4_perspective(fov_y_radians, aspect, z_near, z_far, dest);
    Mat4_set(dest, 1, 1, -Mat4_get(dest, 1, 1));
    Mat4_set(dest, 2, 2, z_far / (z_near - z_far));
    Mat4_set(dest, 2, 3, (z_near * z_far) / (z_near - z_far));
}

void Mat4_orthographic(f32 left, f32 right, f32 bottom, f32 top,
                       f32 z_near, f32 z_far, Mat4 *dest) {
    Mat4_zero(dest);
    Mat4_set(dest, 0, 0, 2.0f / (right - left));
    Mat4_set(dest, 1, 1, 2.0f / (top - bottom));
    Mat4_set(dest, 2, 2, -2.0f / (z_far - z_near));
    Mat4_set(dest, 0, 3, -(right + left) / (right - left));
    Mat4_set(dest, 1, 3, -(top + bottom) / (top - bottom));
    Mat4_set(dest, 2, 3, -(z_far + z_near) / (z_far - z_near));
    Mat4_set(dest, 3, 3, 1.0f);
}

void Mat4_lookAt(f32 eye_x, f32 eye_y, f32 eye_z,
                 f32 target_x, f32 target_y, f32 target_z,
                 f32 up_x, f32 up_y, f32 up_z, Mat4 *dest) {
    f32 fx = eye_x - target_x;
    f32 fy = eye_y - target_y;
    f32 fz = eye_z - target_z;
    f32 f_len_sq = fx * fx + fy * fy + fz * fz;
    if (f_len_sq > FAST_MATH_EPSILON) {
        f32 inv_f = FastMath_invSqrt(f_len_sq);
        fx *= inv_f;
        fy *= inv_f;
        fz *= inv_f;
    }

    f32 rx = up_y * fz - up_z * fy;
    f32 ry = up_z * fx - up_x * fz;
    f32 rz = up_x * fy - up_y * fx;
    f32 r_len_sq = rx * rx + ry * ry + rz * rz;
    if (r_len_sq > FAST_MATH_EPSILON) {
        f32 inv_r = FastMath_invSqrt(r_len_sq);
        rx *= inv_r;
        ry *= inv_r;
        rz *= inv_r;
    }

    f32 ux = fy * rz - fz * ry;
    f32 uy = fz * rx - fx * rz;
    f32 uz = fx * ry - fy * rx;

    Mat4_zero(dest);
    Mat4_set(dest, 0, 0, rx); Mat4_set(dest, 0, 1, ry); Mat4_set(dest, 0, 2, rz); Mat4_set(dest, 0, 3, -(rx * eye_x + ry * eye_y + rz * eye_z));
    Mat4_set(dest, 1, 0, ux); Mat4_set(dest, 1, 1, uy); Mat4_set(dest, 1, 2, uz); Mat4_set(dest, 1, 3, -(ux * eye_x + uy * eye_y + uz * eye_z));
    Mat4_set(dest, 2, 0, fx); Mat4_set(dest, 2, 1, fy); Mat4_set(dest, 2, 2, fz); Mat4_set(dest, 2, 3, -(fx * eye_x + fy * eye_y + fz * eye_z));
    Mat4_set(dest, 3, 3, 1.0f);
}

void Mat4_transform(const Mat4 *m, const Coord4 *src_vec, Coord4 *dest_vec) {
    f32 x = (*src_vec).x;
    f32 y = (*src_vec).y;
    f32 z = (*src_vec).z;
    f32 w = (*src_vec).w;

    f32 rx = Mat4_get(m, 0, 0) * x + Mat4_get(m, 0, 1) * y + Mat4_get(m, 0, 2) * z + Mat4_get(m, 0, 3) * w;
    f32 ry = Mat4_get(m, 1, 0) * x + Mat4_get(m, 1, 1) * y + Mat4_get(m, 1, 2) * z + Mat4_get(m, 1, 3) * w;
    f32 rz = Mat4_get(m, 2, 0) * x + Mat4_get(m, 2, 1) * y + Mat4_get(m, 2, 2) * z + Mat4_get(m, 2, 3) * w;
    f32 rw = Mat4_get(m, 3, 0) * x + Mat4_get(m, 3, 1) * y + Mat4_get(m, 3, 2) * z + Mat4_get(m, 3, 3) * w;

    (*dest_vec).x = rx;
    (*dest_vec).y = ry;
    (*dest_vec).z = rz;
    (*dest_vec).w = rw;
}

void Mat4_transformVec3(const Mat4 *m, const Coord3 *src_vec, Coord3 *dest_vec) {
    f32 x = (*src_vec).x;
    f32 y = (*src_vec).y;
    f32 z = (*src_vec).z;

    f32 rx = Mat4_get(m, 0, 0) * x + Mat4_get(m, 0, 1) * y + Mat4_get(m, 0, 2) * z + Mat4_get(m, 0, 3);
    f32 ry = Mat4_get(m, 1, 0) * x + Mat4_get(m, 1, 1) * y + Mat4_get(m, 1, 2) * z + Mat4_get(m, 1, 3);
    f32 rz = Mat4_get(m, 2, 0) * x + Mat4_get(m, 2, 1) * y + Mat4_get(m, 2, 2) * z + Mat4_get(m, 2, 3);

    if (dest_vec) {
        (*dest_vec).x = rx;
        (*dest_vec).y = ry;
        (*dest_vec).z = rz;
        (*dest_vec).frame = (*src_vec).frame;   // a transform does not change the frame
    }
}
