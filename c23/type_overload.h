#ifndef C23_TYPE_OVERLOAD_H
#define C23_TYPE_OVERLOAD_H

#include "lang/mat4.h"
#include "lang/vec2.h"
#include "lang/vec3.h"
#include "lang/vec4.h"

// c23/overload.h — type-based overloading (the other half of constructor.h).
//
// One name, many types: add/sub/mul/div dispatch on operand TYPES via _Generic,
// in two shapes picked by ARITY:
//
//   x = add(a, b.sh);        // value form — pure math, mutates nothing
//   add(a, b.sh, &dest);     // dest form  — result written through the pointer,
//                         //             dest LAST (PREFERENCES.md rule 9)
//
// Cells: scalars {int, long, float, double} route to the promotion family C
// itself would pick (int+int stays int, anything+long stays long, +float =>
// float, +double => double); Coord2/3/4 pair with themselves (elementwise) and
// with any scalar (broadcast, both orders — s-v and s/v included); mul
// additionally carries Mat4*Mat4 and Mat4*Coord4. Anything else hits
// ov_type_error: a loud compile error, never a silent wrong answer. The dest
// form dictates the destination family the same way (&int* for int+int, &float*
// for anything+float, ...).
//
// Implementation note: every _Generic arm is a bare FUNCTION DESIGNATOR —
// clang type-checks unselected arms, so cells never apply operands themselves;
// one trailing call applies them post-selection.
//
// V*V mul is ELEMENTWISE (GLSL-style); dot/cross stay explicit class calls.
// div mirrors the engine's existing semantics (no zero guard). Header-only:
// every helper is static inline, there should be zero allocations.

static inline void ov_type_error(void) {
}

// --- elementwise helpers ---------------------------------------------------

static inline Coord2 ov_v2_add(Coord2 a, Coord2 b) { return (Coord2){.horizontal = a.x + b.x, .vertical = a.y + b.y}; }
static inline Coord2 ov_v2_sub(Coord2 a, Coord2 b) { return (Coord2){.horizontal = a.x - b.x, .vertical = a.y - b.y}; }
static inline Coord2 ov_v2_mul(Coord2 a, Coord2 b) { return (Coord2){.horizontal = a.x * b.x, .vertical = a.y * b.y}; }
static inline Coord2 ov_v2_div(Coord2 a, Coord2 b) { return (Coord2){.horizontal = a.x / b.x, .vertical = a.y / b.y}; }

static inline Coord3 ov_v3_add(Coord3 a, Coord3 b) { return (Coord3){.horizontal = a.x + b.x, .vertical = a.y + b.y, .depth = a.z + b.z, .frame = 0}; }
static inline Coord3 ov_v3_sub(Coord3 a, Coord3 b) { return (Coord3){.horizontal = a.x - b.x, .vertical = a.y - b.y, .depth = a.z - b.z, .frame = 0}; }
static inline Coord3 ov_v3_mul(Coord3 a, Coord3 b) { return (Coord3){.horizontal = a.x * b.x, .vertical = a.y * b.y, .depth = a.z * b.z, .frame = 0}; }
static inline Coord3 ov_v3_div(Coord3 a, Coord3 b) { return (Coord3){.horizontal = a.x / b.x, .vertical = a.y / b.y, .depth = a.z / b.z, .frame = 0}; }

static inline Coord4 ov_v4_add(Coord4 a, Coord4 b) { return (Coord4){.horizontal = a.x + b.x, .vertical = a.y + b.y, .depth = a.z + b.z, .w = a.w + b.w}; }
static inline Coord4 ov_v4_sub(Coord4 a, Coord4 b) { return (Coord4){.horizontal = a.x - b.x, .vertical = a.y - b.y, .depth = a.z - b.z, .w = a.w - b.w}; }
static inline Coord4 ov_v4_mul(Coord4 a, Coord4 b) { return (Coord4){.horizontal = a.x * b.x, .vertical = a.y * b.y, .depth = a.z * b.z, .w = a.w * b.w}; }
static inline Coord4 ov_v4_div(Coord4 a, Coord4 b) { return (Coord4){.horizontal = a.x / b.x, .vertical = a.y / b.y, .depth = a.z / b.z, .w = a.w / b.w}; }

