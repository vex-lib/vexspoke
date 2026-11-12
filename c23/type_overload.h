#ifndef C23_TYPE_OVERLOAD_H
#define C23_TYPE_OVERLOAD_H

#include "vex.h"

// c23/overload.h — type-based overloading (the other half of constructor.h).
//
// One name, many types: add/sub/mul/div dispatch on operand TYPES via _Generic,
// in two shapes picked by ARITY:
//
//   x = add(a, b.sh);        // value form — pure math, mutates nothing
//   add(a, b.sh, &dest);     // dest form  — result written through the pointer,
//                         //             dest LAST (PREFERENCES.md rule 9)
//
// Cells: scalars {int, long, f32, d64} route to the promotion family C
// itself would pick (int+int stays int, anything+long stays long, +f32 =>
// f32, +d64 => d64); vec2f/vec3f/vec4f pair with themselves (elementwise) and
// with any scalar (broadcast, both orders — s-v and s/v included); mul
// additionally carries mat4f*mat4f and mat4f*vec4f. Anything else hits
// ov_type_error: a loud compile error, never a silent wrong answer. The dest
// form dictates the destination family the same way (&int* for int+int, &f32*
// for anything+f32, ...).
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

static inline vec2f ov_v2_add(vec2f a, vec2f b) { return (vec2f){.x = a.x + b.x, .y = a.y + b.y}; }
static inline vec2f ov_v2_sub(vec2f a, vec2f b) { return (vec2f){.x = a.x - b.x, .y = a.y - b.y}; }
static inline vec2f ov_v2_mul(vec2f a, vec2f b) { return (vec2f){.x = a.x * b.x, .y = a.y * b.y}; }
static inline vec2f ov_v2_div(vec2f a, vec2f b) { return (vec2f){.x = a.x / b.x, .y = a.y / b.y}; }

static inline vec3f ov_v3_add(vec3f a, vec3f b) { return (vec3f){.x = a.x + b.x, .y = a.y + b.y, .z = a.z + b.z}; }
static inline vec3f ov_v3_sub(vec3f a, vec3f b) { return (vec3f){.x = a.x - b.x, .y = a.y - b.y, .z = a.z - b.z}; }
static inline vec3f ov_v3_mul(vec3f a, vec3f b) { return (vec3f){.x = a.x * b.x, .y = a.y * b.y, .z = a.z * b.z}; }
static inline vec3f ov_v3_div(vec3f a, vec3f b) { return (vec3f){.x = a.x / b.x, .y = a.y / b.y, .z = a.z / b.z}; }

static inline vec4f ov_v4_add(vec4f a, vec4f b) { return (vec4f){.x = a.x + b.x, .y = a.y + b.y, .z = a.z + b.z, .w = a.w + b.w}; }
static inline vec4f ov_v4_sub(vec4f a, vec4f b) { return (vec4f){.x = a.x - b.x, .y = a.y - b.y, .z = a.z - b.z, .w = a.w - b.w}; }
static inline vec4f ov_v4_mul(vec4f a, vec4f b) { return (vec4f){.x = a.x * b.x, .y = a.y * b.y, .z = a.z * b.z, .w = a.w * b.w}; }
static inline vec4f ov_v4_div(vec4f a, vec4f b) { return (vec4f){.x = a.x / b.x, .y = a.y / b.y, .z = a.z / b.z, .w = a.w / b.w}; }

// v op s broadcast
static inline vec2f ov_v2_vs_add(vec2f v, f32 s) { return (vec2f){.x = v.x + s, .y = v.y + s}; }
static inline vec2f ov_v2_vs_sub(vec2f v, f32 s) { return (vec2f){.x = v.x - s, .y = v.y - s}; }
static inline vec2f ov_v2_vs_mul(vec2f v, f32 s) { return (vec2f){.x = v.x * s, .y = v.y * s}; }
static inline vec2f ov_v2_vs_div(vec2f v, f32 s) { return (vec2f){.x = v.x / s, .y = v.y / s}; }

