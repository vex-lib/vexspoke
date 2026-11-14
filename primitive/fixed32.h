#ifndef PRIMITIVE_FIXED32_H
#define PRIMITIVE_FIXED32_H

#include "vex.h"

// primitive/fixed32.h — Fixed32 primitive (Legacy: primitive/Fixed32.java).
// Delegates to Bit32 width pool (4B stride).

extern BitPool g_fixed32Pool;

bool Fixed32_init();
fn Fixed32_shutdown();
any Fixed32_alloc();
any Fixed32_allocArray(bytes count);
fn Fixed32_free(any ptr);
i32 Fixed32_get(any ptr);
fn Fixed32_set(any ptr, i32 value);
bool Fixed32_compareAndSet(any ptr, i32 expected, i32 value);
u64 Fixed32_type(any ptr);
bytes Fixed32_length(any ptr);


// Ergonomic constructors — Fixed32() and Fixed32(value)
any Fixed32_allocWithValue(i32 value);
#define Fixed32_0(...) Fixed32_alloc()
#define Fixed32_1(value) Fixed32_allocWithValue(value)
#define Fixed32(...) CONSTRUCTOR_DISPATCH(Fixed32, __VA_ARGS__)


#define Fixed32_array(count) Fixed32_allocArray(count)

#endif