// v op s broadcast
static inline Coord2 ov_v2_vs_add(Coord2 v, float s) { return (Coord2){.horizontal = v.x + s, .vertical = v.y + s}; }
static inline Coord2 ov_v2_vs_sub(Coord2 v, float s) { return (Coord2){.horizontal = v.x - s, .vertical = v.y - s}; }
static inline Coord2 ov_v2_vs_mul(Coord2 v, float s) { return (Coord2){.horizontal = v.x * s, .vertical = v.y * s}; }
static inline Coord2 ov_v2_vs_div(Coord2 v, float s) { return (Coord2){.horizontal = v.x / s, .vertical = v.y / s}; }

static inline Coord3 ov_v3_vs_add(Coord3 v, float s) { return (Coord3){.horizontal = v.x + s, .vertical = v.y + s, .depth = v.z + s, .frame = 0}; }
static inline Coord3 ov_v3_vs_sub(Coord3 v, float s) { return (Coord3){.horizontal = v.x - s, .vertical = v.y - s, .depth = v.z - s, .frame = 0}; }
static inline Coord3 ov_v3_vs_mul(Coord3 v, float s) { return (Coord3){.horizontal = v.x * s, .vertical = v.y * s, .depth = v.z * s, .frame = 0}; }
static inline Coord3 ov_v3_vs_div(Coord3 v, float s) { return (Coord3){.horizontal = v.x / s, .vertical = v.y / s, .depth = v.z / s, .frame = 0}; }

static inline Coord4 ov_v4_vs_add(Coord4 v, float s) { return (Coord4){.horizontal = v.x + s, .vertical = v.y + s, .depth = v.z + s, .w = v.w + s}; }
static inline Coord4 ov_v4_vs_sub(Coord4 v, float s) { return (Coord4){.horizontal = v.x - s, .vertical = v.y - s, .depth = v.z - s, .w = v.w - s}; }
static inline Coord4 ov_v4_vs_mul(Coord4 v, float s) { return (Coord4){.horizontal = v.x * s, .vertical = v.y * s, .depth = v.z * s, .w = v.w * s}; }
static inline Coord4 ov_v4_vs_div(Coord4 v, float s) { return (Coord4){.horizontal = v.x / s, .vertical = v.y / s, .depth = v.z / s, .w = v.w / s}; }

// s op v broadcast (non-commutative ops get real answers)
static inline Coord2 ov_v2_sv_add(float s, Coord2 v) { return (Coord2){.horizontal = s + v.x, .vertical = s + v.y}; }
static inline Coord2 ov_v2_sv_sub(float s, Coord2 v) { return (Coord2){.horizontal = s - v.x, .vertical = s - v.y}; }
static inline Coord2 ov_v2_sv_mul(float s, Coord2 v) { return (Coord2){.horizontal = s * v.x, .vertical = s * v.y}; }
static inline Coord2 ov_v2_sv_div(float s, Coord2 v) { return (Coord2){.horizontal = s / v.x, .vertical = s / v.y}; }

static inline Coord3 ov_v3_sv_add(float s, Coord3 v) { return (Coord3){.horizontal = s + v.x, .vertical = s + v.y, .depth = s + v.z, .frame = 0}; }
static inline Coord3 ov_v3_sv_sub(float s, Coord3 v) { return (Coord3){.horizontal = s - v.x, .vertical = s - v.y, .depth = s - v.z, .frame = 0}; }
static inline Coord3 ov_v3_sv_mul(float s, Coord3 v) { return (Coord3){.horizontal = s * v.x, .vertical = s * v.y, .depth = s * v.z, .frame = 0}; }
static inline Coord3 ov_v3_sv_div(float s, Coord3 v) { return (Coord3){.horizontal = s / v.x, .vertical = s / v.y, .depth = s / v.z, .frame = 0}; }

static inline Coord4 ov_v4_sv_add(float s, Coord4 v) { return (Coord4){.horizontal = s + v.x, .vertical = s + v.y, .depth = s + v.z, .w = s + v.w}; }
static inline Coord4 ov_v4_sv_sub(float s, Coord4 v) { return (Coord4){.horizontal = s - v.x, .vertical = s - v.y, .depth = s - v.z, .w = s - v.w}; }
static inline Coord4 ov_v4_sv_mul(float s, Coord4 v) { return (Coord4){.horizontal = s * v.x, .vertical = s * v.y, .depth = s * v.z, .w = s * v.w}; }
static inline Coord4 ov_v4_sv_div(float s, Coord4 v) { return (Coord4){.horizontal = s / v.x, .vertical = s / v.y, .depth = s / v.z, .w = s / v.w}; }

