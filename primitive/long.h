#ifndef PRIMITIVE_LONG_H
#define PRIMITIVE_LONG_H

#include "vex.h"
#include "nio/nio.h"

// primitive/i64.h — Long primitive (Legacy: primitive/Long.java).
// Delegates to Bit64 width pool (8B stride).

extern BitPool g_longPool;

bool Long_init();
fn Long_shutdown();
any Long_alloc();
any Long_allocArray(bytes count);
fn Long_free(any ptr);
i64 Long_get(any ptr);
fn Long_set(any ptr, i64 value);
bool Long_compareAndSet(any ptr, i64 expected, i64 value);
u64 Long_type(any ptr);
bytes Long_length(any ptr);


// Ergonomic constructors — Long() and Long(value)
any Long_allocWithValue(i64 value);
#define Long_0(...) Long_alloc()
#define Long_1(value) Long_allocWithValue(value)
#define Long(...) CONSTRUCTOR_DISPATCH(Long, __VA_ARGS__)


#define Long_array(count) Long_allocArray(count)

#endif
