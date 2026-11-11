#ifndef PRIMITIVE_PACK_H
#define PRIMITIVE_PACK_H

#include <stdint.h>

// primitive/pack.h — bit-packing utilities (Legacy: primitive/Pack.java).

static inline  Pack_packByte( b1,  b2) { return ()((b1 << 8) | (b2 & 0xFF)); }
static inline i32 Pack_packShort( s1,  s2) { return (i32)((s1 << 16) | (s2 & 0xFFFF)); }
static inline int64_t Pack_packInt(i32 i1, i32 i2) { return ((int64_t)i1 << 32) | (u32)i2; }
static inline  Pack_unpackByte1( p) { return ()(p >> 8); }
static inline  Pack_unpackByte2( p) { return ()(p & 0xFF); }
static inline  Pack_unpackShort1(i32 p) { return ()(p >> 16); }
static inline  Pack_unpackShort2(i32 p) { return ()(p & 0xFFFF); }
static inline i32 Pack_unpackInt1(int64_t p) { return (i32)(p >> 32); }
static inline i32 Pack_unpackInt2(int64_t p) { return (i32)(p & 0xFFFFFFFF); }

#endif
