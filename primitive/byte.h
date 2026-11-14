#ifndef PRIMITIVE_BYTE_H
#define PRIMITIVE_BYTE_H

#include "vex.h"

// primitive/byte.h — Byte primitive (Legacy: primitive/Byte.java).
// Delegates to Bit8 width pool (1B stride).

extern BitPool g_bytePool;

bool Byte_init();
fn Byte_shutdown();
any Byte_alloc();
any Byte_allocArray(bytes count);
fn Byte_free(any ptr);
i8 Byte_get(any ptr);
fn Byte_set(any ptr, i8 value);
bool Byte_compareAndSet(any ptr, i8 expected, i8 value);
u64 Byte_type(any ptr);
bytes Byte_length(any ptr);


// Ergonomic constructors — Byte() and Byte(value)
any Byte_allocWithValue(i8 value);
#define Byte_0(...) Byte_alloc()
#define Byte_1(value) Byte_allocWithValue(value)
#define Byte(...) CONSTRUCTOR_DISPATCH(Byte, __VA_ARGS__)


#define Byte_array(count) Byte_allocArray(count)

#endif