static inline vec3f ov_v3_vs_add(vec3f v, f32 s) { return (vec3f){.x = v.x + s, .y = v.y + s, .z = v.z + s}; }
static inline vec3f ov_v3_vs_sub(vec3f v, f32 s) { return (vec3f){.x = v.x - s, .y = v.y - s, .z = v.z - s}; }
static inline vec3f ov_v3_vs_mul(vec3f v, f32 s) { return (vec3f){.x = v.x * s, .y = v.y * s, .z = v.z * s}; }
static inline vec3f ov_v3_vs_div(vec3f v, f32 s) { return (vec3f){.x = v.x / s, .y = v.y / s, .z = v.z / s}; }

static inline vec4f ov_v4_vs_add(vec4f v, f32 s) { return (vec4f){.x = v.x + s, .y = v.y + s, .z = v.z + s, .w = v.w + s}; }
static inline vec4f ov_v4_vs_sub(vec4f v, f32 s) { return (vec4f){.x = v.x - s, .y = v.y - s, .z = v.z - s, .w = v.w - s}; }
static inline vec4f ov_v4_vs_mul(vec4f v, f32 s) { return (vec4f){.x = v.x * s, .y = v.y * s, .z = v.z * s, .w = v.w * s}; }
static inline vec4f ov_v4_vs_div(vec4f v, f32 s) { return (vec4f){.x = v.x / s, .y = v.y / s, .z = v.z / s, .w = v.w / s}; }

// s op v broadcast (non-commutative ops get real answers)
static inline vec2f ov_v2_sv_add(f32 s, vec2f v) { return (vec2f){.x = s + v.x, .y = s + v.y}; }
static inline vec2f ov_v2_sv_sub(f32 s, vec2f v) { return (vec2f){.x = s - v.x, .y = s - v.y}; }
static inline vec2f ov_v2_sv_mul(f32 s, vec2f v) { return (vec2f){.x = s * v.x, .y = s * v.y}; }
static inline vec2f ov_v2_sv_div(f32 s, vec2f v) { return (vec2f){.x = s / v.x, .y = s / v.y}; }

static inline vec3f ov_v3_sv_add(f32 s, vec3f v) { return (vec3f){.x = s + v.x, .y = s + v.y, .z = s + v.z}; }
static inline vec3f ov_v3_sv_sub(f32 s, vec3f v) { return (vec3f){.x = s - v.x, .y = s - v.y, .z = s - v.z}; }
static inline vec3f ov_v3_sv_mul(f32 s, vec3f v) { return (vec3f){.x = s * v.x, .y = s * v.y, .z = s * v.z}; }
static inline vec3f ov_v3_sv_div(f32 s, vec3f v) { return (vec3f){.x = s / v.x, .y = s / v.y, .z = s / v.z}; }

static inline vec4f ov_v4_sv_add(f32 s, vec4f v) { return (vec4f){.x = s + v.x, .y = s + v.y, .z = s + v.z, .w = s + v.w}; }
static inline vec4f ov_v4_sv_sub(f32 s, vec4f v) { return (vec4f){.x = s - v.x, .y = s - v.y, .z = s - v.z, .w = s - v.w}; }
static inline vec4f ov_v4_sv_mul(f32 s, vec4f v) { return (vec4f){.x = s * v.x, .y = s * v.y, .z = s * v.z, .w = s * v.w}; }
static inline vec4f ov_v4_sv_div(f32 s, vec4f v) { return (vec4f){.x = s / v.x, .y = s / v.y, .z = s / v.z, .w = s / v.w}; }

// --- matrix helpers (mul only) ---------------------------------------------

static inline mat4f ov_m4_mul(mat4f a, mat4f b) {
    return mat4f_multiply(a, b);
}

static inline vec4f ov_m4_transform(mat4f m, vec4f v) {
    return mat4f_multiplyVec(m, v);
}

