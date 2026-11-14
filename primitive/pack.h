#ifndef PRIMITIVE_PACK_H
#define PRIMITIVE_PACK_H

#include "vex.h"

// primitive/pack.h — bit-packing utilities (Legacy: primitive/Pack.java).

static inline i16 Pack_packByte(i8 b1, i8 b2) { return (i16) ((b1 << 8) | (b2 & 0xFF)); }
static inline i32 Pack_packShort(i16 s1, i16 s2) { return (i32) ((s1 << 16) | (s2 & 0xFFFF)); }
static inline i64 Pack_packInt(i32 i1, i32 i2) { return ((i64) i1 << 32) | (u32) i2; }
static inline i8 Pack_unpackByte1(i16 p) { return (i8) (p >> 8); }
static inline i8 Pack_unpackByte2(i16 p) { return (i8) (p & 0xFF); }
static inline i16 Pack_unpackShort1(i32 p) { return (i16) (p >> 16); }
static inline i16 Pack_unpackShort2(i32 p) { return (i16) (p & 0xFFFF); }
static inline i32 Pack_unpackInt1(i64 p) { return (i32) (p >> 32); }
static inline i32 Pack_unpackInt2(i64 p) { return (i32) (p & 0xFFFFFFFF); }

#endif