// --- matrix helpers (mul only) ---------------------------------------------

static inline Mat4 ov_m4_mul(Mat4 a, Mat4 b) {
    Mat4 r;
    Mat4_multiply(&a, &b, &r);
    return r;
}

static inline Coord4 ov_m4_transform(Mat4 m, Coord4 v) {
    Coord4 r;
    Mat4_transform(&m, &v, &r);
    return r;
}

static inline void ov_m4_d_mul(Mat4 a, Mat4 b, Mat4 *d) {
    Mat4_multiply(&a, &b, d);
}

static inline void ov_m4_d_transform(Mat4 m, Coord4 v, Coord4 *d) {
    Mat4_transform(&m, &v, d);
}

// --- scalar family functions -------------------------------------------------
// One set per promotion family; the matrix routes each pair to the family its
// C promotion would produce. Dest writers dictate the matching destination.

#define OV_SCALAR_FNS(op, OP)                                                     \
    static inline int ov_f_##op##_ii(int a, int b) { return a OP b; }             \
    static inline long ov_f_##op##_ll(long a, long b) { return a OP b; }          \
    static inline float ov_f_##op##_ff(float a, float b) { return a OP b; }       \
    static inline double ov_f_##op##_dd(double a, double b) { return a OP b; }    \
    static inline void ov_w_##op##_ii(int a, int b, int *d) { (*d) = a OP b; }    \
    static inline void ov_w_##op##_ll(long a, long b, long *d) { (*d) = a OP b; } \
    static inline void ov_w_##op##_ff(float a, float b, float *d) { (*d) = a OP b; } \
    static inline void ov_w_##op##_dd(double a, double b, double *d) { (*d) = a OP b; }

OV_SCALAR_FNS(add, +)
OV_SCALAR_FNS(sub, -)
OV_SCALAR_FNS(mul, *)
OV_SCALAR_FNS(div, /)

// --- dest writers for vectors ------------------------------------------------

static inline void ov_v2_d_add(Coord2 a, Coord2 b, Coord2 *d) { (*d) = ov_v2_add(a, b); }
static inline void ov_v2_d_sub(Coord2 a, Coord2 b, Coord2 *d) { (*d) = ov_v2_sub(a, b); }
static inline void ov_v2_d_mul(Coord2 a, Coord2 b, Coord2 *d) { (*d) = ov_v2_mul(a, b); }
static inline void ov_v2_d_div(Coord2 a, Coord2 b, Coord2 *d) { (*d) = ov_v2_div(a, b); }
static inline void ov_v2_vs_d_add(Coord2 v, float s, Coord2 *d) { (*d) = ov_v2_vs_add(v, s); }
static inline void ov_v2_vs_d_sub(Coord2 v, float s, Coord2 *d) { (*d) = ov_v2_vs_sub(v, s); }
static inline void ov_v2_vs_d_mul(Coord2 v, float s, Coord2 *d) { (*d) = ov_v2_vs_mul(v, s); }
static inline void ov_v2_vs_d_div(Coord2 v, float s, Coord2 *d) { (*d) = ov_v2_vs_div(v, s); }
static inline void ov_v2_sv_d_add(float s, Coord2 v, Coord2 *d) { (*d) = ov_v2_sv_add(s, v); }
static inline void ov_v2_sv_d_sub(float s, Coord2 v, Coord2 *d) { (*d) = ov_v2_sv_sub(s, v); }
static inline void ov_v2_sv_d_mul(float s, Coord2 v, Coord2 *d) { (*d) = ov_v2_sv_mul(s, v); }
static inline void ov_v2_sv_d_div(float s, Coord2 v, Coord2 *d) { (*d) = ov_v2_sv_div(s, v); }

