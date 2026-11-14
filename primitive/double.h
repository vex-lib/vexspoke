#ifndef PRIMITIVE_f64_H
#define PRIMITIVE_f64_H

#include "vex.h"

// primitive/d64.h — d64 primitive (Legacy: primitive/d64.java).
// Delegates to Bit64 width pool (8B stride).

extern BitPool g_f64Pool;

bool f64_init();
fn f64_shutdown();
any f64_alloc();
any f64_allocArray(bytes count);
fn f64_free(any ptr);
d64 f64_get(any ptr);
fn f64_set(any ptr, d64 value);
bool f64_compareAndSet(any ptr, d64 expected, d64 value);
u64 f64_type(any ptr);
bytes f64_length(any ptr);


// Ergonomic constructors — d64() and d64(value)
any f64_allocWithValue(d64 value);
#define f64_0(...) f64_alloc()
#define f64_1(value) f64_allocWithValue(value)
#define d64(...) CONSTRUCTOR_DISPATCH(d64, __VA_ARGS__)


#define f64_array(count) f64_allocArray(count)

#endif