static inline void ov_m4_d_mul(mat4f a, mat4f b, mat4f *d) {
    (*d) = mat4f_multiply(a, b);
}

static inline void ov_m4_d_transform(mat4f m, vec4f v, vec4f *d) {
    (*d) = mat4f_multiplyVec(m, v);
}

// --- scalar family functions -------------------------------------------------
// One set per promotion family; the matrix routes each pair to the family its
// C promotion would produce. Dest writers dictate the matching destination.

#define OV_SCALAR_FNS(op, OP)                                                     \
    static inline int ov_f_##op##_ii(int a, int b) { return a OP b; }             \
    static inline long ov_f_##op##_ll(long a, long b) { return a OP b; }          \
    static inline f32 ov_f_##op##_ff(f32 a, f32 b) { return a OP b; }       \
    static inline d64 ov_f_##op##_dd(d64 a, d64 b) { return a OP b; }    \
    static inline void ov_w_##op##_ii(int a, int b, int *d) { (*d) = a OP b; }    \
    static inline void ov_w_##op##_ll(long a, long b, long *d) { (*d) = a OP b; } \
    static inline void ov_w_##op##_ff(f32 a, f32 b, f32 *d) { (*d) = a OP b; } \
    static inline void ov_w_##op##_dd(d64 a, d64 b, d64 *d) { (*d) = a OP b; }

OV_SCALAR_FNS(add, +)
OV_SCALAR_FNS(sub, -)
OV_SCALAR_FNS(mul, *)
OV_SCALAR_FNS(div, /)

// --- dest writers for vectors ------------------------------------------------

static inline void ov_v2_d_add(vec2f a, vec2f b, vec2f *d) { (*d) = ov_v2_add(a, b); }
static inline void ov_v2_d_sub(vec2f a, vec2f b, vec2f *d) { (*d) = ov_v2_sub(a, b); }
static inline void ov_v2_d_mul(vec2f a, vec2f b, vec2f *d) { (*d) = ov_v2_mul(a, b); }
static inline void ov_v2_d_div(vec2f a, vec2f b, vec2f *d) { (*d) = ov_v2_div(a, b); }
static inline void ov_v2_vs_d_add(vec2f v, f32 s, vec2f *d) { (*d) = ov_v2_vs_add(v, s); }
static inline void ov_v2_vs_d_sub(vec2f v, f32 s, vec2f *d) { (*d) = ov_v2_vs_sub(v, s); }
static inline void ov_v2_vs_d_mul(vec2f v, f32 s, vec2f *d) { (*d) = ov_v2_vs_mul(v, s); }
static inline void ov_v2_vs_d_div(vec2f v, f32 s, vec2f *d) { (*d) = ov_v2_vs_div(v, s); }
static inline void ov_v2_sv_d_add(f32 s, vec2f v, vec2f *d) { (*d) = ov_v2_sv_add(s, v); }
static inline void ov_v2_sv_d_sub(f32 s, vec2f v, vec2f *d) { (*d) = ov_v2_sv_sub(s, v); }
static inline void ov_v2_sv_d_mul(f32 s, vec2f v, vec2f *d) { (*d) = ov_v2_sv_mul(s, v); }
static inline void ov_v2_sv_d_div(f32 s, vec2f v, vec2f *d) { (*d) = ov_v2_sv_div(s, v); }