static inline void ov_v3_d_add(Coord3 a, Coord3 b, Coord3 *d) { (*d) = ov_v3_add(a, b); }
static inline void ov_v3_d_sub(Coord3 a, Coord3 b, Coord3 *d) { (*d) = ov_v3_sub(a, b); }
static inline void ov_v3_d_mul(Coord3 a, Coord3 b, Coord3 *d) { (*d) = ov_v3_mul(a, b); }
static inline void ov_v3_d_div(Coord3 a, Coord3 b, Coord3 *d) { (*d) = ov_v3_div(a, b); }
static inline void ov_v3_vs_d_add(Coord3 v, float s, Coord3 *d) { (*d) = ov_v3_vs_add(v, s); }
static inline void ov_v3_vs_d_sub(Coord3 v, float s, Coord3 *d) { (*d) = ov_v3_vs_sub(v, s); }
static inline void ov_v3_vs_d_mul(Coord3 v, float s, Coord3 *d) { (*d) = ov_v3_vs_mul(v, s); }
static inline void ov_v3_vs_d_div(Coord3 v, float s, Coord3 *d) { (*d) = ov_v3_vs_div(v, s); }
static inline void ov_v3_sv_d_add(float s, Coord3 v, Coord3 *d) { (*d) = ov_v3_sv_add(s, v); }
static inline void ov_v3_sv_d_sub(float s, Coord3 v, Coord3 *d) { (*d) = ov_v3_sv_sub(s, v); }
static inline void ov_v3_sv_d_mul(float s, Coord3 v, Coord3 *d) { (*d) = ov_v3_sv_mul(s, v); }
static inline void ov_v3_sv_d_div(float s, Coord3 v, Coord3 *d) { (*d) = ov_v3_sv_div(s, v); }

static inline void ov_v4_d_add(Coord4 a, Coord4 b, Coord4 *d) { (*d) = ov_v4_add(a, b); }
static inline void ov_v4_d_sub(Coord4 a, Coord4 b, Coord4 *d) { (*d) = ov_v4_sub(a, b); }
static inline void ov_v4_d_mul(Coord4 a, Coord4 b, Coord4 *d) { (*d) = ov_v4_mul(a, b); }
static inline void ov_v4_d_div(Coord4 a, Coord4 b, Coord4 *d) { (*d) = ov_v4_div(a, b); }
static inline void ov_v4_vs_d_add(Coord4 v, float s, Coord4 *d) { (*d) = ov_v4_vs_add(v, s); }
static inline void ov_v4_vs_d_sub(Coord4 v, float s, Coord4 *d) { (*d) = ov_v4_vs_sub(v, s); }
static inline void ov_v4_vs_d_mul(Coord4 v, float s, Coord4 *d) { (*d) = ov_v4_vs_mul(v, s); }
static inline void ov_v4_vs_d_div(Coord4 v, float s, Coord4 *d) { (*d) = ov_v4_vs_div(v, s); }
static inline void ov_v4_sv_d_add(float s, Coord4 v, Coord4 *d) { (*d) = ov_v4_sv_add(s, v); }
static inline void ov_v4_sv_d_sub(float s, Coord4 v, Coord4 *d) { (*d) = ov_v4_sv_sub(s, v); }
static inline void ov_v4_sv_d_mul(float s, Coord4 v, Coord4 *d) { (*d) = ov_v4_sv_mul(s, v); }
static inline void ov_v4_sv_d_div(float s, Coord4 v, Coord4 *d) { (*d) = ov_v4_sv_div(s, v); }

// --- rows: scalar LHS (value + dest) ------------------------------------------
// Family routing per LHS row: int keeps int/long math exact, long absorbs int,
// float absorbs int/long, double absorbs everything.

#define OV_VROW_INT(op, a, b)                    \
    int: _Generic((b),                     \
        int: ov_f_##op##_ii,               \
        long: ov_f_##op##_ll,              \
        float: ov_f_##op##_ff,             \
        double: ov_f_##op##_dd,            \
        Coord2: ov_v2_sv_##op,               \
        Coord3: ov_v3_sv_##op,               \
        Coord4: ov_v4_sv_##op,               \
        default: ov_type_error)

#define OV_VROW_LONG(op, a, b)                   \
    long: _Generic((b),                    \
        int: ov_f_##op##_ll,               \
        long: ov_f_##op##_ll,              \
        float: ov_f_##op##_ff,             \
        double: ov_f_##op##_dd,            \
        Coord2: ov_v2_sv_##op,               \
        Coord3: ov_v3_sv_##op,               \
        Coord4: ov_v4_sv_##op,               \
        default: ov_type_error)

#define OV_VROW_FLOAT(op, a, b)                  \
    float: _Generic((b),                   \
        int: ov_f_##op##_ff,               \
        long: ov_f_##op##_ff,              \
        float: ov_f_##op##_ff,             \
        double: ov_f_##op##_dd,            \
        Coord2: ov_v2_sv_##op,               \
        Coord3: ov_v3_sv_##op,               \
        Coord4: ov_v4_sv_##op,               \
        default: ov_type_error)

#define OV_VROW_DOUBLE(op, a, b)                 \
    double: _Generic((b),                  \
        int: ov_f_##op##_dd,               \
        long: ov_f_##op##_dd,              \
        float: ov_f_##op##_dd,             \
        double: ov_f_##op##_dd,            \
        Coord2: ov_v2_sv_##op,               \
        Coord3: ov_v3_sv_##op,               \
        Coord4: ov_v4_sv_##op,               \
        default: ov_type_error)

#define OV_DROW_INT(op, a, b, d)                    \
    int: _Generic((b),                     \
        int: ov_w_##op##_ii,               \
        long: ov_w_##op##_ll,              \
        float: ov_w_##op##_ff,             \
        double: ov_w_##op##_dd,            \
        Coord2: ov_v2_sv_d_##op,             \
        Coord3: ov_v3_sv_d_##op,             \
        Coord4: ov_v4_sv_d_##op,             \
        default: ov_type_error)

#define OV_DROW_LONG(op, a, b, d)                   \
    long: _Generic((b),                    \
        int: ov_w_##op##_ll,               \
        long: ov_w_##op##_ll,              \
        float: ov_w_##op##_ff,             \
        double: ov_w_##op##_dd,            \
        Coord2: ov_v2_sv_d_##op,             \
        Coord3: ov_v3_sv_d_##op,             \
        Coord4: ov_v4_sv_d_##op,             \
        default: ov_type_error)

#define OV_DROW_FLOAT(op, a, b, d)                  \
    float: _Generic((b),                   \
        int: ov_w_##op##_ff,               \
        long: ov_w_##op##_ff,              \
        float: ov_w_##op##_ff,             \
        double: ov_w_##op##_dd,            \
        Coord2: ov_v2_sv_d_##op,             \
        Coord3: ov_v3_sv_d_##op,             \
        Coord4: ov_v4_sv_d_##op,             \
        default: ov_type_error)

#define OV_DROW_DOUBLE(op, a, b, d)                 \
    double: _Generic((b),                  \
        int: ov_w_##op##_dd,               \
        long: ov_w_##op##_dd,              \
        float: ov_w_##op##_dd,             \
        double: ov_w_##op##_dd,            \
        Coord2: ov_v2_sv_d_##op,             \
        Coord3: ov_v3_sv_d_##op,             \
        Coord4: ov_v4_sv_d_##op,             \
        default: ov_type_error)

// --- rows: vector LHS (value + dest) ------------------------------------------

#define OV_VVEC_ROW(n, op, a, b)                             \
    Vec##n: _Generic((b),                              \
        Vec##n: ov_v##n##_##op,                        \
        int: ov_v##n##_vs_##op,                        \
        long: ov_v##n##_vs_##op,                       \
        float: ov_v##n##_vs_##op,                      \
        double: ov_v##n##_vs_##op,                     \
        default: ov_type_error)

#define OV_DVEC_ROW(n, op, a, b, d)                             \
    Vec##n: _Generic((b),                              \
        Vec##n: ov_v##n##_d_##op,                      \
        int: ov_v##n##_vs_d_##op,                      \
        long: ov_v##n##_vs_d_##op,                     \
        float: ov_v##n##_vs_d_##op,                    \
        double: ov_v##n##_vs_d_##op,                   \
        default: ov_type_error)

// --- assembled operations ------------------------------------------------------

#define OV_VAL_ADD(a, b) _Generic((a), OV_VROW_INT(add, a, b), OV_VROW_LONG(add, a, b), OV_VROW_FLOAT(add, a, b), OV_VROW_DOUBLE(add, a, b), OV_VVEC_ROW(2, add, a, b), OV_VVEC_ROW(3, add, a, b), OV_VVEC_ROW(4, add, a, b), default: ov_type_error)((a), (b))
#define OV_VAL_SUB(a, b) _Generic((a), OV_VROW_INT(sub, a, b), OV_VROW_LONG(sub, a, b), OV_VROW_FLOAT(sub, a, b), OV_VROW_DOUBLE(sub, a, b), OV_VVEC_ROW(2, sub, a, b), OV_VVEC_ROW(3, sub, a, b), OV_VVEC_ROW(4, sub, a, b), default: ov_type_error)((a), (b))
#define OV_VAL_DIV(a, b) _Generic((a), OV_VROW_INT(div, a, b), OV_VROW_LONG(div, a, b), OV_VROW_FLOAT(div, a, b), OV_VROW_DOUBLE(div, a, b), OV_VVEC_ROW(2, div, a, b), OV_VVEC_ROW(3, div, a, b), OV_VVEC_ROW(4, div, a, b), default: ov_type_error)((a), (b))