static inline void ov_v3_d_add(vec3f a, vec3f b, vec3f *d) { (*d) = ov_v3_add(a, b); }
static inline void ov_v3_d_sub(vec3f a, vec3f b, vec3f *d) { (*d) = ov_v3_sub(a, b); }
static inline void ov_v3_d_mul(vec3f a, vec3f b, vec3f *d) { (*d) = ov_v3_mul(a, b); }
static inline void ov_v3_d_div(vec3f a, vec3f b, vec3f *d) { (*d) = ov_v3_div(a, b); }
static inline void ov_v3_vs_d_add(vec3f v, f32 s, vec3f *d) { (*d) = ov_v3_vs_add(v, s); }
static inline void ov_v3_vs_d_sub(vec3f v, f32 s, vec3f *d) { (*d) = ov_v3_vs_sub(v, s); }
static inline void ov_v3_vs_d_mul(vec3f v, f32 s, vec3f *d) { (*d) = ov_v3_vs_mul(v, s); }
static inline void ov_v3_vs_d_div(vec3f v, f32 s, vec3f *d) { (*d) = ov_v3_vs_div(v, s); }
static inline void ov_v3_sv_d_add(f32 s, vec3f v, vec3f *d) { (*d) = ov_v3_sv_add(s, v); }
static inline void ov_v3_sv_d_sub(f32 s, vec3f v, vec3f *d) { (*d) = ov_v3_sv_sub(s, v); }
static inline void ov_v3_sv_d_mul(f32 s, vec3f v, vec3f *d) { (*d) = ov_v3_sv_mul(s, v); }
static inline void ov_v3_sv_d_div(f32 s, vec3f v, vec3f *d) { (*d) = ov_v3_sv_div(s, v); }

static inline void ov_v4_d_add(vec4f a, vec4f b, vec4f *d) { (*d) = ov_v4_add(a, b); }
static inline void ov_v4_d_sub(vec4f a, vec4f b, vec4f *d) { (*d) = ov_v4_sub(a, b); }
static inline void ov_v4_d_mul(vec4f a, vec4f b, vec4f *d) { (*d) = ov_v4_mul(a, b); }
static inline void ov_v4_d_div(vec4f a, vec4f b, vec4f *d) { (*d) = ov_v4_div(a, b); }
static inline void ov_v4_vs_d_add(vec4f v, f32 s, vec4f *d) { (*d) = ov_v4_vs_add(v, s); }
static inline void ov_v4_vs_d_sub(vec4f v, f32 s, vec4f *d) { (*d) = ov_v4_vs_sub(v, s); }
static inline void ov_v4_vs_d_mul(vec4f v, f32 s, vec4f *d) { (*d) = ov_v4_vs_mul(v, s); }
static inline void ov_v4_vs_d_div(vec4f v, f32 s, vec4f *d) { (*d) = ov_v4_vs_div(v, s); }
static inline void ov_v4_sv_d_add(f32 s, vec4f v, vec4f *d) { (*d) = ov_v4_sv_add(s, v); }
static inline void ov_v4_sv_d_sub(f32 s, vec4f v, vec4f *d) { (*d) = ov_v4_sv_sub(s, v); }
static inline void ov_v4_sv_d_mul(f32 s, vec4f v, vec4f *d) { (*d) = ov_v4_sv_mul(s, v); }
static inline void ov_v4_sv_d_div(f32 s, vec4f v, vec4f *d) { (*d) = ov_v4_sv_div(s, v); }

// --- rows: scalar LHS (value + dest) ------------------------------------------
// Family routing per LHS row: int keeps int/long math exact, long absorbs int,
// f32 absorbs int/long, d64 absorbs everything.

#define OV_VROW_INT(op, a, b)                    \
    int: _Generic((b),                     \
        int: ov_f_##op##_ii,               \
        long: ov_f_##op##_ll,              \
        f32: ov_f_##op##_ff,             \
        d64: ov_f_##op##_dd,            \
        vec2f: ov_v2_sv_##op,               \
        vec3f: ov_v3_sv_##op,               \
        vec4f: ov_v4_sv_##op,               \
        default: ov_type_error)

#define OV_VROW_LONG(op, a, b)                   \
    long: _Generic((b),                    \
        int: ov_f_##op##_ll,               \
        long: ov_f_##op##_ll,              \
        f32: ov_f_##op##_ff,             \
        d64: ov_f_##op##_dd,            \
        vec2f: ov_v2_sv_##op,               \
        vec3f: ov_v3_sv_##op,               \
        vec4f: ov_v4_sv_##op,               \
        default: ov_type_error)