#define OV_VAL_MUL(a, b) _Generic((a), OV_VROW_INT(mul, a, b), OV_VROW_LONG(mul, a, b), OV_VROW_FLOAT(mul, a, b), OV_VROW_DOUBLE(mul, a, b), OV_VVEC_ROW(2, mul, a, b), OV_VVEC_ROW(3, mul, a, b), OV_VVEC_ROW(4, mul, a, b), \
    Mat4: _Generic((b),                                                                                          \
        Mat4: ov_m4_mul,                                                                                         \
        Coord4: ov_m4_transform,                                                                                   \
        default: ov_type_error),                                                                                 \
    default: ov_type_error)((a), (b))

#define OV_DEST_ADD(a, b, d) _Generic((a), OV_DROW_INT(add, a, b, d), OV_DROW_LONG(add, a, b, d), OV_DROW_FLOAT(add, a, b, d), OV_DROW_DOUBLE(add, a, b, d), OV_DVEC_ROW(2, add, a, b, d), OV_DVEC_ROW(3, add, a, b, d), OV_DVEC_ROW(4, add, a, b, d), default: ov_type_error)((a), (b), (d))
#define OV_DEST_SUB(a, b, d) _Generic((a), OV_DROW_INT(sub, a, b, d), OV_DROW_LONG(sub, a, b, d), OV_DROW_FLOAT(sub, a, b, d), OV_DROW_DOUBLE(sub, a, b, d), OV_DVEC_ROW(2, sub, a, b, d), OV_DVEC_ROW(3, sub, a, b, d), OV_DVEC_ROW(4, sub, a, b, d), default: ov_type_error)((a), (b), (d))
#define OV_DEST_DIV(a, b, d) _Generic((a), OV_DROW_INT(div, a, b, d), OV_DROW_LONG(div, a, b, d), OV_DROW_FLOAT(div, a, b, d), OV_DROW_DOUBLE(div, a, b, d), OV_DVEC_ROW(2, div, a, b, d), OV_DVEC_ROW(3, div, a, b, d), OV_DVEC_ROW(4, div, a, b, d), default: ov_type_error)((a), (b), (d))

#define OV_DEST_MUL(a, b, d) _Generic((a), OV_DROW_INT(mul, a, b, d), OV_DROW_LONG(mul, a, b, d), OV_DROW_FLOAT(mul, a, b, d), OV_DROW_DOUBLE(mul, a, b, d), OV_DVEC_ROW(2, mul, a, b, d), OV_DVEC_ROW(3, mul, a, b, d), OV_DVEC_ROW(4, mul, a, b, d), \
    Mat4: _Generic((b),                                                                                              \
        Mat4: ov_m4_d_mul,                                                                                           \
        Coord4: ov_m4_d_transform,                                                                                     \
        default: ov_type_error),                                                                                     \
    default: ov_type_error)((a), (b), (d))

// --- public surface: arity picks shape, type picks function ---------------------

#define OV_PICK(_0, _1, _2, _3, NAME, ...) NAME

#define add(...) OV_PICK(dummy __VA_OPT__(,) __VA_ARGS__, ov3_add, ov2_add)(__VA_ARGS__)
#define sub(...) OV_PICK(dummy __VA_OPT__(,) __VA_ARGS__, ov3_sub, ov2_sub)(__VA_ARGS__)
#define mul(...) OV_PICK(dummy __VA_OPT__(,) __VA_ARGS__, ov3_mul, ov2_mul)(__VA_ARGS__)
#define div(...) OV_PICK(dummy __VA_OPT__(,) __VA_ARGS__, ov3_div, ov2_div)(__VA_ARGS__)

#define ov2_add(a, b) OV_VAL_ADD((a), (b))
#define ov3_add(a, b, d) OV_DEST_ADD((a), (b), (d))
#define ov2_sub(a, b) OV_VAL_SUB((a), (b))
#define ov3_sub(a, b, d) OV_DEST_SUB((a), (b), (d))
#define ov2_mul(a, b) OV_VAL_MUL((a), (b))
#define ov3_mul(a, b, d) OV_DEST_MUL((a), (b), (d))
#define ov2_div(a, b) OV_VAL_DIV((a), (b))
#define ov3_div(a, b, d) OV_DEST_DIV((a), (b), (d))

#endif