#define OV_VROW_FLOAT(op, a, b)                  \
    f32: _Generic((b),                   \
        int: ov_f_##op##_ff,               \
        long: ov_f_##op##_ff,              \
        f32: ov_f_##op##_ff,             \
        d64: ov_f_##op##_dd,            \
        vec2f: ov_v2_sv_##op,               \
        vec3f: ov_v3_sv_##op,               \
        vec4f: ov_v4_sv_##op,               \
        default: ov_type_error)

#define OV_VROW_f64(op, a, b)                 \
    d64: _Generic((b),                  \
        int: ov_f_##op##_dd,               \
        long: ov_f_##op##_dd,              \
        f32: ov_f_##op##_dd,             \
        d64: ov_f_##op##_dd,            \
        vec2f: ov_v2_sv_##op,               \
        vec3f: ov_v3_sv_##op,               \
        vec4f: ov_v4_sv_##op,               \
        default: ov_type_error)

#define OV_DROW_INT(op, a, b, d)                    \
    int: _Generic((b),                     \
        int: ov_w_##op##_ii,               \
        long: ov_w_##op##_ll,              \
        f32: ov_w_##op##_ff,             \
        d64: ov_w_##op##_dd,            \
        vec2f: ov_v2_sv_d_##op,             \
        vec3f: ov_v3_sv_d_##op,             \
        vec4f: ov_v4_sv_d_##op,             \
        default: ov_type_error)

#define OV_DROW_LONG(op, a, b, d)                   \
    long: _Generic((b),                    \
        int: ov_w_##op##_ll,               \
        long: ov_w_##op##_ll,              \
        f32: ov_w_##op##_ff,             \
        d64: ov_w_##op##_dd,            \
        vec2f: ov_v2_sv_d_##op,             \
        vec3f: ov_v3_sv_d_##op,             \
        vec4f: ov_v4_sv_d_##op,             \
        default: ov_type_error)

#define OV_DROW_FLOAT(op, a, b, d)                  \
    f32: _Generic((b),                   \
        int: ov_w_##op##_ff,               \
        long: ov_w_##op##_ff,              \
        f32: ov_w_##op##_ff,             \
        d64: ov_w_##op##_dd,            \
        vec2f: ov_v2_sv_d_##op,             \
        vec3f: ov_v3_sv_d_##op,             \
        vec4f: ov_v4_sv_d_##op,             \
        default: ov_type_error)

#define OV_DROW_f64(op, a, b, d)                 \
    d64: _Generic((b),                  \
        int: ov_w_##op##_dd,               \
        long: ov_w_##op##_dd,              \
        f32: ov_w_##op##_dd,             \
        d64: ov_w_##op##_dd,            \
        vec2f: ov_v2_sv_d_##op,             \
        vec3f: ov_v3_sv_d_##op,             \
        vec4f: ov_v4_sv_d_##op,             \
        default: ov_type_error)

// --- rows: vector LHS (value + dest) ------------------------------------------

#define OV_VVEC_ROW(n, op, a, b)                             \
    vec##n##f: _Generic((b),                              \
        vec##n##f: ov_v##n##_##op,                        \
        int: ov_v##n##_vs_##op,                        \
        long: ov_v##n##_vs_##op,                       \
        f32: ov_v##n##_vs_##op,                      \
        d64: ov_v##n##_vs_##op,                     \
        default: ov_type_error)

#define OV_DVEC_ROW(n, op, a, b, d)                             \
    vec##n##f: _Generic((b),                              \
        vec##n##f: ov_v##n##_d_##op,                      \
        int: ov_v##n##_vs_d_##op,                      \
        long: ov_v##n##_vs_d_##op,                     \
        f32: ov_v##n##_vs_d_##op,                    \
        d64: ov_v##n##_vs_d_##op,                   \
        default: ov_type_error)

// --- assembled operations ------------------------------------------------------

#define OV_VAL_ADD(a, b) _Generic((a), OV_VROW_INT(add, a, b), OV_VROW_LONG(add, a, b), OV_VROW_FLOAT(add, a, b), OV_VROW_f64(add, a, b), OV_VVEC_ROW(2, add, a, b), OV_VVEC_ROW(3, add, a, b), OV_VVEC_ROW(4, add, a, b), default: ov_type_error)((a), (b))
#define OV_VAL_SUB(a, b) _Generic((a), OV_VROW_INT(sub, a, b), OV_VROW_LONG(sub, a, b), OV_VROW_FLOAT(sub, a, b), OV_VROW_f64(sub, a, b), OV_VVEC_ROW(2, sub, a, b), OV_VVEC_ROW(3, sub, a, b), OV_VVEC_ROW(4, sub, a, b), default: ov_type_error)((a), (b))
#define OV_VAL_DIV(a, b) _Generic((a), OV_VROW_INT(div, a, b), OV_VROW_LONG(div, a, b), OV_VROW_FLOAT(div, a, b), OV_VROW_f64(div, a, b), OV_VVEC_ROW(2, div, a, b), OV_VVEC_ROW(3, div, a, b), OV_VVEC_ROW(4, div, a, b), default: ov_type_error)((a), (b))

#define OV_VAL_MUL(a, b) _Generic((a), OV_VROW_INT(mul, a, b), OV_VROW_LONG(mul, a, b), OV_VROW_FLOAT(mul, a, b), OV_VROW_f64(mul, a, b), OV_VVEC_ROW(2, mul, a, b), OV_VVEC_ROW(3, mul, a, b), OV_VVEC_ROW(4, mul, a, b), \
    mat4f: _Generic((b),                                                                                          \
        mat4f: ov_m4_mul,                                                                                         \
        vec4f: ov_m4_transform,                                                                                   \
        default: ov_type_error),                                                                                 \
    default: ov_type_error)((a), (b))

#define OV_DEST_ADD(a, b, d) _Generic((a), OV_DROW_INT(add, a, b, d), OV_DROW_LONG(add, a, b, d), OV_DROW_FLOAT(add, a, b, d), OV_DROW_f64(add, a, b, d), OV_DVEC_ROW(2, add, a, b, d), OV_DVEC_ROW(3, add, a, b, d), OV_DVEC_ROW(4, add, a, b, d), default: ov_type_error)((a), (b), (d))
#define OV_DEST_SUB(a, b, d) _Generic((a), OV_DROW_INT(sub, a, b, d), OV_DROW_LONG(sub, a, b, d), OV_DROW_FLOAT(sub, a, b, d), OV_DROW_f64(sub, a, b, d), OV_DVEC_ROW(2, sub, a, b, d), OV_DVEC_ROW(3, sub, a, b, d), OV_DVEC_ROW(4, sub, a, b, d), default: ov_type_error)((a), (b), (d))
#define OV_DEST_DIV(a, b, d) _Generic((a), OV_DROW_INT(div, a, b, d), OV_DROW_LONG(div, a, b, d), OV_DROW_FLOAT(div, a, b, d), OV_DROW_f64(div, a, b, d), OV_DVEC_ROW(2, div, a, b, d), OV_DVEC_ROW(3, div, a, b, d), OV_DVEC_ROW(4, div, a, b, d), default: ov_type_error)((a), (b), (d))

#define OV_DEST_MUL(a, b, d) _Generic((a), OV_DROW_INT(mul, a, b, d), OV_DROW_LONG(mul, a, b, d), OV_DROW_FLOAT(mul, a, b, d), OV_DROW_f64(mul, a, b, d), OV_DVEC_ROW(2, mul, a, b, d), OV_DVEC_ROW(3, mul, a, b, d), OV_DVEC_ROW(4, mul, a, b, d), \
    mat4f: _Generic((b),                                                                                              \
        mat4f: ov_m4_d_mul,                                                                                           \
        vec4f: ov_m4_d_transform,                                                                                     \
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
